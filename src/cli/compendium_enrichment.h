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

/**
 * @brief Enrich games using the RetroAchievements online API (hash-based matching).
 *
 * For each system that has a RetroAchievements system-ID mapping, bulk-fetches
 * the full game list with MD5 hashes (one API call per system), matches the
 * hashes against @c game_signatures, and then:
 *  - Writes @c ra_game_id and @c achievement_count facts for every hash match.
 *  - For matched games still missing genre/developer/publisher/release year,
 *    calls @c API_GetGame.php to retrieve metadata and apply COALESCE updates
 *    (existing values are never overwritten). Games already stamped with a
 *    prior @c ra_game_id fact suppress repeated no-op metadata retries.
 *
 * This function manages its own per-system transactions internally.
 * Do NOT wrap it in an external transaction.
 *
 * Credentials are read from the @c retroachievements block of the shared
 * @p credentialsPath JSON file:
 * @code
 * { "retroachievements": { "username": "...", "api_key": "..." } }
 * @endcode
 * Silently skipped if the file is absent or the credentials block is empty.
 *
 * @param database         Open SQLite connection (no active transaction required).
 * @param credentialsPath  Path to enrichment-credentials.json.
 * @param gamesEnriched    [out] Number of game rows updated with full metadata.
 * @param factsInserted    [out] Number of new game_facts rows inserted
 *                              (includes ra_game_id and achievement_count rows).
 * @param error            [out] Human-readable error message on failure.
 * @return true on success (or silently skipped), false on error.
 */
bool enrichFromRetroAchievements(QSqlDatabase &database,
                                  const QString &credentialsPath,
                                  int &gamesEnriched,
                                  int &factsInserted,
                                  QString &error,
                                  int *apiCallsNeededOut = nullptr,
                                  int *apiCallsPerformedOut = nullptr,
                                  int *apiCallsSuppressedOut = nullptr);

/**
 * @brief Enrich Arcade/MAME games using the MAME catver.ini category database.
 *
 * Reads the `[Category]` section of a local catver.ini file and writes genre
 * strings for Arcade games whose @c canonical_title matches a catver ROM name.
 * Uses COALESCE semantics (existing genre values are never overwritten).
 *
 * Callers are responsible for wrapping calls in a database transaction.
 *
 * @param database    Open SQLite connection (must be in a transaction).
 * @param catverPath  Path to catver.ini (skipped if file absent).
 * @param gamesEnriched [out] Number of game rows updated.
 * @param factsInserted [out] Number of new game_facts rows inserted.
 * @param error       [out] Human-readable error message on failure.
 * @return true on success, false on error.
 */
bool enrichFromMameCatver(QSqlDatabase &database,
                          const QString &catverPath,
                          int &gamesEnriched,
                          int &factsInserted,
                          QString &error);

/**
 * @brief Enrich Arcade/MAME games using a MAME listxml XML database.
 *
 * Reads machine entries from a local `mame -listxml` XML export and writes
 * developer, publisher, release_year, and players_max for Arcade games whose
 * @c canonical_title matches a machine @c name attribute.
 * Uses COALESCE semantics (existing values are never overwritten).
 *
 * Only non-device, runnable machines are processed. Clones are included.
 * The @c <description> field is intentionally ignored — it contains the
 * display name, not a synopsis.
 *
 * Callers are responsible for wrapping calls in a database transaction.
 *
 * @param database      Open SQLite connection (must be in a transaction).
 * @param listxmlPath   Path to listxml.xml (skipped if file absent).
 * @param gamesEnriched [out] Number of game rows updated.
 * @param factsInserted [out] Number of new game_facts rows inserted.
 * @param error         [out] Human-readable error message on failure.
 * @return true on success, false on error.
 */
bool enrichFromMameListXml(QSqlDatabase &database,
                            const QString &listxmlPath,
                            int &gamesEnriched,
                            int &factsInserted,
                            QString &error);

/**
 * @brief Enrich ZX Spectrum games using the ZXInfo online API.
 *
 * Searches each ZX Spectrum game by title against the ZXInfo/ZXDB search API
 * and writes genre, release year, publisher, and developer (when available)
 * using COALESCE semantics.
 *
 * This function manages its own transaction internally.
 * Do NOT wrap it in an external transaction.
 *
 * @param database      Open SQLite connection (no active transaction required).
 * @param gamesEnriched [out] Number of game rows updated.
 * @param factsInserted [out] Number of new game_facts rows inserted.
 * @param error         [out] Human-readable error message on failure.
 * @return true on success, false on error.
 */
bool enrichFromZXInfo(QSqlDatabase &database,
                      int &gamesEnriched,
                      int &factsInserted,
                      QString &error);

} // namespace CompendiumEnrichment
