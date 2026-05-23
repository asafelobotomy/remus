#pragma once

#include <QString>

class QSqlDatabase;

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
    int raGamesEnriched       = 0;
    int raFactsInserted       = 0;
    int resolvedFields        = 0;
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
                                   EnrichmentStats &stats,
                                   QString &error);

/**
 * @brief Populate the FTS search index tables from the games/game_names rows.
 *
 * Non-fatal: any failure is logged as a warning only. The function manages its
 * own transaction internally.
 *
 * @param db Open SQLite connection (no active transaction required).
 */
void populateCompendiumFtsIndex(QSqlDatabase &db);
