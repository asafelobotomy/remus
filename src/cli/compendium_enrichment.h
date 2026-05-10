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

/**
 * @brief Enrich games with the OpenVGDB SQLite database.
 *
 * Matches compendium games by CRC32 hash against the OpenVGDB ROM index and
 * fills in description, genre, developer, publisher, and release year using
 * COALESCE semantics (existing values are never overwritten).
 *
 * @param database      Open SQLite connection (must be in a transaction).
 * @param openvgdbPath  Path to openvgdb.sqlite (skipped if file absent).
 * @param gamesEnriched [out] Number of game rows actually updated.
 * @param factsInserted [out] Number of new game_facts rows inserted.
 * @param error         [out] Human-readable error message on failure.
 * @return true on success, false on error.
 */
bool enrichFromOpenVGDB(QSqlDatabase &database,
                        const QString &openvgdbPath,
                        int &gamesEnriched,
                        int &factsInserted,
                        QString &error);

/**
 * @brief Enrich games using the IGDB online API (bulk platform-slug fetch).
 *
 * Authenticates with IGDB, then for each system that has games missing
 * descriptions bulk-fetches all IGDB entries for that platform slug and
 * matches by normalised title (lowercase, strip articles, keep alnum).
 * Fills in description, genre, developer, publisher, and release year
 * with COALESCE semantics (existing values are never overwritten).
 *
 * This function manages its own per-system transactions internally.
 * Do NOT wrap it in an external transaction.
 *
 * @param database         Open SQLite connection (no active transaction required).
 * @param credentialsPath  Path to enrichment-credentials.json; silently skipped
 *                         if absent or if the igdb credentials block is empty.
 * @param gamesEnriched    [out] Number of game rows actually updated.
 * @param factsInserted    [out] Number of new game_facts rows inserted.
 * @param error            [out] Human-readable error message on failure.
 * @return true on success (or silently skipped), false on error.
 */
bool enrichFromIGDB(QSqlDatabase &database,
                    const QString &credentialsPath,
                    int &gamesEnriched,
                    int &factsInserted,
                    QString &error);

} // namespace CompendiumEnrichment
