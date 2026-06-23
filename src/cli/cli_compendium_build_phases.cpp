#include "cli_compendium_build_phases.h"

#include "compendium_enrichment.h"
#include "compendium_consolidate_thumbnails.h"
#include "compendium_sql_utilities.h"
#include "../core/compendium_disc_bridge.h"
#include "../core/compendium_manifest_parser.h"
#include "../core/constants/database_schema.h"
#include "../core/constants/settings.h"
#include "../core/constants/system_ids.h"
#include "../metadata/compendium_merge_resolver.h"
#include "../metadata/compendium_hasheous_offline.h"
#include "../metadata/compendium_types.h"
#include "../services/credential_manager.h"

#include <QCommandLineParser>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
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

bool hasScreenScraperCredentials(const QString &credPath) {
    const auto load = [&](const char *key) {
        const QString qkey = QString::fromLatin1(key);
        return credPath.isEmpty() ? CredentialManager::get(qkey) : CredentialManager::get(qkey, credPath);
    };
    using namespace Constants::Settings::Providers;
    return !load(SCREENSCRAPER_USERNAME).isEmpty() && !load(SCREENSCRAPER_PASSWORD).isEmpty()
        && !load(SCREENSCRAPER_DEVID).isEmpty() && !load(SCREENSCRAPER_DEVPASSWORD).isEmpty();
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

bool hasLaunchBoxGaps(QSqlDatabase &db, QString &error) {
    return queryHasRows(db,
        QStringLiteral("SELECT 1 FROM games g "
                       "WHERE (%1) "
                       "  AND EXISTS ("
                       "      SELECT 1 FROM game_signatures gs "
                       "      JOIN source_items si ON si.external_key = gs.source_entry_key "
                       "      WHERE gs.game_id = g.game_id "
                       "        AND json_extract(si.payload_json, '$.rom_name') IS NOT NULL "
                       "        AND TRIM(json_extract(si.payload_json, '$.rom_name')) <> ''"
                       "  ) "
                       "LIMIT 1")
            .arg(QLatin1String(kGeneralMetadataGapSql)),
        error);
}

bool hasRemusThumbnailsAssets(QSqlDatabase &db) {
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type='table' AND name='game_assets' LIMIT 1"))
        || !q.next()) {
        return false;
    }
    QSqlQuery countQ(db);
    if (!countQ.exec(QStringLiteral("SELECT 1 FROM game_assets WHERE asset_type = 'box' LIMIT 1"))) {
        return false;
    }
    return countQ.next();
}

bool hasArtworkGaps(QSqlDatabase &db, QString &error) {
    QSqlQuery tableQ(db);
    if (!tableQ.exec(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type='table' AND name='game_assets' LIMIT 1"))
        || !tableQ.next()) {
        return true;
    }
    return queryHasRows(db,
        QStringLiteral("SELECT 1 FROM games g "
                       "WHERE (g.cover_url IS NULL OR TRIM(g.cover_url) = '' "
                       "       OR g.cover_url LIKE 'https://%' OR g.cover_url LIKE 'http://%') "
                       "  AND EXISTS (SELECT 1 FROM game_assets ga "
                       "              WHERE ga.game_id = g.game_id AND ga.asset_type = 'box') "
                       "LIMIT 1"),
        error);
}

bool hasWikidataGaps(QSqlDatabase &db, QString &error) {
    return queryHasRows(
        db, QStringLiteral("SELECT 1 FROM games WHERE (%1) LIMIT 1").arg(QLatin1String(kGeneralMetadataGapSql)), error);
}

bool hasTheGamesDBGaps(QSqlDatabase &db, QString &error) {
    return hasWikidataGaps(db, error);
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
                       "JOIN game_signatures gs ON gs.game_id = g.game_id "
                       "  AND gs.hash_type IN ('md5', 'sha1', 'crc32', 'sha256') "
                       "WHERE (g.genre IS NULL OR TRIM(g.genre) = '' "
                       "    OR g.developer IS NULL OR TRIM(g.developer) = '' "
                       "    OR g.publisher IS NULL OR TRIM(g.publisher) = '' "
                       "    OR g.release_year IS NULL "
                       "    OR g.release_date IS NULL OR TRIM(g.release_date) = '' "
                       "    OR g.description IS NULL OR TRIM(g.description) = '') "
                       "LIMIT 1"),
        error);
}

bool hasAnyScreenScraperGaps(QSqlDatabase &db, QString &error) {
    return queryHasRows(db,
        QStringLiteral("SELECT 1 FROM games g "
                       "WHERE EXISTS ("
                       "  SELECT 1 FROM game_signatures gs "
                       "  WHERE gs.game_id = g.game_id "
                       "    AND gs.hash_type IN ('md5', 'sha1', 'crc32', 'sha256')) "
                       "  AND (g.genre IS NULL OR TRIM(g.genre) = '' "
                       "    OR g.developer IS NULL OR TRIM(g.developer) = '' "
                       "    OR g.publisher IS NULL OR TRIM(g.publisher) = '' "
                       "    OR g.release_year IS NULL "
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
                       "  AND EXISTS ("
                       "  SELECT 1 FROM game_signatures gs2 "
                       "  JOIN source_items si ON si.source_id = gs2.source_id "
                       "    AND si.external_key = gs2.source_entry_key "
                       "  WHERE gs2.game_id = g.game_id "
                       "    AND json_extract(si.payload_json, '$.size') IS NOT NULL "
                       "    AND CAST(json_extract(si.payload_json, '$.size') AS INTEGER) > 0) "
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
    const QString &openvgdbPath, const QString &mameCatverPath, const QString &mameListXmlPath,
    const QString &launchboxMetadataPath, const QString &credPath, const QStringList &sourceFilter,
    bool offlineOnlyEnrichment, bool onlineEnrichmentAll) {
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
    addLabeledTree("launchbox_metadata", launchboxMetadataPath);
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
    if (!stats.passSkips.isEmpty()) {
        QJsonArray passSkipArray;
        for (const EnrichmentStats::PassSkip &entry : stats.passSkips) {
            passSkipArray.append(QJsonObject {
                { QStringLiteral("source_key"), entry.sourceKey },
                { QStringLiteral("pass_name"), entry.passName },
                { QStringLiteral("skip_reason"), entry.skipReason },
            });
        }
        report.insert(QStringLiteral("enrichment_pass_skips"), passSkipArray);
    } else {
        report.remove(QStringLiteral("enrichment_pass_skips"));
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
    report.insert(QStringLiteral("screenscraper_games_enriched"), stats.screenscraperGamesEnriched);
    report.insert(QStringLiteral("screenscraper_facts_inserted"), stats.screenscraperFactsInserted);
    report.insert(QStringLiteral("screenscraper_api_calls_needed"), stats.screenscraperApiCallsNeeded);
    report.insert(QStringLiteral("screenscraper_api_calls_performed"), stats.screenscraperApiCallsPerformed);
    report.insert(QStringLiteral("wikidata_games_enriched"), stats.wikidataGamesEnriched);
    report.insert(QStringLiteral("wikidata_facts_inserted"), stats.wikidataFactsInserted);
    report.insert(QStringLiteral("launchbox_games_enriched"), stats.launchboxGamesEnriched);
    report.insert(QStringLiteral("launchbox_facts_inserted"), stats.launchboxFactsInserted);
    report.insert(QStringLiteral("remus_thumbnails_games_enriched"), stats.remusThumbnailsGamesEnriched);
    report.insert(QStringLiteral("remus_thumbnails_facts_inserted"), stats.remusThumbnailsFactsInserted);
    report.insert(QStringLiteral("thegamesdb_games_enriched"), stats.thegamesdbGamesEnriched);
    report.insert(QStringLiteral("thegamesdb_facts_inserted"), stats.thegamesdbFactsInserted);
    report.insert(QStringLiteral("post_enrich_fts_rows_indexed"), stats.ftsRowsIndexed);
}

EnrichmentCliOptions resolveEnrichmentCliOptions(const QCommandLineParser &parser, const QStringList &sourceFilter) {
    EnrichmentCliOptions opts;
    opts.onlineEnrichmentAll = parser.isSet(QStringLiteral("online-enrichment-all"));
    opts.offlineOnlyEnrichment = parser.isSet(QStringLiteral("offline-only-enrichment"));
    opts.strictOfflineEnrichment = parser.isSet(QStringLiteral("strict-offline"));
    opts.sourceFilter = sourceFilter;

    if (opts.offlineOnlyEnrichment) {
        qInfo() << "[compendium] Offline-only enrichment (local DAT/metadata/files). "
                << "Omit --offline-only-enrichment to allow online gap-fill when credentials exist.";
    } else if (opts.onlineEnrichmentAll) {
        qInfo() << "[compendium] Full online enrichment (includes per-game Hasheous/PlayMatch/ZXInfo APIs).";
    } else {
        qInfo() << "[compendium] Enrichment: offline sources first, then online gap-fill (IGDB/RA/ScreenScraper/etc.). "
                << "Hasheous uses local dumps unless --online-enrichment-all is set.";
        if (parser.isSet(QStringLiteral("online-enrichment"))) {
            qInfo()
                << "[compendium] Note: --online-enrichment is default; use --offline-only-enrichment to skip online.";
        }
    }
    return opts;
}

bool runCompendiumEnrichmentPasses(QSqlDatabase &db, const QString &metadataDir, const QString &gametdbDir,
    const QString &openvgdbPath, const QString &credPath, const QString &mameCatverPath, const QString &mameListXmlPath,
    const QString &launchboxMetadataPath, EnrichmentStats &stats, QString &error, EnrichmentProgressCallback onProgress,
    QStringList sourceFilter, bool offlineOnlyEnrichment, bool onlineEnrichmentAll) {
    auto runSelfManagedPass = [&](const QString &passName, auto &&passFn) -> bool {
        if (!passFn()) {
            error = QStringLiteral("%1 failed: %2").arg(passName, error);
            return false;
        }
        return true;
    };

    struct EnrichmentPassSpec {
        QString name;
        QString sourceKey; // key used for --enrich-source filtering (e.g. "gametdb", "mame-listxml")
        std::function<bool()> isEnabled;
        std::function<bool()> hasWork;
        std::function<bool()> run;
    };

    const bool hasMetadataDir = !metadataDir.isEmpty();
    const bool hasGametdbDir = !gametdbDir.isEmpty();
    const bool hasOpenvgdbPath = !openvgdbPath.isEmpty() && QFile::exists(openvgdbPath);
    const bool hasIgdbCreds = hasIgdbCredentials(credPath);
    const bool hasRaCreds = hasRaCredentials(credPath);
    const bool hasScreenScraperCreds = hasScreenScraperCredentials(credPath);
    const bool hasMameCatverPath = !mameCatverPath.isEmpty() && QFile::exists(mameCatverPath);
    const bool hasMameListXmlPath = !mameListXmlPath.isEmpty() && QFile::exists(mameListXmlPath);
    const bool hasLaunchBoxMetadataPath = !launchboxMetadataPath.isEmpty() && QFile::exists(launchboxMetadataPath);

    const bool hasHasheousOfflineDumps = Remus::Compendium::hasHasheousOfflineDumpFiles();
    // --online-enrichment: offline dumps only. Per-game Hasheous API requires --online-enrichment-all.
    const bool hasheousOfflineOnly = offlineOnlyEnrichment || !onlineEnrichmentAll;

    const QList<EnrichmentPassSpec> passes {
        {
            QStringLiteral("Libretro metadata enrichment"),
            QStringLiteral("libretro"),
            [&] { return hasMetadataDir; },
            [&] { return hasLibretroMetadataGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromLibretroMetadata(
                    db, metadataDir, stats.metadataGamesEnriched, stats.metadataFactsInserted, error);
            },
        },
        {
            QStringLiteral("Remus thumbnails enrichment"),
            QStringLiteral("remus-thumbnails"),
            [&] { return hasRemusThumbnailsAssets(db); },
            [&] { return hasArtworkGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromRemusThumbnails(
                    db, stats.remusThumbnailsGamesEnriched, stats.remusThumbnailsFactsInserted, error);
            },
        },
        {
            QStringLiteral("GameTDB enrichment"),
            QStringLiteral("gametdb"),
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
            [&] { return hasOpenvgdbPath; },
            [&] { return hasOpenVgdbGaps(db, error, gametdbDir); },
            [&] {
                return CompendiumEnrichment::enrichFromOpenVGDB(
                    db, openvgdbPath, gametdbDir, stats.openvgdbGamesEnriched, stats.openvgdbFactsInserted, error);
            },
        },
        {
            QStringLiteral("LaunchBox enrichment"),
            QStringLiteral("launchbox"),
            [&] { return hasLaunchBoxMetadataPath; },
            [&] { return hasLaunchBoxGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromLaunchBox(
                    db, launchboxMetadataPath, stats.launchboxGamesEnriched, stats.launchboxFactsInserted, error);
            },
        },
        {
            QStringLiteral("MAME catver enrichment"),
            QStringLiteral("mame-catver"),
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
            [&] { return hasMameListXmlPath; },
            [&] { return hasArcadeListxmlGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromMameListXml(
                    db, mameListXmlPath, stats.mameListXmlGamesEnriched, stats.mameListXmlFactsInserted, error);
            },
        },
        {
            QStringLiteral("Hasheous enrichment"),
            QStringLiteral("hasheous"),
            [&] { return hasheousOfflineOnly ? hasHasheousOfflineDumps : true; },
            [&] { return hasAnyHasheousGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromHasheous(db, credPath, stats.hasheousGamesEnriched,
                    stats.hasheousFactsInserted, error, &stats.hasheousApiCallsNeeded, &stats.hasheousApiCallsPerformed,
                    hasheousOfflineOnly);
            },
        },
        {
            QStringLiteral("ScreenScraper enrichment"),
            QStringLiteral("screenscraper"),
            [&] { return hasScreenScraperCreds && !offlineOnlyEnrichment; },
            [&] { return hasAnyScreenScraperGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromScreenScraper(db, credPath, stats.screenscraperGamesEnriched,
                    stats.screenscraperFactsInserted, error, &stats.screenscraperApiCallsNeeded,
                    &stats.screenscraperApiCallsPerformed);
            },
        },
        {
            QStringLiteral("IGDB enrichment"),
            QStringLiteral("igdb"),
            [&] { return hasIgdbCreds && !offlineOnlyEnrichment; },
            [&] { return hasIgdbBulkMetadataGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromIGDB(
                    db, credPath, stats.igdbGamesEnriched, stats.igdbFactsInserted, error);
            },
        },
        {
            QStringLiteral("RetroAchievements enrichment"),
            QStringLiteral("ra"),
            [&] { return hasRaCreds && !offlineOnlyEnrichment; },
            [&] { return hasAnyRaGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromRetroAchievements(db, credPath, stats.raGamesEnriched,
                    stats.raFactsInserted, error, &stats.raApiCallsNeeded, &stats.raApiCallsPerformed,
                    &stats.raApiCallsSuppressed);
            },
        },
        {
            QStringLiteral("TheGamesDB enrichment"),
            QStringLiteral("thegamesdb"),
            [&] { return !offlineOnlyEnrichment; },
            [&] { return hasTheGamesDBGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromTheGamesDB(
                    db, credPath, stats.thegamesdbGamesEnriched, stats.thegamesdbFactsInserted, error);
            },
        },
        {
            QStringLiteral("Wikidata enrichment"),
            QStringLiteral("wikidata"),
            [&] { return !offlineOnlyEnrichment; },
            [&] { return hasWikidataGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromWikidata(
                    db, stats.wikidataGamesEnriched, stats.wikidataFactsInserted, error);
            },
        },
        {
            QStringLiteral("PlayMatch enrichment"),
            QStringLiteral("playmatch"),
            [&] { return !offlineOnlyEnrichment && onlineEnrichmentAll; },
            [&] { return hasAnyPlayMatchGaps(db, error); },
            [&] {
                return CompendiumEnrichment::enrichFromPlayMatch(db, stats.playmatchGamesEnriched,
                    stats.playmatchFactsInserted, error, &stats.playmatchApiCallsNeeded,
                    &stats.playmatchApiCallsPerformed);
            },
        },
        {
            QStringLiteral("ZXInfo enrichment"),
            QStringLiteral("zxinfo"),
            [&] { return !offlineOnlyEnrichment && onlineEnrichmentAll; },
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
            qInfo().noquote() << QStringLiteral("[ENRICH] Pass %1/%2: %3 — skipped (offline-only build; omit "
                                                "--offline-only-enrichment for online gap-fill)")
                                     .arg(passIdx)
                                     .arg(totalPasses)
                                     .arg(pass.name);
            stats.passSkips.append({ pass.sourceKey, pass.name, QStringLiteral("offline_only") });
            ++stats.passesSkippedOfflineOnly;
            continue;
        }
        if (!onlineEnrichmentAll && perGameOnlineEnrichmentSourceKeys().contains(pass.sourceKey)
            && pass.sourceKey != QStringLiteral("hasheous")) {
            qInfo().noquote() << QStringLiteral(
                "[ENRICH] Pass %1/%2: %3 — skipped (per-game API; use --online-enrichment-all)")
                                     .arg(passIdx)
                                     .arg(totalPasses)
                                     .arg(pass.name);
            stats.passSkips.append({ pass.sourceKey, pass.name, QStringLiteral("online_all_required") });
            ++stats.passesSkippedOfflineOnly;
            continue;
        }
        if (!sourceFilter.isEmpty() && !sourceFilter.contains(pass.sourceKey)) {
            qInfo().noquote() << QStringLiteral("[ENRICH] Pass %1/%2: %3 — skipped (source filter)")
                                     .arg(passIdx)
                                     .arg(totalPasses)
                                     .arg(pass.name);
            stats.passSkips.append({ pass.sourceKey, pass.name, QStringLiteral("source_filter") });
            ++stats.passesSkippedFiltered;
            continue;
        }
        if (!pass.isEnabled()) {
            qInfo().noquote() << QStringLiteral("[ENRICH] Pass %1/%2: %3 — skipped (no data source)")
                                     .arg(passIdx)
                                     .arg(totalPasses)
                                     .arg(pass.name);
            stats.passSkips.append({ pass.sourceKey, pass.name, QStringLiteral("no_input") });
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
            stats.passSkips.append({ pass.sourceKey, pass.name, QStringLiteral("no_gaps") });
            ++stats.passesSkippedNoGaps;
            continue;
        }

        const bool ok = runSelfManagedPass(pass.name, pass.run);
        if (!ok) {
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
        if (enrichmentMatches) {
            plan.mode = CompendiumBuildMode::Skip;
        } else {
            plan.mode = CompendiumBuildMode::EnrichmentOnly;
        }
        Q_UNUSED(reportExists);
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
        QStringLiteral("PRAGMA busy_timeout = %1")
            .arg(Remus::Constants::DatabaseSchema::Compendium::BUSY_TIMEOUT_WRITE_MS),
    };
    for (const QString &pragma : buildPragmas) {
        if (!pragmaQuery.exec(pragma)) {
            qWarning() << "[buildCompendium] PRAGMA hint failed (non-fatal):" << pragma
                       << pragmaQuery.lastError().text();
        }
    }
}

bool populateCompendiumCoverageSnapshot(QSqlDatabase &database, QString &error) {
    namespace CompendiumTables = Remus::Constants::DatabaseSchema::Compendium::Tables;
    if (!CompendiumSqlUtilities::compendiumTableExists(database, QString::fromLatin1(CompendiumTables::COVERAGE_STATS))
        || !CompendiumSqlUtilities::compendiumTableExists(
            database, QString::fromLatin1(CompendiumTables::SOURCE_COVERAGE))) {
        error = QStringLiteral("Materialized coverage tables are missing (apply migration 0011)");
        return false;
    }

    const auto scalar = [&](const QString &sql) -> qint64 {
        QSqlQuery q(database);
        if (!q.exec(sql) || !q.next())
            return -1;
        return q.value(0).toLongLong();
    };

    const qint64 totalGames = scalar(QStringLiteral("SELECT COUNT(*) FROM games"));
    const qint64 totalSignatures = scalar(QStringLiteral("SELECT COUNT(*) FROM game_signatures"));
    const qint64 totalSystems = scalar(QStringLiteral("SELECT COUNT(*) FROM systems"));
    const qint64 totalSources = scalar(QStringLiteral("SELECT COUNT(*) FROM sources WHERE enabled = 1"));
    const qint64 shadowedSources = scalar(QStringLiteral(
        "SELECT COUNT(*) FROM ("
        "  SELECT si.source_id FROM source_items si "
        "  JOIN sources s ON s.source_id = si.source_id AND s.enabled = 1 "
        "  GROUP BY si.source_id "
        "  HAVING COUNT(*) > 100 "
        "    AND COALESCE((SELECT COUNT(*) FROM game_signatures gs WHERE gs.source_id = si.source_id), 0) = 0"
        ")"));
    if (totalGames < 0) {
        error = QStringLiteral("Failed to query compendium counts for coverage snapshot");
        return false;
    }

    qint64 discBasedGames = 0;
    qint64 gamesWithDiscSets = 0;
    double discSetCoveragePct = 0.0;
    if (compendiumDiscSetsAvailable(database)) {
        discBasedGames = scalar(QStringLiteral("SELECT COUNT(*) FROM games g "
                                               "JOIN systems s ON s.system_id = g.system_id "
                                               "WHERE s.is_disc_based = 1"));
        gamesWithDiscSets = scalar(QStringLiteral("SELECT COUNT(DISTINCT gds.game_id) "
                                                  "FROM game_disc_sets gds "
                                                  "JOIN games g ON g.game_id = gds.game_id "
                                                  "JOIN systems s ON s.system_id = g.system_id "
                                                  "WHERE s.is_disc_based = 1"));
        if (discBasedGames < 0 || gamesWithDiscSets < 0) {
            error = QStringLiteral("Failed to query disc set coverage for snapshot");
            return false;
        }
        if (discBasedGames > 0)
            discSetCoveragePct = 100.0 * static_cast<double>(gamesWithDiscSets) / static_cast<double>(discBasedGames);
    }

    QString txError;
    if (!CompendiumSqlUtilities::beginImmediateTransaction(database, txError)) {
        error = QStringLiteral("Failed to start coverage snapshot transaction: %1").arg(txError);
        return false;
    }

    {
        QSqlQuery clearQuery(database);
        if (!clearQuery.exec(QStringLiteral("DELETE FROM compendium_source_coverage"))) {
            error = clearQuery.lastError().text();
            database.rollback();
            return false;
        }
    }

    {
        QSqlQuery insertSources(database);
        if (!insertSources.exec(QStringLiteral(
                "INSERT INTO compendium_source_coverage "
                "    (source_id, enabled, priority, source_items, sigs_owned, games_covered, "
                "     coverage_pct, sig_yield_pct, shadowed) "
                "WITH "
                "si AS ( "
                "  SELECT source_id, COUNT(*) AS source_items "
                "  FROM source_items GROUP BY source_id "
                "), "
                "gs_owned AS ( "
                "  SELECT source_id, COUNT(*) AS sigs_owned "
                "  FROM game_signatures GROUP BY source_id "
                "), "
                "games_with_sig AS ( "
                "  SELECT DISTINCT game_id FROM game_signatures "
                "), "
                "gf_covered AS ( "
                "  SELECT gf.source_id, COUNT(DISTINCT gf.game_id) AS games_covered "
                "  FROM game_facts gf "
                "  INNER JOIN games_with_sig gws ON gws.game_id = gf.game_id "
                "  GROUP BY gf.source_id "
                ") "
                "SELECT si.source_id, "
                "       COALESCE(s.enabled, 1), "
                "       COALESCE(s.priority, 0), "
                "       si.source_items, "
                "       COALESCE(gs_owned.sigs_owned, 0), "
                "       COALESCE(gf_covered.games_covered, 0), "
                "       ROUND(COALESCE(gf_covered.games_covered, 0) * 100.0 / si.source_items, 1), "
                "       ROUND(COALESCE(gs_owned.sigs_owned, 0) * 100.0 / si.source_items, 1), "
                "       CASE WHEN si.source_items > 100 AND COALESCE(gs_owned.sigs_owned, 0) = 0 THEN 1 ELSE 0 END "
                "FROM si "
                "LEFT JOIN sources s ON s.source_id = si.source_id "
                "LEFT JOIN gs_owned ON gs_owned.source_id = si.source_id "
                "LEFT JOIN gf_covered ON gf_covered.source_id = si.source_id "
                "WHERE COALESCE(s.enabled, 1) = 1"))) {
            error = insertSources.lastError().text();
            database.rollback();
            return false;
        }
    }

    {
        QSqlQuery upsertStats(database);
        upsertStats.prepare(
            QStringLiteral("INSERT INTO compendium_coverage_stats "
                           "    (id, built_at, total_games, total_signatures, total_systems, active_sources, "
                           "     shadowed_sources, disc_based_games, games_with_disc_sets, disc_set_coverage_pct) "
                           "VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                           "ON CONFLICT(id) DO UPDATE SET "
                           "    built_at = excluded.built_at, "
                           "    total_games = excluded.total_games, "
                           "    total_signatures = excluded.total_signatures, "
                           "    total_systems = excluded.total_systems, "
                           "    active_sources = excluded.active_sources, "
                           "    shadowed_sources = excluded.shadowed_sources, "
                           "    disc_based_games = excluded.disc_based_games, "
                           "    games_with_disc_sets = excluded.games_with_disc_sets, "
                           "    disc_set_coverage_pct = excluded.disc_set_coverage_pct"));
        upsertStats.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        upsertStats.addBindValue(totalGames);
        upsertStats.addBindValue(totalSignatures);
        upsertStats.addBindValue(totalSystems);
        upsertStats.addBindValue(totalSources);
        upsertStats.addBindValue(shadowedSources);
        upsertStats.addBindValue(discBasedGames);
        upsertStats.addBindValue(gamesWithDiscSets);
        upsertStats.addBindValue(discSetCoveragePct);
        if (!upsertStats.exec()) {
            error = upsertStats.lastError().text();
            database.rollback();
            return false;
        }
    }

    if (!database.commit()) {
        error = database.lastError().text();
        return false;
    }

    qInfo() << "[buildCompendium] Coverage snapshot refreshed.";
    return true;
}

void finalizeCompendiumBuildArtifacts(QSqlDatabase &database) {
    QString coverageError;
    if (!populateCompendiumCoverageSnapshot(database, coverageError)) {
        qWarning() << "[buildCompendium] Coverage snapshot refresh skipped:" << coverageError;
    }
    CompendiumSqlUtilities::finalizeCompendiumDatabasePragmas(database);
}

int runCompendiumEnrichmentOnlyRefresh(QSqlDatabase &database, const QString &buildId, const QString &reportPath,
    const QJsonObject &existingReportBase, const QString &enrichmentFingerprint, const QString &metadataDir,
    const QString &gametdbDir, const QString &openvgdbPath, const QString &credPath, const QString &mameCatverPath,
    const QString &mameListXmlPath, const QString &launchboxMetadataPath, const QStringList &sourceFilter,
    EnrichmentProgressCallback onProgress, QJsonObject &reportOut, QString &error, bool offlineOnlyEnrichment,
    bool onlineEnrichmentAll) {
    EnrichmentStats enrichStats;
    if (!runCompendiumEnrichmentPasses(database, metadataDir, gametdbDir, openvgdbPath, credPath, mameCatverPath,
            mameListXmlPath, launchboxMetadataPath, enrichStats, error, onProgress, sourceFilter, offlineOnlyEnrichment,
            onlineEnrichmentAll)) {
        return 1;
    }

    if (!populateCompendiumFtsIndex(database, enrichStats.ftsRowsIndexed, error)) {
        return 1;
    }

    finalizeCompendiumBuildArtifacts(database);

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
