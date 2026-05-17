#pragma once
// Internal shared helpers for CLI command handlers.
// Include from CLI .cpp files only — not from public library headers.

#include <memory>
#include <QCommandLineParser>
#include <QList>
#include <QSet>
#include <QString>
#include "../core/database.h"
#include "../core/hasher.h"
#include "../core/match_utils.h"
#include "../core/conversion_result.h"
#include "../core/constants/constants.h"
#include "../metadata/metadata_provider.h"
#include "../metadata/provider_orchestrator.h"

using namespace Remus;
using namespace Remus::Constants;

// Select the best available hash for a file, preferring the algorithm appropriate
// for the file's system (disc-based → MD5/SHA1; cartridge → CRC32).
QString selectBestHash(const FileRecord &file);

// Calculate hashes for a file record, transparently handling compressed archives
// by extracting to a temporary directory first.
HashResult hashFileRecord(const FileRecord &file, Hasher &hasher);

// Construct a ProviderOrchestrator configured from parser credentials.
// Adds Hasheous, TheGamesDB, and IGDB unconditionally; ScreenScraper only
// when --ss-user / --ss-pass are both set.
// When a database reference is provided, creates a MetadataCache and
// attaches it to the orchestrator.
std::unique_ptr<ProviderOrchestrator> buildOrchestrator(const QCommandLineParser &parser,
                                                         Database *db = nullptr);

// Discover a data/<subdir>/ directory relative to cwd or app location.
QString findDataSubdir(const QString &subdir);

// Convenience wrappers for common data subdirectories.
inline QString findDatabaseDir()  { return findDataSubdir(QStringLiteral("databases")); }
inline QString findMetadataDir()  { return findDataSubdir(QStringLiteral("metadata")); }
inline QString findGameTDBDir()   { return findDataSubdir(QStringLiteral("gametdb")); }
inline QString findOpenVGDBPath() {
    const QString dir = findDataSubdir(QStringLiteral("openvgdb"));
    return dir.isEmpty() ? QString() : dir + QStringLiteral("/openvgdb.sqlite");
}

// Probe for artwork that may have been renamed to match the detected image
// format after download. Expects a path without an extension.
QString findExistingArtworkPath(const QString &basePath);

// Resolve an option value when presets are acting as defaults.
// Explicit CLI values win, then preset values, then parser defaults.
QString resolveCliOptionValue(const QCommandLineParser &parser,
                              const QString &optionName,
                              const QString &presetValue = QString());

// Return only files that have at least one computed hash value.
QList<FileRecord> getHashedFiles(Database &db);
QList<FileRecord> getHashedFiles(Database &db, const QSet<int> &fileScopeIds);
bool fileMatchesProcessScope(const FileRecord &file, const QSet<int> &fileScopeIds);

// Resolve the effective system ID for downstream handling, preferring the
// matched game system when available and falling back to the scanned file.
int resolveMatchedSystemId(const FileRecord &file,
                           const Database::MatchResult *match = nullptr);

// Convenience predicate for per-system process batches.
bool fileMatchesSystemFilter(const FileRecord &file,
                             int systemId,
                             const Database::MatchResult *match = nullptr);

// Return the best user-facing name for matching/search. For archive-backed
// records this prefers the container name over the inner entry extension.
QString getMatchingDisplayName(const FileRecord &file);

// Return the best system name to pass into metadata providers for this file.
QString getMatchingSystemName(const FileRecord &file);

// Return the best provider lookup system name, preferring the matched game
// system when available and otherwise falling back to the scanned file.
QString getProviderLookupSystemName(const FileRecord &file,
                                    const Database::MatchResult *match = nullptr);

// Insert a matched game into the database and record the match confidence/method.
// Returns the newly-inserted gameId, or 0 on failure.
int persistMetadata(Database &db, const FileRecord &file, const GameMetadata &metadata);

// Print a detailed file record to the current log category.
void printFileInfo(const FileRecord &file);

// Build an output path from an input file, optional output directory, and target extension.
// Creates the output directory if it doesn't exist.
QString buildOutputPath(const QString &inputPath, const QString &outputDir, const QString &targetExt);

// Print conversion result statistics (sizes, compression ratio).
// Returns true on success, false on failure.
bool printConversionResult(const ConversionResult &result, const QString &formatName);
