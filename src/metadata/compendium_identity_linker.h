#pragma once
// Phase 1 compendium compiler: identity linker.
// Groups a flat list of SourceRecordEnvelopes into canonical games using
// a conservative four-pass strategy:
//   Pass 0 — exact hash collision (sha256)
//   Pass 1 — exact hash collision (sha1, then md5, then crc32)
//   Pass 2 — exact serial match (within the same system)
//   Pass 3 — conservative normalized title match (same system, same region)
// After linking each record has a non-empty linkedGameId.

#include "compendium_types.h"

#include <QHash>
#include <QList>
#include <QSqlDatabase>
#include <QString>

namespace Remus {
namespace Compendium {

    class IdentityLinker {
    public:
        // Pre-populate identity maps from an already-built compendium DB so that
        // newly ingested records link to existing games rather than minting fresh
        // IDs for titles that are already present.  Must be called before the
        // first link() call; calling it a second time will insert duplicates.
        // Returns false on fatal DB error.
        bool loadFromDatabase(QSqlDatabase &db, QString &error);

        // Link all records in-place.  After this call every envelope has a
        // non-empty linkedGameId and a populated linkedConfidencePercent.
        // Returns the number of distinct canonical games created in this batch.
        // Identity maps accumulate across calls so that records from different
        // sources are deduplicated against previously linked games.
        int link(QList<SourceRecordEnvelope> &records);

    private:
        // Generate a stable deterministic game_id from a seed string.
        static QString generateGameId(const QString &seed);

        // Normalize a title for conservative fuzzy comparison.
        static QString normalizeTitle(const QString &raw);

        // Identity maps — persist across link() calls for cross-source dedup.
        QHash<QString, QString> m_sha256ToId;
        QHash<QString, QString> m_sha1ToId;
        QHash<QString, QString> m_md5ToId;
        QHash<QString, QString> m_crc32ToId;
        QHash<QString, QString> m_titleToId;
        QHash<QString, QString> m_serialToId;
    };

} // namespace Compendium
} // namespace Remus
