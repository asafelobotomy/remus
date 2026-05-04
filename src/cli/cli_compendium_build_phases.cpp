#include "cli_compendium_build_phases.h"

#include "compendium_enrichment.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

bool runCompendiumEnrichmentPasses(QSqlDatabase &db,
                                   const QString &metadataDir,
                                   const QString &gametdbDir,
                                   int &metadataGamesEnriched,
                                   int &metadataFactsInserted,
                                   int &gametdbGamesEnriched,
                                   int &gametdbFactsInserted,
                                   QString &error)
{
    // ── Enrichment pass 1: Libretro metadata DATs ─────────────────────────────
    if (!metadataDir.isEmpty()) {
        if (!db.transaction()) {
            error = QStringLiteral("Failed to start libretro enrichment transaction: %1")
                        .arg(db.lastError().text());
            return false;
        }
        if (!CompendiumEnrichment::enrichFromLibretroMetadata(db,
                                                              metadataDir,
                                                              metadataGamesEnriched,
                                                              metadataFactsInserted,
                                                              error)) {
            db.rollback();
            error = QStringLiteral("Libretro metadata enrichment failed: %1").arg(error);
            return false;
        }
        if (!db.commit()) {
            error = QStringLiteral("Failed to commit libretro enrichment transaction: %1")
                        .arg(db.lastError().text());
            return false;
        }
    }

    // ── Enrichment pass 2: GameTDB XML databases ───────────────────────────────
    if (!gametdbDir.isEmpty()) {
        if (!db.transaction()) {
            error = QStringLiteral("Failed to start GameTDB enrichment transaction: %1")
                        .arg(db.lastError().text());
            return false;
        }
        if (!CompendiumEnrichment::enrichFromGameTDB(db,
                                                     gametdbDir,
                                                     gametdbGamesEnriched,
                                                     gametdbFactsInserted,
                                                     error)) {
            db.rollback();
            error = QStringLiteral("GameTDB enrichment failed: %1").arg(error);
            return false;
        }
        if (!db.commit()) {
            error = QStringLiteral("Failed to commit GameTDB enrichment transaction: %1")
                        .arg(db.lastError().text());
            return false;
        }
    }

    return true;
}

void populateCompendiumFtsIndex(QSqlDatabase &db)
{
    qInfo() << "[buildCompendium] Populating FTS search index...";
    if (!db.transaction()) {
        qWarning() << "[buildCompendium] Could not start FTS transaction (non-fatal)";
        return;
    }

    QSqlQuery ftsQ(db);
    const bool ok1 = ftsQ.exec(QStringLiteral(
        "INSERT INTO games_search(title, game_id, system_id, region_code) "
        "SELECT canonical_title, game_id, system_id, "
        "       COALESCE(primary_region_code, '') FROM games"));
    if (!ok1) {
        qWarning() << "[buildCompendium] FTS canonical title insert failed (non-fatal):"
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
        qWarning() << "[buildCompendium] FTS alias insert failed (non-fatal):"
                   << ftsQ.lastError().text();
        db.rollback();
        return;
    }

    db.commit();
    ftsQ.exec(QStringLiteral("INSERT INTO games_search(games_search) VALUES('optimize')"));
    qInfo() << "[buildCompendium] FTS index populated and optimized.";
}
