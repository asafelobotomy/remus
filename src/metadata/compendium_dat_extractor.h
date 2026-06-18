#pragma once
// Phase 1 compendium compiler: DAT source extractor.
// Wraps ClrMameProParser and converts entries into SourceRecordEnvelopes
// suitable for system/region normalization and identity linking.

#include "compendium_types.h"
#include "clrmamepro_parser.h"

#include "../core/disc_title_parser.h"

#include <QList>
#include <QString>

namespace Remus {
namespace Compendium {

    /// One DAT @c game ( … ) block before envelope emission.
    struct DatGameBlock {
        QString gameName;
        QList<ClrMameProEntry> tracks;
        DiscTitleInfo titleInfo;
    };

    class DatExtractor {
    public:
        // Extract all normalized records from a single .dat file.
        // sourceId and snapshotId come from the manifest descriptor.
        // Returns an empty list and sets error on failure.
        static QList<SourceRecordEnvelope> extract(
            const QString &filePath, const QString &sourceId, const QString &snapshotId, QString &error);

        // Produce the stable external key for a DAT entry.
        // Format: "<systemHint>|<gameName>|<romName>" truncated to 512 chars.
        static QString makeExternalKey(const QString &systemHint, const ClrMameProEntry &entry);

        // Serialize a ClrMameProEntry to a compact JSON payload string
        // suitable for storage in source_items.payload_json.
        static QString entryToPayloadJson(const QString &systemHint, const ClrMameProEntry &entry);

        // Normalize a hash value: uppercase, spaces stripped.
        static QString normalizeHash(const QString &raw);

        // Normalize a serial value: uppercase, trimmed.
        static QString normalizeSerial(const QString &raw);

        /// Group parsed DAT entries into one block per @c gameName .
        static QList<DatGameBlock> groupGameBlocks(const QList<ClrMameProEntry> &entries);

        /// Data tracks for a single DAT game block (skips .cue/.m3u metadata rows).
        static QList<ClrMameProEntry> dataTracksForBlock(const QList<ClrMameProEntry> &group);
    };

} // namespace Compendium
} // namespace Remus
