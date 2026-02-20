#pragma once
// Internal shared helpers for CLI command handlers.
// Include from CLI .cpp files only — not from public library headers.

#include <memory>
#include <QCommandLineParser>
#include <QList>
#include <QString>
#include "../core/database.h"
#include "../core/hasher.h"
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
std::unique_ptr<ProviderOrchestrator> buildOrchestrator(const QCommandLineParser &parser);

// Return only files that have at least one computed hash value.
QList<FileRecord> getHashedFiles(Database &db);

// Insert a matched game into the database and record the match confidence/method.
// Returns the newly-inserted gameId, or 0 on failure.
int persistMetadata(Database &db, const FileRecord &file, const GameMetadata &metadata);

// Print a detailed file record to the current log category.
void printFileInfo(const FileRecord &file);
