#include "cli_compendium_build_phases.h"

#include "compendium_enrichment.h"
#include "../core/compendium_manifest_parser.h"
#include "../core/constants/settings.h"
#include "../core/constants/system_ids.h"
#include "../metadata/compendium_merge_resolver.h"
#include "../metadata/compendium_hasheous_offline.h"
#include "../metadata/compendium_types.h"
#include "../services/credential_manager.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QCryptographicHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

#include <functional>

using namespace Remus;

namespace {

bool hasIgdbCredentials(const QString &credPath) {
    const auto load = [&](const char *key) {
        const QString qkey = QString::fromLatin1(key);
        return credPath.isEmpty() ? CredentialManager::get(qkey) : CredentialManager::get(qkey, credPath);
    };
    using namespace Constants::Settings::Providers;
    return !load(IGDB_CLIENT_ID).isEmpty() && !load(IGDB_CLIENT_SECRET).isEmpty();
}

bool hasRaCredentials(const QString &credPath) {
    const auto load = [&](const char *key) {
        const QString qkey = QString::fromLatin1(key);
        return credPath.isEmpty() ? CredentialManager::get(qkey) : CredentialManager::get(qkey, credPath);
    };
    using namespace Constants::Settings::Providers;
    return !load(RETROACHIEVEMENTS_USERNAME).isEmpty() && !load(RETROACHIEVEMENTS_API_KEY).isEmpty();
}

bool queryHasRows(QSqlDatabase &db, const QString &sql, QString &error) {
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        error = q.lastError().text();
        return false;
    }
    return q.next();
}

// Shared metadata gap predicate (genre, developer, publisher, year, date, description, players).
static const char kGeneralMetadataGapSql[] = "genre IS NULL OR TRIM(genre) = '' "
                                             "   OR developer IS NULL OR TRIM(developer) = '' "
                                             "   OR publisher IS NULL OR TRIM(publisher) = '' "
                                             "   OR release_year IS NULL "
                                             "   OR release_date IS NULL OR TRIM(release_date) = '' "
                                             "   OR description IS NULL OR TRIM(description) = '' "
                                             "   OR players_max IS NULL ";

// Libretro matches by CRC32 or serial — only run when hash-linked games still have gaps.
bool hasLibretroMetadataGaps(QSqlDatabase &db, QString &error) {
    return queryHasRows(db,
        QStringLiteral("SELECT 1 FROM games g "
                       "WHERE (%1) "
                       "  AND (EXISTS (SELECT 1 FROM game_signatures gs "
                       "               WHERE gs.game_id = g.game_id AND gs.hash_type = 'crc32') "
                       "       OR EXISTS (SELECT 1 FROM game_serials s WHERE s.game_id = g.game_id)) "
                       "LIMIT 1")
            .arg(QLatin1String(kGeneralMetadataGapSql)),
        error);
}

// GameTDB matches by crc32/sha1/md5 — only run when hash-linked games still have gaps.
bool hasGametdbGaps(QSqlDatabase &db, QString &error) {
    return queryHasRows(db,
        QStringLiteral("SELECT 1 FROM games g "
                       "WHERE (%1) "
                       "  AND EXISTS (SELECT 1 FROM game_signatures gs "
                       "              WHERE gs.game_id = g.game_id "
                       "                AND gs.hash_type IN ('crc32', 'sha1', 'md5')) "
                       "LIMIT 1")
            .arg(QLatin1String(kGeneralMetadataGapSql)),
        error);
}

// Checks metadata gaps IGDB bulk enrichment can fill (excludes rating-only gaps to
// avoid re-downloading entire platform catalogs when only ratings are missing).
bool hasIgdbBulkMetadataGaps(QSqlDatabase &db, QString &error) {
    return queryHasRows(db,
        QStringLiteral("SELECT 1 FROM games "
                       "WHERE (genre IS NULL OR TRIM(genre) = '' "
                       "   OR developer IS NULL OR TRIM(developer) = '' "
                       "   OR publisher IS NULL OR TRIM(publisher) = '' "
                       "   OR release_year IS NULL "
                       "   OR release_date IS NULL OR TRIM(release_date) = '' "
                       "   OR description IS NULL OR TRIM(description) = '' "
                       "   OR players_max IS NULL) "
                       "  AND (igdb_id IS NULL OR TRIM(igdb_id) = '') "
                       "  AND NOT EXISTS (SELECT 1 FROM game_facts gf "
                       "                  WHERE gf.game_id = games.game_id "
                       "                    AND gf.field_name = 'igdb_id') "
                       "LIMIT 1"),
        error);
}

// OpenVGDB does not provide players_max or rating.
static const char kOpenVgdbMetadataGapSql[] = "genre IS NULL OR TRIM(genre) = '' "
                                              "   OR developer IS NULL OR TRIM(developer) = '' "
                                              "   OR publisher IS NULL OR TRIM(publisher) = '' "
                                              "   OR release_year IS NULL "
                                              "   OR release_date IS NULL OR TRIM(release_date) = '' "
                                              "   OR description IS NULL OR TRIM(description) = '' ";

// OpenVGDB matches by crc32/md5 — only run when hash-linked games still have gaps
// on systems not covered by GameTDB XML.
bool hasOpenVgdbGaps(QSqlDatabase &db, QString &error, const QString &gametdbDir) {
    QString excludeSystemsSql;
    const QSet<int> gametdbSystems = CompendiumEnrichment::gametdbCoveredSystemIds(gametdbDir);
    if (!gametdbSystems.isEmpty()) {
        QStringList systemIdLiterals;
        for (const int systemId : gametdbSystems)
            systemIdLiterals.append(QString::number(systemId));
        excludeSystemsSql = QStringLiteral(" AND g.system_id NOT IN (%1)").arg(systemIdLiterals.join(QLatin1Char(',')));
    }

    return queryHasRows(db,
        QStringLiteral("SELECT 1 FROM games g "
                       "WHERE (%1)%2 "
                       "  AND EXISTS (SELECT 1 FROM game_signatures gs "
                       "              WHERE gs.game_id = g.game_id "
                       "                AND gs.hash_type IN ('crc32', 'md5')) "
                       "LIMIT 1")
            .arg(QLatin1String(kOpenVgdbMetadataGapSql), excludeSystemsSql),
        error);
}

// MAME catver only provides genre, and only for arcade machines.
bool hasArcadeCatverGaps(QSqlDatabase &db, QString &error) {
    return queryHasRows(db,
        QStringLiteral("SELECT 1 FROM games "
                       "WHERE system_id = %1 "
                       "  AND (genre IS NULL OR TRIM(genre) = '') "
                       "LIMIT 1")
            .arg(Remus::Constants::Systems::ID_ARCADE),
        error);
}

// MAME listxml provides developer, publisher, release_year, players_max for arcade machines.
// It does not provide genre, description, or rating.
bool hasArcadeListxmlGaps(QSqlDatabase &db, QString &error) {
    return queryHasRows(db,
        QStringLiteral("SELECT 1 FROM games "
                       "WHERE system_id = %1 "
                       "  AND (developer IS NULL OR TRIM(developer) = '' "
                       "    OR publisher IS NULL OR TRIM(publisher) = '' "
                       "    OR release_year IS NULL "
                       "    OR players_max IS NULL) "
                       "LIMIT 1")
            .arg(Remus::Constants::Systems::ID_ARCADE),
        error);
}

// ZXInfo provides genre, developer, publisher, release_year, and description.
// It does not provide players_max or rating.
bool hasZxSpectrumGaps(QSqlDatabase &db, QString &error) {
    return queryHasRows(db,
        QStringLiteral("SELECT 1 FROM games "
                       "WHERE system_id = %1 "
                       "  AND (genre IS NULL OR TRIM(genre) = '' "
                       "    OR developer IS NULL OR TRIM(developer) = '' "
                       "    OR publisher IS NULL OR TRIM(publisher) = '' "
                       "    OR release_year IS NULL "
                       "    OR description IS NULL OR TRIM(description) = '') "
                       "LIMIT 1")
            .arg(Remus::Constants::Systems::ID_ZX_SPECTRUM),
        error);
}

bool hasAnyRaGaps(QSqlDatabase &db, QString &error) {
    // Check for any game that has an MD5 signature but is still missing enrichable
    // metadata.  Intentionally does NOT exclude games that already have ra_game_id:
    // a prior run may have written ra_game_id while the metadata API call failed,
    // leaving genre/developer/publisher/release_year blank.  The enrichment pass
    // is idempotent (INSERT OR IGNORE / COALESCE), so re-running is always safe.
    return queryHasRows(db,
        QStringLiteral("SELECT 1 FROM games g "
                       "JOIN game_signatures gs ON gs.game_id = g.game_id AND gs.hash_type = 'md5' "
                       "WHERE (g.genre IS NULL OR TRIM(g.genre) = '' "
                       "    OR g.developer IS NULL OR TRIM(g.developer) = '' "
                       "    OR g.publisher IS NULL OR TRIM(g.publisher) = '' "
                       "    OR g.release_year IS NULL "
                       "    OR g.release_date IS NULL OR TRIM(g.release_date) = '' "
                       "    OR g.description IS NULL OR TRIM(g.description) = '') "
                       "LIMIT 1"),
        error);
}

bool hasAnyHasheousGaps(QSqlDatabase &db, QString &error) {
    return queryHasRows(db,
        QStringLiteral("SELECT 1 FROM games g "
                       "WHERE EXISTS ("
                       "  SELECT 1 FROM game_signatures gs "
                       "  WHERE gs.game_id = g.game_id "
                       "    AND gs.hash_type IN ('md5', 'sha1', 'crc32', 'sha256')) "
                       "  AND NOT EXISTS ("
                       "  SELECT 1 FROM game_facts gf "
                       "  WHERE gf.game_id = g.game_id "
                       "    AND gf.field_name = 'igdb_id') "
                       "LIMIT 1"),
        error);
}

bool hasAnyPlayMatchGaps(QSqlDatabase &db, QString &error) {
    return queryHasRows(db,
        QStringLiteral("SELECT 1 FROM games g "
                       "WHERE EXISTS ("
                       "  SELECT 1 FROM game_signatures gs "
                       "  WHERE gs.game_id = g.game_id "
                       "    AND gs.hash_type IN ('md5', 'sha1', 'crc32', 'sha256')) "
                       "  AND NOT EXISTS ("
                       "  SELECT 1 FROM game_facts gf "
                       "  WHERE gf.game_id = g.game_id "
                       "    AND gf.field_name = 'igdb_id') "
                       "LIMIT 1"),
        error);
}

} // namespace

static void addTreeToEnrichmentFingerprint(QCryptographicHash &hash, const QString &rootPath) {
    QFileInfo fi(rootPath);
    if (!fi.exists()) {
        hash.addData("missing:");
        hash.addData(QDir::toNativeSeparators(rootPath).toUtf8());
        hash.addData("\n");
        return;
    }
    if (fi.isFile()) {
        const QString digest = fileSha256Hex(rootPath);
        hash.addData(QDir::toNativeSeparators(rootPath).toUtf8());
        hash.addData(":");
        hash.addData(digest.toUtf8());
        hash.addData("\n");
        return;
    }

    QStringList paths;
    QDirIterator it(rootPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        paths.append(it.next());
    }
    paths.sort(Qt::CaseInsensitive);
    for (const QString &path : paths) {
        const QString digest = fileSha256Hex(path);
        hash.addData(QDir::toNativeSeparators(path).toUtf8());
        hash.addData(":");
        hash.addData(digest.toUtf8());
        hash.addData("\n");
    }
}

QString computeEnrichmentInputsFingerprint(const QString &metadataDir, const QString &gametdbDir,
    const QString &openvgdbPath, const QString &mameCatverPath, const QString &mameListXmlPath, const QString &credPath,
    const QStringList &sourceFilter, bool offlineOnlyEnrichment, bool onlineEnrichmentAll) {
    QCryptographicHash hash(QCryptographicHash::Sha256);

    auto addLabeledTree = [&](const char *label, const QString &path) {
        hash.addData(label);
        hash.addData("\n");
        if (path.isEmpty()) {
            hash.addData("empty\n");
        } else {
            addTreeToEnrichmentFingerprint(hash, path);
        }
    };

    addLabeledTree("metadata", metadataDir);
    addLabeledTree("gametdb", gametdbDir);
    addLabeledTree("openvgdb", openvgdbPath);
    addLabeledTree("mame_catver", mameCatverPath);
    addLabeledTree("mame_listxml", mameListXmlPath);
    addLabeledTree("credentials", credPath);
    addLabeledTree("hasheous_dumps", Remus::Compendium::findHasheousDumpDir());

    QStringList sortedFilter = sourceFilter;
    sortedFilter.sort(Qt::CaseInsensitive);
    hash.addData("enrich_source:");
    hash.addData(sortedFilter.join(QLatin1Char(',')).toUtf8());
    hash.addData("\n");
    hash.addData("offline_only:");
    hash.addData(offlineOnlyEnrichment ? "1" : "0");
    hash.addData("\n");
    hash.addData("online_enrichment_all:");
    hash.addData(onlineEnrichmentAll ? "1" : "0");
    hash.addData("\n");

    return QString::fromLatin1(hash.result().toHex());
}

QString enrichmentFingerprintFromBuildNotes(const QString &notes) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(notes.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return { };
    }
    return document.object().value(QStringLiteral("enrichment_inputs_fingerprint")).toString();
}

void insertEnrichmentStatsReportFields(
    QJsonObject &report, const EnrichmentStats &stats, const QString &resolvedFieldsKey) {
    report.insert(QStringLiteral("metadata_games_enriched"), stats.metadataGamesEnriched);
    report.insert(QStringLiteral("metadata_facts_inserted"), stats.metadataFactsInserted);
    report.insert(QStringLiteral("gametdb_games_enriched"), stats.gametdbGamesEnriched);
    report.insert(QStringLiteral("gametdb_facts_inserted"), stats.gametdbFactsInserted);
    report.insert(QStringLiteral("openvgdb_games_enriched"), stats.openvgdbGamesEnriched);
    report.insert(QStringLiteral("openvgdb_facts_inserted"), stats.openvgdbFactsInserted);
    report.insert(QStringLiteral("igdb_games_enriched"), stats.igdbGamesEnriched);
    report.insert(QStringLiteral("igdb_facts_inserted"), stats.igdbFactsInserted);
    report.insert(QStringLiteral("ra_games_enriched"), stats.raGamesEnriched);
    report.insert(QStringLiteral("ra_facts_inserted"), stats.raFactsInserted);
    report.insert(QStringLiteral("mame_games_enriched"), stats.mameGamesEnriched);
    report.insert(QStringLiteral("mame_facts_inserted"), stats.mameFactsInserted);
    report.insert(QStringLiteral("mame_listxml_games_enriched"), stats.mameListXmlGamesEnriched);
    report.insert(QStringLiteral("mame_listxml_facts_inserted"), stats.mameListXmlFactsInserted);
    report.insert(QStringLiteral("zxinfo_games_enriched"), stats.zxinfoGamesEnriched);
    report.insert(QStringLiteral("zxinfo_facts_inserted"), stats.zxinfoFactsInserted);
    report.insert(resolvedFieldsKey, stats.resolvedFields);
    report.insert(QStringLiteral("post_enrich_unresolved_conflicts"), stats.unresolvedConflicts);
    report.insert(QStringLiteral("enrichment_passes_executed"), stats.passesExecuted);
    report.insert(QStringLiteral("enrichment_passes_skipped_no_input"), stats.passesSkippedNoInput);
    report.insert(QStringLiteral("enrichment_passes_skipped_no_gaps"), stats.passesSkippedNoGaps);
    report.insert(QStringLiteral("enrichment_passes_skipped_filtered"), stats.passesSkippedFiltered);
    report.insert(QStringLiteral("enrichment_passes_skipped_offline_only"), stats.passesSkippedOfflineOnly);
    report.insert(QStringLiteral("enrichment_passes_failed_with_error"), stats.passesFailedWithError);
    if (!stats.passErrors.isEmpty()) {
        QJsonArray passErrorArray;
        for (const EnrichmentStats::PassError &entry : stats.passErrors) {
            passErrorArray.append(QJsonObject {
                { QStringLiteral("source_key"), entry.sourceKey },
                { QStringLiteral("pass_name"), entry.passName },
                { QStringLiteral("error"), entry.message },
            });
        }
        report.insert(QStringLiteral("enrichment_pass_errors"), passErrorArray);
    } else {
        report.remove(QStringLiteral("enrichment_pass_errors"));
    }
    report.insert(QStringLiteral("post_enrich_merge_runs"), stats.mergeRuns);
    report.insert(QStringLiteral("ra_api_calls_needed"), stats.raApiCallsNeeded);
    report.insert(QStringLiteral("ra_api_calls_performed"), stats.raApiCallsPerformed);
    report.insert(QStringLiteral("ra_api_calls_suppressed"), stats.raApiCallsSuppressed);
    report.insert(QStringLiteral("hasheous_games_enriched"), stats.hasheousGamesEnriched);
    report.insert(QStringLiteral("hasheous_facts_inserted"), stats.hasheousFactsInserted);
    report.insert(QStringLiteral("hasheous_api_calls_needed"), stats.hasheousApiCallsNeeded);
    report.insert(QStringLiteral("hasheous_api_calls_performed"), stats.hasheousApiCallsPerformed);
    report.insert(QStringLiteral("playmatch_games_enriched"), stats.playmatchGamesEnriched);
    report.insert(QStringLiteral("playmatch_facts_inserted"), stats.playmatchFactsInserted);
    report.insert(QStringLiteral("playmatch_api_calls_needed"), stats.playmatchApiCallsNeeded);
    report.insert(QStringLiteral("playmatch_api_calls_performed"), stats.playmatchApiCallsPerformed);
    report.insert(QStringLiteral("post_enrich_fts_rows_indexed"), stats.ftsRowsIndexed);
}

bool runCompendiumEnrichmentPasses(QSqlDatabase &db, const QString &metadataDir, const QString &gametdbDir,
    const QString &openvgdbPath, const QString &credPath, const QString &mameCatverPath, const QString &mameListXmlPath,
    EnrichmentStats &stats, QString &error, EnrichmentProgressCallback onProgress, QStringList sourceFilter,
    bool offlineOnlyEnrichment, bool onlineEnrichmentAll) {
    auto runTransactionalPass = [&](const QString &passName, auto &&passFn) -> bool {
        if (!db.transaction()) {
            error = QStringLiteral("Failed to start %1 transaction: %2").arg(passName, db.lastError().text());
            return false;
        }
        if (!passFn()) {
            db.rollback();
            error = QStringLiteral("%1 failed: %2").arg(passName, error);
            return false;
        }
        if (!db.commit()) {
            error = QStringLiteral("Failed to commit %1 transaction: %2").arg(passName, db.lastError().text());
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

    enum class TransactionMode {
        CallerWrapped,
        SelfManaged,
    };

    struct EnrichmentPassSpec {
        QString name;
        QString sourceKey; // key used for --enrich-source filtering (e.g. "gametdb", "mame-listxml")
        TransactionMode mode;
        std::function<bool()> isEnabled;
        std::function<bool()> hasWork;
        std::function<bool()> run;
        bool critical = false; // if true, a failure aborts the pipeline; otherwise log and continue
    };

    const bool hasMetadataDir = !metadataDir.isEmpty();
    const bool hasGametdbDir = !gametdbDir.isEmpty();
    const bool hasOpenvgdbPath = !openvgdbPath.isEmpty() && QFile::exists(openvgdbPath);
    const bool hasIgdbCreds = hasIgdbCredentials(credPath);
    const bool hasRaCreds = hasRaCredentials(credPath);
    const bool hasMameCatverPath = !mameCatverPath.isEmpty() && QFile::exists(mameCatverPath);
    const bool hasMameListXmlPath = !mameListXmlPath.isEmpty() && QFile::exists(mameListXmlPath);

    const bool hasHasheousOfflineDumps = Remus::Compendium::hasHasheousOfflineDumpFiles();
    // --online-enrichment: offline dumps only. Per-game Hasheous API requires --online-enrichment-all.
    const bool hasheousOfflineOnly = offlineOnlyEnrichment || !onlineEnrichmentAll;

    const QList<EnrichmentPassSpec> passes {
        {
            QStringLiteral("Libretro metadata enrichment"),
            QStringLiteral("libretro"),
            TransactionMode::SelfManaged,
            [&] { return hasMetadataDir; },
            [&] { return hasLibretroMetadataGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromLibretroMetadata(
                    db, metadataDir, stats.metadataGamesEnriched, stats.metadataFactsInserted, error);
            },
        },
        {
            QStringLiteral("GameTDB enrichment"),
            QStringLiteral("gametdb"),
            TransactionMode::SelfManaged,
            [&] { return hasGametdbDir; },
            [&] { return hasGametdbGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromGameTDB(
                    db, gametdbDir, stats.gametdbGamesEnriched, stats.gametdbFactsInserted, error);
            },
        },
        {
            QStringLiteral("OpenVGDB enrichment"),
            QStringLiteral("openvgdb"),
            TransactionMode::SelfManaged,
            [&] { return hasOpenvgdbPath; },
            [&] { return hasOpenVgdbGaps(db, error, gametdbDir); },
            [&] {
                return CompendiumEnrichment::enrichFromOpenVGDB(
                    db, openvgdbPath, gametdbDir, stats.openvgdbGamesEnriched, stats.openvgdbFactsInserted, error);
            },
        },
        {
            QStringLiteral("Hasheous enrichment"),
            QStringLiteral("hasheous"),
            TransactionMode::SelfManaged,
            [&] { return hasheousOfflineOnly ? hasHasheousOfflineDumps : true; },
            [&] { return hasAnyHasheousGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromHasheous(db, credPath, stats.hasheousGamesEnriched,
                    stats.hasheousFactsInserted, error, &stats.hasheousApiCallsNeeded, &stats.hasheousApiCallsPerformed,
                    hasheousOfflineOnly);
            },
        },
        {
            QStringLiteral("PlayMatch enrichment"),
            QStringLiteral("playmatch"),
            TransactionMode::SelfManaged,
            [] { return true; },
            [&] { return hasAnyPlayMatchGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromPlayMatch(db, stats.playmatchGamesEnriched,
                    stats.playmatchFactsInserted, error, &stats.playmatchApiCallsNeeded,
                    &stats.playmatchApiCallsPerformed);
            },
        },
        {
            QStringLiteral("IGDB enrichment"),
            QStringLiteral("igdb"),
            TransactionMode::SelfManaged,
            [&] { return hasIgdbCreds; },
            [&] { return hasIgdbBulkMetadataGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromIGDB(
                    db, credPath, stats.igdbGamesEnriched, stats.igdbFactsInserted, error);
            },
        },
        {
            QStringLiteral("RetroAchievements enrichment"),
            QStringLiteral("ra"),
            TransactionMode::SelfManaged,
            [&] { return hasRaCreds; },
            [&] { return hasAnyRaGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromRetroAchievements(db, credPath, stats.raGamesEnriched,
                    stats.raFactsInserted, error, &stats.raApiCallsNeeded, &stats.raApiCallsPerformed,
                    &stats.raApiCallsSuppressed);
            },
        },
        {
            QStringLiteral("MAME catver enrichment"),
            QStringLiteral("mame-catver"),
            TransactionMode::SelfManaged,
            [&] { return hasMameCatverPath; },
            [&] { return hasArcadeCatverGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromMameCatver(
                    db, mameCatverPath, stats.mameGamesEnriched, stats.mameFactsInserted, error);
            },
        },
        {
            QStringLiteral("MAME listxml enrichment"),
            QStringLiteral("mame-listxml"),
            TransactionMode::SelfManaged,
            [&] { return hasMameListXmlPath; },
            [&] { return hasArcadeListxmlGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromMameListXml(
                    db, mameListXmlPath, stats.mameListXmlGamesEnriched, stats.mameListXmlFactsInserted, error);
            },
        },
        {
            QStringLiteral("ZXInfo enrichment"),
            QStringLiteral("zxinfo"),
            TransactionMode::SelfManaged,
            [] { return true; },
            [&] { return hasZxSpectrumGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromZXInfo(
                    db, stats.zxinfoGamesEnriched, stats.zxinfoFactsInserted, error);
            },
        },
    };

    int passIdx = 0;
    const int totalPasses = passes.size();
    for (const EnrichmentPassSpec &pass : passes) {
        ++passIdx;
        if (offlineOnlyEnrichment && onlineEnrichmentSourceKeys().contains(pass.sourceKey)
            && pass.sourceKey != QStringLiteral("hasheous")) {
            qInfo().noquote() << QStringLiteral(
                "[ENRICH] Pass %1/%2: %3 — skipped (offline-only build; use --online-enrichment to enable)")
                                     .arg(passIdx)
                                     .arg(totalPasses)
                                     .arg(pass.name);
            ++stats.passesSkippedOfflineOnly;
            continue;
        }
        if (!sourceFilter.isEmpty() && !sourceFilter.contains(pass.sourceKey)) {
            qInfo().noquote() << QStringLiteral("[ENRICH] Pass %1/%2: %3 — skipped (source filter)")
                                     .arg(passIdx)
                                     .arg(totalPasses)
                                     .arg(pass.name);
            ++stats.passesSkippedFiltered;
            continue;
        }
        if (!pass.isEnabled()) {
            qInfo().noquote() << QStringLiteral("[ENRICH] Pass %1/%2: %3 — skipped (no data source)")
                                     .arg(passIdx)
                                     .arg(totalPasses)
                                     .arg(pass.name);
            ++stats.passesSkippedNoInput;
            continue;
        }

        if (onProgress)
            onProgress(passIdx, totalPasses, pass.name);
        qInfo().noquote() << QStringLiteral("[ENRICH] Pass %1/%2: %3 …").arg(passIdx).arg(totalPasses).arg(pass.name);

        error.clear();
        if (!pass.hasWork()) {
            if (!error.isEmpty()) {
                error = QStringLiteral("%1 pre-check failed: %2").arg(pass.name, error);
                return false;
            }
            qInfo().noquote() << QStringLiteral("[ENRICH] Pass %1/%2: %3 — skipped (no gaps to fill)")
                                     .arg(passIdx)
                                     .arg(totalPasses)
                                     .arg(pass.name);
            ++stats.passesSkippedNoGaps;
            continue;
        }

        const bool ok = pass.mode == TransactionMode::CallerWrapped ? runTransactionalPass(pass.name, pass.run)
                                                                    : runSelfManagedPass(pass.name, pass.run);
        if (!ok) {
            if (pass.critical) {
                return false;
            }
            qWarning().noquote() << QStringLiteral("[ENRICH] Pass %1/%2: %3 failed (non-fatal): %4")
                                        .arg(passIdx)
                                        .arg(totalPasses)
                                        .arg(pass.name, error);
            stats.passErrors.append({ pass.sourceKey, pass.name, error });
            error.clear();
            ++stats.passesFailedWithError;
            continue;
        }
        ++stats.passesExecuted;
    }

    if (stats.passesExecuted > 0) {
        // Wrap merge resolution in its own transaction so a failure does not leave
        // the games table stale against already-committed enrichment facts.
        if (!db.transaction()) {
            error = QStringLiteral("Failed to start post-enrichment merge transaction: %1").arg(db.lastError().text());
            return false;
        }
        Remus::Compendium::CompilerStats resolveStats;
        const Remus::Compendium::MergeResolver resolver;
        if (!resolver.resolve(db, resolveStats, error)) {
            db.rollback();
            error = QStringLiteral("Post-enrichment merge resolution failed: %1").arg(error);
            return false;
        }
        if (!db.commit()) {
            error = QStringLiteral("Failed to commit post-enrichment merge transaction: %1").arg(db.lastError().text());
            return false;
        }
        stats.resolvedFields = resolveStats.resolvedFields;
        stats.unresolvedConflicts = resolveStats.unresolvedConflicts;
        stats.mergeRuns = 1;
    }

    return true;
}

bool populateCompendiumFtsIndex(QSqlDatabase &db, int &rowsIndexed, QString &error) {
    rowsIndexed = 0;
    qInfo() << "[buildCompendium] Rebuilding FTS search index (clearing previous content)...";
    if (!db.transaction()) {
        error = QStringLiteral("Could not start FTS transaction: %1").arg(db.lastError().text());
        return false;
    }

    QSqlQuery ftsQ(db);

    // Clear any previously populated FTS content before repopulating. These
    // are contentful FTS5 tables, so a normal DELETE avoids unsupported
    // 'delete-all' warnings from SQLite on rebuilds.
    if (!ftsQ.exec(QStringLiteral("DELETE FROM games_search"))) {
        error = QStringLiteral("games_search clear failed: %1").arg(ftsQ.lastError().text());
        db.rollback();
        return false;
    }
    if (!ftsQ.exec(QStringLiteral("DELETE FROM games_fts"))) {
        error = QStringLiteral("games_fts clear failed: %1").arg(ftsQ.lastError().text());
        db.rollback();
        return false;
    }

    // ── games_search (trigram, columns: title / game_id / system_id / region_code) ──
    const bool ok1 = ftsQ.exec(QStringLiteral("INSERT INTO games_search(title, game_id, system_id, region_code) "
                                              "SELECT canonical_title, game_id, system_id, "
                                              "       COALESCE(primary_region_code, '') FROM games"));
    if (!ok1) {
        error = QStringLiteral("games_search canonical title insert failed: %1").arg(ftsQ.lastError().text());
        db.rollback();
        return false;
    }
    rowsIndexed += ftsQ.numRowsAffected();

    const bool ok2 = ftsQ.exec(QStringLiteral("INSERT INTO games_search(title, game_id, system_id, region_code) "
                                              "SELECT gn.name_text, gn.game_id, g.system_id, "
                                              "       COALESCE(g.primary_region_code, '') "
                                              "FROM game_names gn JOIN games g ON g.game_id = gn.game_id"));
    if (!ok2) {
        error = QStringLiteral("games_search alias insert failed: %1").arg(ftsQ.lastError().text());
        db.rollback();
        return false;
    }
    rowsIndexed += ftsQ.numRowsAffected();

    // ── games_fts (unicode61, columns: game_id / system_id / title_text) ─────────
    // Populated here so the provider does not perform a slow lazy-populate on first
    // open of a pipeline-built DB.
    const bool ok3 = ftsQ.exec(QStringLiteral("INSERT INTO games_fts(game_id, system_id, title_text) "
                                              "SELECT game_id, system_id, canonical_title FROM games "
                                              "UNION ALL "
                                              "SELECT gn.game_id, g.system_id, gn.name_text "
                                              "FROM game_names gn JOIN games g ON gn.game_id = g.game_id"));
    if (!ok3) {
        error = QStringLiteral("games_fts insert failed: %1").arg(ftsQ.lastError().text());
        db.rollback();
        return false;
    }
    rowsIndexed += ftsQ.numRowsAffected();

    if (!db.commit()) {
        error = QStringLiteral("FTS transaction commit failed: %1").arg(db.lastError().text());
        return false;
    }
    if (!ftsQ.exec(QStringLiteral("INSERT INTO games_search(games_search) VALUES('optimize')"))) {
        error = QStringLiteral("games_search optimize failed: %1").arg(ftsQ.lastError().text());
        return false;
    }
    if (!ftsQ.exec(QStringLiteral("INSERT INTO games_fts(games_fts) VALUES('optimize')"))) {
        error = QStringLiteral("games_fts optimize failed: %1").arg(ftsQ.lastError().text());
        return false;
    }
    qInfo() << "[buildCompendium] FTS index rebuilt and optimized.";
    return true;
}

namespace {

QString latestChecksumForSource(QSqlDatabase &db, const QString &sourceId) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT checksum_sha256 FROM source_snapshots "
                             "WHERE source_id = ? AND checksum_sha256 IS NOT NULL AND TRIM(checksum_sha256) != '' "
                             "ORDER BY fetched_at DESC, snapshot_id DESC LIMIT 1"));
    q.addBindValue(sourceId);
    if (!q.exec() || !q.next()) {
        return { };
    }
    return q.value(0).toString().trimmed();
}

bool sourceRowExists(QSqlDatabase &db, const QString &sourceId) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT 1 FROM sources WHERE source_id = ? LIMIT 1"));
    q.addBindValue(sourceId);
    return q.exec() && q.next();
}

QString readStoredEnrichmentFingerprint(QSqlDatabase &db) {
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT notes FROM compendium_builds ORDER BY built_at DESC LIMIT 1")) || !q.next()) {
        return { };
    }
    return enrichmentFingerprintFromBuildNotes(q.value(0).toString());
}

bool databaseHasPopulatedContent(QSqlDatabase &db) {
    QSqlQuery q(db);
    return q.exec(QStringLiteral("SELECT 1 FROM games LIMIT 1")) && q.next();
}

} // namespace

bool planCompendiumBuild(const QString &dbPath, int schemaVersion, const QList<CompendiumSourceDescriptor> &sources,
    const QString &enrichmentFingerprint, bool forceFullRebuild, bool reportExists, CompendiumBuildPlan &plan,
    QString &error) {
    plan = CompendiumBuildPlan { };
    plan.mode = CompendiumBuildMode::Full;

    if (forceFullRebuild || !QFileInfo::exists(dbPath)) {
        for (const CompendiumSourceDescriptor &source : sources) {
            if (source.enabled && source.sourceType == QStringLiteral("dat")) {
                plan.sourcesToIngest.insert(source.sourceId);
            }
        }
        return true;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("compendium-plan"));
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        error = db.lastError().text();
        return false;
    }

    if (!databaseHasPopulatedContent(db)) {
        db.close();
        QSqlDatabase::removeDatabase(QStringLiteral("compendium-plan"));
        for (const CompendiumSourceDescriptor &source : sources) {
            if (source.enabled && source.sourceType == QStringLiteral("dat")) {
                plan.sourcesToIngest.insert(source.sourceId);
            }
        }
        return true;
    }

    QSqlQuery schemaQ(db);
    schemaQ.prepare(QStringLiteral("SELECT 1 FROM compendium_builds WHERE schema_version = ? LIMIT 1"));
    schemaQ.addBindValue(schemaVersion);
    if (!schemaQ.exec() || !schemaQ.next()) {
        db.close();
        QSqlDatabase::removeDatabase(QStringLiteral("compendium-plan"));
        for (const CompendiumSourceDescriptor &source : sources) {
            if (source.enabled && source.sourceType == QStringLiteral("dat")) {
                plan.sourcesToIngest.insert(source.sourceId);
            }
        }
        return true;
    }

    plan.storedEnrichmentFingerprint = readStoredEnrichmentFingerprint(db);

    for (const CompendiumSourceDescriptor &source : sources) {
        if (!source.enabled || source.sourceType != QStringLiteral("dat")) {
            continue;
        }
        if (!sourceRowExists(db, source.sourceId)) {
            plan.sourcesToIngest.insert(source.sourceId);
            continue;
        }
        const QString storedChecksum = latestChecksumForSource(db, source.sourceId);
        const QString manifestChecksum = source.checksumSha256.trimmed();
        if (storedChecksum.isEmpty() || manifestChecksum.compare(storedChecksum, Qt::CaseInsensitive) != 0) {
            plan.sourcesToIngest.insert(source.sourceId);
        }
    }

    db.close();
    QSqlDatabase::removeDatabase(QStringLiteral("compendium-plan"));

    const bool enrichmentMatches
        = !plan.storedEnrichmentFingerprint.isEmpty() && plan.storedEnrichmentFingerprint == enrichmentFingerprint;

    if (plan.sourcesToIngest.isEmpty()) {
        if (enrichmentMatches && reportExists) {
            plan.mode = CompendiumBuildMode::Skip;
        } else if (enrichmentMatches) {
            plan.mode = CompendiumBuildMode::Full;
            for (const CompendiumSourceDescriptor &source : sources) {
                if (source.enabled && source.sourceType == QStringLiteral("dat")) {
                    plan.sourcesToIngest.insert(source.sourceId);
                }
            }
        } else {
            plan.mode = CompendiumBuildMode::EnrichmentOnly;
        }
    } else {
        plan.mode = CompendiumBuildMode::IncrementalIngest;
    }

    return true;
}

bool syncManifestSourcesToDatabase(QSqlDatabase &db, const QList<CompendiumSourceDescriptor> &sources,
    const QSet<QString> &changedSourceIds, const QString &buildId, int schemaVersion,
    const QString &normalizedManifestJson, QString &error) {
    QSqlQuery buildQuery(db);
    buildQuery.prepare(QStringLiteral(
        "INSERT INTO compendium_builds (build_id, schema_version, built_at, source_manifest_json, notes) "
        "VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(build_id) DO UPDATE SET "
        "schema_version = excluded.schema_version, "
        "built_at = excluded.built_at, "
        "source_manifest_json = excluded.source_manifest_json"));
    buildQuery.addBindValue(buildId);
    buildQuery.addBindValue(schemaVersion);
    buildQuery.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    buildQuery.addBindValue(normalizedManifestJson);
    buildQuery.addBindValue(QStringLiteral("Phase 1 bootstrap compiler run"));
    if (!buildQuery.exec()) {
        error = buildQuery.lastError().text();
        return false;
    }

    for (const CompendiumSourceDescriptor &source : sources) {
        QSqlQuery sourceQuery(db);
        sourceQuery.prepare(QStringLiteral("INSERT INTO sources (source_id, display_name, source_type, license_id, "
                                           "license_url, attribution_required, priority, enabled) "
                                           "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
                                           "ON CONFLICT(source_id) DO UPDATE SET "
                                           "display_name = excluded.display_name, "
                                           "source_type = excluded.source_type, "
                                           "license_id = excluded.license_id, "
                                           "license_url = excluded.license_url, "
                                           "attribution_required = excluded.attribution_required, "
                                           "priority = excluded.priority, "
                                           "enabled = excluded.enabled"));
        sourceQuery.addBindValue(source.sourceId);
        sourceQuery.addBindValue(source.displayName);
        sourceQuery.addBindValue(source.sourceType);
        sourceQuery.addBindValue(source.licenseId.isEmpty() ? QVariant() : QVariant(source.licenseId));
        sourceQuery.addBindValue(source.licenseUrl.isEmpty() ? QVariant() : QVariant(source.licenseUrl));
        sourceQuery.addBindValue(source.attributionRequired ? 1 : 0);
        sourceQuery.addBindValue(source.priority);
        sourceQuery.addBindValue(source.enabled ? 1 : 0);
        if (!sourceQuery.exec()) {
            error = sourceQuery.lastError().text();
            return false;
        }

        if (!changedSourceIds.contains(source.sourceId)) {
            continue;
        }

        QSqlQuery snapshotQuery(db);
        snapshotQuery.prepare(QStringLiteral("INSERT INTO source_snapshots (snapshot_id, source_id, snapshot_label, "
                                             "snapshot_ref, fetched_at, checksum_sha256) "
                                             "VALUES (?, ?, ?, ?, ?, ?) "
                                             "ON CONFLICT(snapshot_id) DO UPDATE SET "
                                             "source_id = excluded.source_id, "
                                             "snapshot_label = excluded.snapshot_label, "
                                             "snapshot_ref = excluded.snapshot_ref, "
                                             "fetched_at = excluded.fetched_at, "
                                             "checksum_sha256 = excluded.checksum_sha256"));
        snapshotQuery.addBindValue(source.snapshotId);
        snapshotQuery.addBindValue(source.sourceId);
        snapshotQuery.addBindValue(source.snapshotLabel);
        snapshotQuery.addBindValue(source.snapshotRef.isEmpty() ? QVariant() : QVariant(source.snapshotRef));
        snapshotQuery.addBindValue(source.fetchedAt.isEmpty() ? QVariant() : QVariant(source.fetchedAt));
        snapshotQuery.addBindValue(source.checksumSha256.isEmpty() ? QVariant() : QVariant(source.checksumSha256));
        if (!snapshotQuery.exec()) {
            error = snapshotQuery.lastError().text();
            return false;
        }
    }

    return true;
}

void applyCompendiumBuildPragmas(QSqlDatabase &database) {
    QSqlQuery pragmaQuery(database);
    pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    const QStringList buildPragmas = {
        QStringLiteral("PRAGMA journal_mode = WAL"),
        QStringLiteral("PRAGMA synchronous = OFF"),
        QStringLiteral("PRAGMA temp_store = MEMORY"),
        QStringLiteral("PRAGMA cache_size = -131072"),
        QStringLiteral("PRAGMA mmap_size = 268435456"),
    };
    for (const QString &pragma : buildPragmas) {
        if (!pragmaQuery.exec(pragma)) {
            qWarning() << "[buildCompendium] PRAGMA hint failed (non-fatal):" << pragma
                       << pragmaQuery.lastError().text();
        }
    }
}

int runCompendiumEnrichmentOnlyRefresh(QSqlDatabase &database, const QString &buildId, const QString &reportPath,
    const QJsonObject &existingReportBase, const QString &enrichmentFingerprint, const QString &metadataDir,
    const QString &gametdbDir, const QString &openvgdbPath, const QString &credPath, const QString &mameCatverPath,
    const QString &mameListXmlPath, const QStringList &sourceFilter, EnrichmentProgressCallback onProgress,
    QJsonObject &reportOut, QString &error, bool offlineOnlyEnrichment, bool onlineEnrichmentAll) {
    EnrichmentStats enrichStats;
    if (!runCompendiumEnrichmentPasses(database, metadataDir, gametdbDir, openvgdbPath, credPath, mameCatverPath,
            mameListXmlPath, enrichStats, error, onProgress, sourceFilter, offlineOnlyEnrichment,
            onlineEnrichmentAll)) {
        return 1;
    }

    if (!populateCompendiumFtsIndex(database, enrichStats.ftsRowsIndexed, error)) {
        return 1;
    }

    reportOut = existingReportBase;
    insertEnrichmentStatsReportFields(reportOut, enrichStats, QStringLiteral("post_enrich_resolved_fields"));
    reportOut.insert(QStringLiteral("enrichment_inputs_fingerprint"), enrichmentFingerprint);
    reportOut.insert(QStringLiteral("build_mode"), QStringLiteral("enrichment_only"));

    const QJsonObject notesObj {
        { QStringLiteral("description"), QStringLiteral("Enrichment-only refresh") },
        { QStringLiteral("enrichment_inputs_fingerprint"), enrichmentFingerprint },
    };
    QSqlQuery notesQuery(database);
    notesQuery.prepare(QStringLiteral("UPDATE compendium_builds SET notes = ?, built_at = ? WHERE build_id = ?"));
    notesQuery.addBindValue(QString::fromUtf8(QJsonDocument(notesObj).toJson(QJsonDocument::Compact)));
    notesQuery.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    notesQuery.addBindValue(buildId);
    if (!notesQuery.exec()) {
        qWarning() << "[build-compendium] Failed to persist enrichment fingerprint:" << notesQuery.lastError().text();
    }

    Q_UNUSED(reportPath);
    return 0;
}
