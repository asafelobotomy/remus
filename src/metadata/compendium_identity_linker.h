#pragma once
// Phase 1 compendium compiler: identity linker.
// Groups a flat list of SourceRecordEnvelopes into canonical games using
// a conservative three-pass strategy:
//   Pass 1 — exact hash collision (sha1, then md5, then crc32)
//   Pass 2 — exact serial match (within the same system)
//   Pass 3 — conservative normalized title match (same system, same region)
// After linking each record has a non-empty linkedGameId.

#include "compendium_types.h"

#include <QList>
#include <QMap>
#include <QString>

namespace Remus {
namespace Compendium {

class IdentityLinker
{
public:
    // Link all records in-place.  After this call every envelope has a
    // non-empty linkedGameId and a populated linkedConfidencePercent.
    // Returns the number of distinct canonical games created.
    int link(QList<SourceRecordEnvelope> &records) const;

private:
    // Generate a stable deterministic game_id from a seed string.
    static QString generateGameId(const QString &seed);

    // Normalize a title for conservative fuzzy comparison.
    static QString normalizeTitle(const QString &raw);
};

} // namespace Compendium
} // namespace Remus
