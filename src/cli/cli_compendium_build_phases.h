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

/**
 * @brief Run both enrichment passes (Libretro metadata + GameTDB) against an open database.
 *
 * Each pass runs inside its own transaction supplied by this function.
 * On failure the transaction is rolled back and @p error is set to a human-readable
 * description; the database connection itself remains open for the caller to close.
 *
 * @param db                   Open SQLite connection (no active transaction required).
 * @param metadataDir          Path to data/metadata/ (may be empty — pass is skipped).
 * @param gametdbDir           Path to data/gametdb/  (may be empty — pass is skipped).
 * @param metadataGamesEnriched [out] Games updated by the Libretro pass.
 * @param metadataFactsInserted [out] Facts inserted by the Libretro pass.
 * @param gametdbGamesEnriched  [out] Games updated by the GameTDB pass.
 * @param gametdbFactsInserted  [out] Facts inserted by the GameTDB pass.
 * @param error                [out] Human-readable error message on failure.
 * @return true on success, false on error.
 */
bool runCompendiumEnrichmentPasses(QSqlDatabase &db,
                                   const QString &metadataDir,
                                   const QString &gametdbDir,
                                   int &metadataGamesEnriched,
                                   int &metadataFactsInserted,
                                   int &gametdbGamesEnriched,
                                   int &gametdbFactsInserted,
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
