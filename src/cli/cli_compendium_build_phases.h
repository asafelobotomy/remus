#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>

#include "../core/compendium_manifest_parser.h"

class QJsonArray;
class QJsonObject;

class QSqlDatabase;
class QJsonObject;

// Progress callback fired before each enrichment pass begins.
// passIdx:    1-based index of the pass about to run.
// totalPasses: total number of configured passes (including skipped ones).
// passName:   human-readable name of the pass.
using EnrichmentProgressCallback = std::function<void(int passIdx, int totalPasses, const QString &passName)>;

/**
 * @brief Post-build enrichment and FTS index population phases for --build-compendium.
 *
 * Extracted from handleBuildCompendiumCommand to keep that function under the
 * 400-line threshold. Callers manage the database connection lifecycle and error
 * reporting to the user.
 */

// Aggregated counts returned by runCompendiumEnrichmentPasses.
struct EnrichmentStats {
    int metadataGamesEnriched = 0;
    int metadataFactsInserted = 0;
    int gametdbGamesEnriched = 0;
    int gametdbFactsInserted = 0;
    int openvgdbGamesEnriched = 0;
    int openvgdbFactsInserted = 0;
    int igdbGamesEnriched = 0;
    int igdbFactsInserted = 0;
    int raGamesEnriched = 0;
    int raFactsInserted = 0;
    int mameGamesEnriched = 0;
    int mameFactsInserted = 0;
    int mameListXmlGamesEnriched = 0;
    int mameListXmlFactsInserted = 0;
    int zxinfoGamesEnriched = 0;
    int zxinfoFactsInserted = 0;
    int resolvedFields = 0;
    int unresolvedConflicts = 0;
    int passesExecuted = 0;
    int passesSkippedNoInput = 0;
    int passesSkippedNoGaps = 0;
    int passesSkippedFiltered = 0; // passes skipped by --enrich-source filter
    int passesSkippedOfflineOnly = 0; // online passes skipped in offline-only build mode
    int passesFailedWithError = 0; // non-fatal pass failures; pipeline continues
    struct PassError {
        QString sourceKey;
        QString passName;
        QString message;
    };
    QList<PassError> passErrors;
    int mergeRuns = 0;
    int raApiCallsNeeded = 0;
    int raApiCallsPerformed = 0;
    int raApiCallsSuppressed = 0;
    int hasheousGamesEnriched = 0;
    int hasheousFactsInserted = 0;
    int hasheousApiCallsNeeded = 0;
    int hasheousApiCallsPerformed = 0;
    int playmatchGamesEnriched = 0;
    int playmatchFactsInserted = 0;
    int playmatchApiCallsNeeded = 0;
    int playmatchApiCallsPerformed = 0;
    int ftsRowsIndexed = 0;
};

/**
 * @brief Run all enrichment passes (Libretro metadata, GameTDB, OpenVGDB, IGDB,
 *        RetroAchievements, MAME catver, MAME listxml, ZXInfo) and follow with a
 *        merge-resolution pass to materialise enriched facts.
 *
 * Each transactional pass runs inside its own transaction. IGDB and RA manage their
 * own per-system transactions internally.  On failure the current transaction is rolled
 * back, @p error is set, and the DB connection remains open for the caller to close.
 *
 * @param db             Open SQLite connection (no active transaction required).
 * @param metadataDir    Path to data/metadata/     (empty → Libretro pass skipped).
 * @param gametdbDir     Path to data/gametdb/      (empty → GameTDB pass skipped).
 * @param openvgdbPath   Path to openvgdb.sqlite     (empty → OpenVGDB pass skipped).
 * @param credPath       Path to enrichment-credentials.json (empty → IGDB and RA skipped).
 * @param mameCatverPath Path to data/mame/catver.ini (empty → MAME catver skipped).
 * @param mameListXmlPath Path to data/mame/listxml.xml (empty → MAME listxml skipped).
 * @param stats          [out] Aggregated enrichment and merge-resolution counts.
 * @param error          [out] Human-readable error message on failure.
 * @param onProgress     Optional callback fired before each pass begins.
 * @param sourceFilter   Optional list of source keys; only matching passes run.
 *                       Empty list (default) means run all passes.
 *                       Valid keys: see knownEnrichmentSourceKeys().
 * @param offlineOnlyEnrichment When true, skip online API passes (Hasheous API, PlayMatch,
 *                       ZXInfo, IGDB, RA). Hasheous runs only against local dump JSON when present.
 * @param onlineEnrichmentAll When true, Hasheous/PlayMatch/ZXInfo may call per-game APIs.
 *                       When false with bulk online enrichment, Hasheous uses offline dumps only.
 * @return true on success, false on error.
 */
bool runCompendiumEnrichmentPasses(QSqlDatabase &db, const QString &metadataDir, const QString &gametdbDir,
    const QString &openvgdbPath, const QString &credPath, const QString &mameCatverPath, const QString &mameListXmlPath,
    EnrichmentStats &stats, QString &error, EnrichmentProgressCallback onProgress = nullptr,
    QStringList sourceFilter = { }, bool offlineOnlyEnrichment = false, bool onlineEnrichmentAll = false);

/**
 * @brief Source keys that require live network access during enrichment.
 */
inline QStringList onlineEnrichmentSourceKeys() {
    return {
        QStringLiteral("hasheous"),
        QStringLiteral("playmatch"),
        QStringLiteral("zxinfo"),
        QStringLiteral("igdb"),
        QStringLiteral("ra"),
    };
}

/**
 * @brief Source keys that use only local files or SQLite databases.
 */
inline QStringList offlineEnrichmentSourceKeys() {
    return {
        QStringLiteral("libretro"),
        QStringLiteral("gametdb"),
        QStringLiteral("openvgdb"),
        QStringLiteral("mame-catver"),
        QStringLiteral("mame-listxml"),
    };
}

/**
 * @brief Recommended online passes for full builds: bulk platform/system fetches (IGDB, RA).
 * Excludes per-game bridge APIs (Hasheous, PlayMatch) and ZXInfo.
 */
inline QStringList defaultBulkOnlineEnrichmentSourceKeys() {
    QStringList keys = offlineEnrichmentSourceKeys();
    keys.append({
        QStringLiteral("hasheous"), // offline dump JSON only unless --online-enrichment-all
        QStringLiteral("igdb"),
        QStringLiteral("ra"),
    });
    return keys;
}

/**
 * @brief Per-game online bridge passes — very slow on full catalogues (100k+ API calls).
 */
inline QStringList perGameOnlineEnrichmentSourceKeys() {
    return {
        QStringLiteral("hasheous"),
        QStringLiteral("playmatch"),
        QStringLiteral("zxinfo"),
    };
}

/**
 * @brief Populate the FTS search index tables from the games/game_names rows.
 *
 * Failures are fatal for the current command and surfaced to the caller.
 * The function manages its own transaction internally.
 *
 * @param db Open SQLite connection (no active transaction required).
 * @param rowsIndexed [out] Number of rows inserted into FTS tables.
 * @param error [out] Human-readable error on failure.
 * @return true on success, false on failure.
 */
bool populateCompendiumFtsIndex(QSqlDatabase &db, int &rowsIndexed, QString &error);

/**
 * @brief Insert enrichment stat fields into a report JSON object.
 *
 * @param report Report JSON object to mutate.
 * @param stats  Enrichment stats source.
 * @param resolvedFieldsKey JSON key name to use for stats.resolvedFields.
 */
void insertEnrichmentStatsReportFields(
    QJsonObject &report, const EnrichmentStats &stats, const QString &resolvedFieldsKey);

/**
 * @brief SHA-256 fingerprint of local enrichment inputs (metadata trees, GameTDB,
 *        OpenVGDB, MAME payloads, credentials, and optional --enrich-source filter).
 *
 * Stored in compendium_builds.notes and compared on rebuild skip so enrichment-only
 * input changes invalidate a manifest-identical cached DB.
 */
QString computeEnrichmentInputsFingerprint(const QString &metadataDir, const QString &gametdbDir,
    const QString &openvgdbPath, const QString &mameCatverPath, const QString &mameListXmlPath, const QString &credPath,
    const QStringList &sourceFilter = { }, bool offlineOnlyEnrichment = false, bool onlineEnrichmentAll = false);

/**
 * @brief Extract a stored enrichment fingerprint from compendium_builds.notes JSON.
 */
QString enrichmentFingerprintFromBuildNotes(const QString &notes);

/**
 * @brief Returns the canonical set of source key strings accepted by the
 *        @c --enrich-source CLI option and the @p sourceFilter parameter of
 *        runCompendiumEnrichmentPasses().
 *
 * This list is the single source of truth: it must stay in sync with the
 * @c sourceKey field of each EnrichmentPassSpec in cli_compendium_build_phases.cpp.
 * Tests and CLI option-validation code should use this function rather than
 * hard-coding the key strings.
 */
inline QStringList knownEnrichmentSourceKeys() {
    return {
        QStringLiteral("libretro"),
        QStringLiteral("gametdb"),
        QStringLiteral("openvgdb"),
        QStringLiteral("igdb"),
        QStringLiteral("ra"),
        QStringLiteral("hasheous"),
        QStringLiteral("playmatch"),
        QStringLiteral("mame-catver"),
        QStringLiteral("mame-listxml"),
        QStringLiteral("zxinfo"),
    };
}

enum class CompendiumBuildMode {
    Skip,
    EnrichmentOnly,
    IncrementalIngest,
    Full,
};

struct CompendiumBuildPlan {
    CompendiumBuildMode mode = CompendiumBuildMode::Full;
    QSet<QString> sourcesToIngest;
    QString storedEnrichmentFingerprint;
};

/**
 * @brief Decide whether to skip, enrich-only refresh, incrementally ingest, or fully rebuild.
 *
 * Compares per-source @c checksum_sha256 values against @c source_snapshots rather than
 * requiring an exact manifest JSON match (daily @c build_id churn is ignored).
 */
bool planCompendiumBuild(const QString &dbPath, int schemaVersion,
    const QList<Remus::CompendiumSourceDescriptor> &sources, const QString &enrichmentFingerprint,
    bool forceFullRebuild, bool reportExists, CompendiumBuildPlan &plan, QString &error);

/// Upsert manifest source metadata and insert snapshot rows for changed sources.
bool syncManifestSourcesToDatabase(QSqlDatabase &db, const QList<Remus::CompendiumSourceDescriptor> &sources,
    const QSet<QString> &changedSourceIds, const QString &buildId, int schemaVersion,
    const QString &normalizedManifestJson, QString &error);

/// Apply build pragmas used during compendium builds.
void applyCompendiumBuildPragmas(QSqlDatabase &database);

/// Run enrichment + FTS on an existing DB, update build notes/report fingerprint.
int runCompendiumEnrichmentOnlyRefresh(QSqlDatabase &database, const QString &buildId, const QString &reportPath,
    const QJsonObject &existingReportBase, const QString &enrichmentFingerprint, const QString &metadataDir,
    const QString &gametdbDir, const QString &openvgdbPath, const QString &credPath, const QString &mameCatverPath,
    const QString &mameListXmlPath, const QStringList &sourceFilter, EnrichmentProgressCallback onProgress,
    QJsonObject &reportOut, QString &error, bool offlineOnlyEnrichment = false, bool onlineEnrichmentAll = false);
