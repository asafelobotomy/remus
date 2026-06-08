#pragma once
// Phase 1 compendium compiler: DAT source extractor.
// Wraps ClrMameProParser and converts entries into SourceRecordEnvelopes
// suitable for system/region normalization and identity linking.

#include "compendium_types.h"
#include "clrmamepro_parser.h"

#include <QString>
#include <QList>

namespace Remus {
namespace Compendium {

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
    };

} // namespace Compendium
} // namespace Remus
