#include "cli_compendium_build_phases.h"

#include "compendium_enrichment.h"
#include "../metadata/compendium_merge_resolver.h"
#include "../metadata/compendium_types.h"

#include <QFile>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

void insertEnrichmentStatsReportFields(QJsonObject &report,
                                       const EnrichmentStats &stats,
                                       const QString &resolvedFieldsKey)
{
    report.insert(QStringLiteral("metadata_games_enriched"),  stats.metadataGamesEnriched);
    report.insert(QStringLiteral("metadata_facts_inserted"),  stats.metadataFactsInserted);
    report.insert(QStringLiteral("gametdb_games_enriched"),   stats.gametdbGamesEnriched);
    report.insert(QStringLiteral("gametdb_facts_inserted"),   stats.gametdbFactsInserted);
    report.insert(QStringLiteral("openvgdb_games_enriched"),  stats.openvgdbGamesEnriched);
    report.insert(QStringLiteral("openvgdb_facts_inserted"),  stats.openvgdbFactsInserted);
    report.insert(QStringLiteral("igdb_games_enriched"),      stats.igdbGamesEnriched);
    report.insert(QStringLiteral("igdb_facts_inserted"),      stats.igdbFactsInserted);
    report.insert(QStringLiteral("ra_games_enriched"),        stats.raGamesEnriched);
    report.insert(QStringLiteral("ra_facts_inserted"),        stats.raFactsInserted);
    report.insert(QStringLiteral("mame_games_enriched"),      stats.mameGamesEnriched);
    report.insert(QStringLiteral("mame_facts_inserted"),      stats.mameFactsInserted);
    report.insert(QStringLiteral("zxinfo_games_enriched"),    stats.zxinfoGamesEnriched);
    report.insert(QStringLiteral("zxinfo_facts_inserted"),    stats.zxinfoFactsInserted);
    report.insert(resolvedFieldsKey,                           stats.resolvedFields);
}

bool runCompendiumEnrichmentPasses(QSqlDatabase &db,
                                   const QString &metadataDir,
                                   const QString &gametdbDir,
                                   const QString &openvgdbPath,
                                   const QString &credPath,
                                   const QString &mameCatverPath,
                                   EnrichmentStats &stats,
                                   QString &error)
{
    auto runTransactionalPass = [&](const QString &passName, auto &&passFn) -> bool {
        if (!db.transaction()) {
            error = QStringLiteral("Failed to start %1 transaction: %2")
                        .arg(passName, db.lastError().text());
            return false;
        }
        if (!passFn()) {
            db.rollback();
            error = QStringLiteral("%1 failed: %2").arg(passName, error);
            return false;
        }
        if (!db.commit()) {
            error = QStringLiteral("Failed to commit %1 transaction: %2")
                        .arg(passName, db.lastError().text());
            return false;
        }
        return true;
    };

    auto runSelfManagedPass = [&](const QString &passName, auto &&passFn) -> bool {
        if (!passFn()) {
            error = QStringLiteral("%1 failed: %2").arg(passName, error);
            return false;
        }
        return true;
    };

    // ── Enrichment pass 1: Libretro metadata DATs ─────────────────────────────
    if (!metadataDir.isEmpty()) {
        if (!runTransactionalPass(QStringLiteral("Libretro metadata enrichment"), [&] {
                return CompendiumEnrichment::enrichFromLibretroMetadata(db,
                                                                        metadataDir,
                                                                        stats.metadataGamesEnriched,
                                                                        stats.metadataFactsInserted,
                                                                        error);
            })) {
            return false;
        }
    }

    // ── Enrichment pass 2: GameTDB XML databases ───────────────────────────────
    if (!gametdbDir.isEmpty()) {
        if (!runTransactionalPass(QStringLiteral("GameTDB enrichment"), [&] {
                return CompendiumEnrichment::enrichFromGameTDB(db,
                                                               gametdbDir,
                                                               stats.gametdbGamesEnriched,
                                                               stats.gametdbFactsInserted,
                                                               error);
            })) {
            return false;
        }
    }

    // ── Enrichment pass 3: OpenVGDB ───────────────────────────────────────────
    if (!openvgdbPath.isEmpty() && QFile::exists(openvgdbPath)) {
        if (!runTransactionalPass(QStringLiteral("OpenVGDB enrichment"), [&] {
                return CompendiumEnrichment::enrichFromOpenVGDB(db,
                                                                openvgdbPath,
                                                                stats.openvgdbGamesEnriched,
                                                                stats.openvgdbFactsInserted,
                                                                error);
            })) {
            return false;
        }
    }

    // ── Enrichment pass 4: IGDB (manages its own per-system transactions) ─────
    if (!credPath.isEmpty() && QFile::exists(credPath)) {
        if (!runSelfManagedPass(QStringLiteral("IGDB enrichment"), [&] {
                return CompendiumEnrichment::enrichFromIGDB(db,
                                                            credPath,
                                                            stats.igdbGamesEnriched,
                                                            stats.igdbFactsInserted,
                                                            error);
            })) {
            return false;
        }
    }

    // ── Enrichment pass 5: RetroAchievements (manages its own per-system transactions) ──
    if (!credPath.isEmpty() && QFile::exists(credPath)) {
        if (!runSelfManagedPass(QStringLiteral("RetroAchievements enrichment"), [&] {
                return CompendiumEnrichment::enrichFromRetroAchievements(db,
                                                                         credPath,
                                                                         stats.raGamesEnriched,
                                                                         stats.raFactsInserted,
                                                                         error);
            })) {
            return false;
        }
    }

    // ── Enrichment pass 6: MAME catver.ini (caller-wrapped transaction) ────────
    if (!mameCatverPath.isEmpty() && QFile::exists(mameCatverPath)) {
        if (!runTransactionalPass(QStringLiteral("MAME catver enrichment"), [&] {
                return CompendiumEnrichment::enrichFromMameCatver(db,
                                                                  mameCatverPath,
                                                                  stats.mameGamesEnriched,
                                                                  stats.mameFactsInserted,
                                                                  error);
            })) {
            return false;
        }
    }

    // ── Enrichment pass 7: ZXInfo (manages its own transaction) ──────────────
    if (!runSelfManagedPass(QStringLiteral("ZXInfo enrichment"), [&] {
            return CompendiumEnrichment::enrichFromZXInfo(db,
                                                          stats.zxinfoGamesEnriched,
                                                          stats.zxinfoFactsInserted,
                                                          error);
        })) {
        return false;
    }

    // ── Post-enrichment: re-run merge resolution to pick up newly-written facts ─
    {
        Remus::Compendium::CompilerStats resolveStats;
        const Remus::Compendium::MergeResolver resolver;
        if (!resolver.resolve(db, resolveStats, error)) {
            error = QStringLiteral("Post-enrichment merge resolution failed: %1").arg(error);
            return false;
        }
        stats.resolvedFields = resolveStats.resolvedFields;
    }

    return true;
}

void populateCompendiumFtsIndex(QSqlDatabase &db)
{
    qInfo() << "[buildCompendium] Rebuilding FTS search index (clearing previous content)...";
    if (!db.transaction()) {
        qWarning() << "[buildCompendium] Could not start FTS transaction (non-fatal)";
        return;
    }

    QSqlQuery ftsQ(db);

    // Clear any previously populated FTS content before repopulating. These
    // are contentful FTS5 tables, so a normal DELETE avoids unsupported
    // 'delete-all' warnings from SQLite on rebuilds.
    if (!ftsQ.exec(QStringLiteral("DELETE FROM games_search"))) {
        qWarning() << "[buildCompendium] games_search clear failed (non-fatal):"
                   << ftsQ.lastError().text();
    }
    if (!ftsQ.exec(QStringLiteral("DELETE FROM games_fts"))) {
        qWarning() << "[buildCompendium] games_fts clear failed (non-fatal):"
                   << ftsQ.lastError().text();
    }

    // ── games_search (trigram, columns: title / game_id / system_id / region_code) ──
    const bool ok1 = ftsQ.exec(QStringLiteral(
        "INSERT INTO games_search(title, game_id, system_id, region_code) "
        "SELECT canonical_title, game_id, system_id, "
        "       COALESCE(primary_region_code, '') FROM games"));
    if (!ok1) {
        qWarning() << "[buildCompendium] games_search canonical title insert failed (non-fatal):"
                   << ftsQ.lastError().text();
        db.rollback();
        return;
    }

    const bool ok2 = ftsQ.exec(QStringLiteral(
        "INSERT INTO games_search(title, game_id, system_id, region_code) "
        "SELECT gn.name_text, gn.game_id, g.system_id, "
        "       COALESCE(g.primary_region_code, '') "
        "FROM game_names gn JOIN games g ON g.game_id = gn.game_id"));
    if (!ok2) {
        qWarning() << "[buildCompendium] games_search alias insert failed (non-fatal):"
                   << ftsQ.lastError().text();
        db.rollback();
        return;
    }

    // ── games_fts (unicode61, columns: game_id / system_id / title_text) ─────────
    // Populated here so the provider does not perform a slow lazy-populate on first
    // open of a pipeline-built DB.
    const bool ok3 = ftsQ.exec(QStringLiteral(
        "INSERT INTO games_fts(game_id, system_id, title_text) "
        "SELECT game_id, system_id, canonical_title FROM games "
        "UNION ALL "
        "SELECT gn.game_id, g.system_id, gn.name_text "
        "FROM game_names gn JOIN games g ON gn.game_id = g.game_id"));
    if (!ok3) {
        qWarning() << "[buildCompendium] games_fts insert failed (non-fatal):"
                   << ftsQ.lastError().text();
        db.rollback();
        return;
    }

    db.commit();
    ftsQ.exec(QStringLiteral("INSERT INTO games_search(games_search) VALUES('optimize')"));
    ftsQ.exec(QStringLiteral("INSERT INTO games_fts(games_fts) VALUES('optimize')"));
    qInfo() << "[buildCompendium] FTS index rebuilt and optimized.";
}
