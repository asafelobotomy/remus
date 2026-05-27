#pragma once

#include <QString>
#include <QStringList>
#include <functional>

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
    int gametdbGamesEnriched  = 0;
    int gametdbFactsInserted  = 0;
    int openvgdbGamesEnriched = 0;
    int openvgdbFactsInserted = 0;
    int igdbGamesEnriched     = 0;
    int igdbFactsInserted     = 0;
    int raGamesEnriched        = 0;
    int raFactsInserted        = 0;
    int mameGamesEnriched      = 0;
    int mameFactsInserted      = 0;
    int mameListXmlGamesEnriched = 0;
    int mameListXmlFactsInserted = 0;
    int zxinfoGamesEnriched    = 0;
    int zxinfoFactsInserted    = 0;
    int resolvedFields         = 0;
    int unresolvedConflicts    = 0;
    int passesExecuted           = 0;
    int passesSkippedNoInput     = 0;
    int passesSkippedNoGaps      = 0;
    int passesSkippedFiltered    = 0;  // passes skipped by --enrich-source filter
    int passesFailedWithError    = 0;  // non-fatal pass failures; pipeline continues
    int mergeRuns              = 0;
    int raApiCallsNeeded       = 0;
    int raApiCallsPerformed    = 0;
    int raApiCallsSuppressed   = 0;
    int ftsRowsIndexed         = 0;
};

/**
 * @brief Run all enrichment passes (Libretro metadata, GameTDB, OpenVGDB, IGDB) and
 *        follow with a merge-resolution pass to materialise enriched facts.
 *
 * Each transactional pass runs inside its own transaction. IGDB manages its own
 * per-system transactions internally.  On failure the current transaction is rolled
 * back, @p error is set, and the DB connection remains open for the caller to close.
 *
 * @param db          Open SQLite connection (no active transaction required).
 * @param metadataDir Path to data/metadata/  (empty → Libretro pass skipped).
 * @param gametdbDir  Path to data/gametdb/   (empty → GameTDB pass skipped).
 * @param openvgdbPath Path to openvgdb.sqlite (empty → OpenVGDB pass skipped).
 * @param credPath    Path to enrichment-credentials.json (empty → IGDB skipped).
 * @param stats       [out] Aggregated enrichment and merge-resolution counts.
 * @param error       [out] Human-readable error message on failure.
 * @return true on success, false on error.
 */
bool runCompendiumEnrichmentPasses(QSqlDatabase &db,
                                   const QString &metadataDir,
                                   const QString &gametdbDir,
                                   const QString &openvgdbPath,
                                   const QString &credPath,
                                   const QString &mameCatverPath,
                                   const QString &mameListXmlPath,
                                   EnrichmentStats &stats,
                                   QString &error,
                                   EnrichmentProgressCallback onProgress = nullptr,
                                   QStringList sourceFilter = {});

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
void insertEnrichmentStatsReportFields(QJsonObject &report,
                                       const EnrichmentStats &stats,
                                       const QString &resolvedFieldsKey);
