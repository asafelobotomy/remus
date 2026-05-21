#pragma once
// Shared in-flight data types for the Phase 1 compendium compiler pipeline.
// Every extractor, linker, fact inserter and merge resolver works exclusively
// with these structs so the stages are decoupled and independently testable.

#include <QString>
#include <QStringList>
#include <QMap>

namespace Remus {
namespace Compendium {

// ── Normalized hash container ─────────────────────────────────────────────────

struct NormalizedHashes {
    QString crc32;   // uppercase, no spaces (e.g. "F9394E97")
    QString md5;     // lowercase hex
    QString sha1;    // lowercase hex
    QString sha256;  // lowercase hex

    bool hasAny() const
    {
        return !crc32.isEmpty() || !md5.isEmpty() || !sha1.isEmpty() || !sha256.isEmpty();
    }
};

// ── Source record envelope ────────────────────────────────────────────────────
// Single normalized record produced by any source extractor before persistence.
// After identity linking the linkedGameId field is populated.

struct SourceRecordEnvelope {
    // Source provenance
    QString sourceId;       // "nointro", "redump", "libretro_metadata", …
    QString snapshotId;     // stable snapshot key from manifest
    QString externalKey;    // source-native stable key (e.g. datName + romName)

    // Raw fields (stored as provenance; used for linking before normalization)
    QString systemHint;     // raw system string from source file
    QString titleRaw;       // raw game title from source
    QString regionRaw;      // raw region string ("USA", "Europe", …)

    // Normalized identity
    NormalizedHashes hashes;
    QStringList serials;    // normalized uppercase trimmed serial values

    // Candidate metadata facts — field_name → field_value
    // Keys follow the Phase 1 schema field_name vocabulary:
    //   title, publisher, developer, release_year, release_date,
    //   genre, players_max, description, rating, region
    QMap<QString, QString> fields;

    // Raw source payload kept for provenance (full JSON-serialized entry)
    QString payloadJson;

    // Set by the identity linker after assignment
    QString linkedGameId;           // canonical game_id once linked
    int     linkedConfidencePercent = 0;  // 0-100

    // Resolved canonical system/region from normalization pass
    int     resolvedSystemId = 0;   // 0 = unresolved
    QString resolvedRegionCode;     // empty = unresolved
};

// ── Compiler run statistics ───────────────────────────────────────────────────
// Updated incrementally by the compiler service and written to the report.

struct CompilerStats {
    int recordsIngested      = 0;
    int gamesCreated         = 0;
    int deduplicatedGames    = 0;
    int signaturesCreated    = 0;
    int serialsCreated       = 0;
    int factsCreated         = 0;
    int resolvedFields       = 0;
    int unresolvedConflicts  = 0;
    int skippedDisabled      = 0;
    int collisionHashSkipped = 0;
};

} // namespace Compendium
} // namespace Remus
