#pragma once

#include <QSqlDatabase>
#include <QString>

/**
 * @file compendium_enrichment.h
 * @brief Enrichment passes run after the core DAT ingestion pipeline.
 *
 * Each function opens its own source/snapshot rows and enriches the
 * games/game_facts tables using COALESCE semantics (existing values are
 * never overwritten).  Callers are responsible for wrapping calls in a
 * database transaction.
 */

namespace CompendiumEnrichment {

/**
 * @brief Enrich games with Libretro metadata DAT files.
 *
 * Loads genre/developer/publisher/maxusers/releaseyear DATs from
 * @p metadataDir, then for each game in the DB resolves metadata by
 * CRC32 → serial → name and applies it with COALESCE.
 *
 * @param database     Open SQLite connection (must be in a transaction).
 * @param metadataDir  Path to the data/metadata/ directory tree.
 * @param gamesEnriched [out] Number of game rows actually updated.
 * @param factsInserted [out] Number of new game_facts rows inserted.
 * @param error        [out] Human-readable error message on failure.
 * @return true on success, false on error.
 */
bool enrichFromLibretroMetadata(QSqlDatabase &database,
                                const QString &metadataDir,
                                int &gamesEnriched,
                                int &factsInserted,
                                QString &error);

/**
 * @brief Enrich games with GameTDB XML databases.
 *
 * Loads all *.xml files from @p gametdbDir (wiitdb.xml, dstdb.xml,
 * 3dstdb.xml, wiiutdb.xml, switchtdb.xml, ps3tdb.xml), then for each
 * game in the DB resolves metadata by CRC32 → SHA1 → MD5 hash lookup.
 *
 * @param database     Open SQLite connection (must be in a transaction).
 * @param gametdbDir   Path to the data/gametdb/ directory.
 * @param gamesEnriched [out] Number of game rows actually updated.
 * @param factsInserted [out] Number of new game_facts rows inserted.
 * @param error        [out] Human-readable error message on failure.
 * @return true on success, false on error.
 */
bool enrichFromGameTDB(QSqlDatabase &database,
                       const QString &gametdbDir,
                       int &gamesEnriched,
                       int &factsInserted,
                       QString &error);

} // namespace CompendiumEnrichment
