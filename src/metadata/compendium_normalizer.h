#pragma once
// Phase 1 compendium compiler: system and region normalization.
// Resolves raw DAT system names and region strings into canonical IDs/codes
// aligned with the seeded compendium tables.

#include "compendium_types.h"
#include <QString>
#include <QMap>

namespace Remus {
namespace Compendium {

    class CompendiumNormalizer {
    public:
        CompendiumNormalizer();

        // Resolve a raw DAT system name to a canonical system_id.
        // Returns 0 if the system name cannot be resolved.
        int resolveSystemId(const QString &rawSystemName) const;

        // Normalize a raw region string to a seeded region_code.
        // Returns empty string if the region cannot be resolved.
        QString resolveRegionCode(const QString &rawRegion) const;

        // Apply system and region normalization to a record in-place.
        void normalize(SourceRecordEnvelope &record) const;

    private:
        // Maps from lowercased DAT system name substrings to system IDs.
        // Built from Constants::Systems::SYSTEMS in the constructor.
        QMap<QString, int> m_systemNameToId; // exact lowercase key → id

        // Maps from lowercased raw region token to region_code.
        QMap<QString, QString> m_regionToCode;

        void buildSystemMap();
        void buildRegionMap();

        static QString canonicalizeToken(const QString &raw);
    };

} // namespace Compendium
} // namespace Remus
