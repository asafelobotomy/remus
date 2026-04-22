#include "compendium_normalizer.h"
#include "../core/system_resolver.h"

namespace Remus {
namespace Compendium {

// ── Region mapping ────────────────────────────────────────────────────────────
// Maps common raw region tokens (lowercase) to compendium region_codes.
// The region_code values must match the seeded regions table.

void CompendiumNormalizer::buildRegionMap()
{
    // USA / Americas
    const QString usa = QStringLiteral("USA");
    m_regionToCode.insert(QStringLiteral("usa"),     usa);
    m_regionToCode.insert(QStringLiteral("us"),      usa);
    m_regionToCode.insert(QStringLiteral("ntsc-u"),  usa);
    m_regionToCode.insert(QStringLiteral("ntsc"),    usa);
    m_regionToCode.insert(QStringLiteral("america"), usa);
    m_regionToCode.insert(QStringLiteral("americas"), usa);
    m_regionToCode.insert(QStringLiteral("north america"), usa);

    // Europe
    const QString eur = QStringLiteral("EUR");
    m_regionToCode.insert(QStringLiteral("europe"),  eur);
    m_regionToCode.insert(QStringLiteral("eur"),     eur);
    m_regionToCode.insert(QStringLiteral("eu"),      eur);
    m_regionToCode.insert(QStringLiteral("pal"),     eur);
    m_regionToCode.insert(QStringLiteral("pal-e"),   eur);

    // Japan
    const QString jpn = QStringLiteral("JPN");
    m_regionToCode.insert(QStringLiteral("japan"),   jpn);
    m_regionToCode.insert(QStringLiteral("jpn"),     jpn);
    m_regionToCode.insert(QStringLiteral("jp"),      jpn);
    m_regionToCode.insert(QStringLiteral("ntsc-j"),  jpn);

    // World / global
    const QString world = QStringLiteral("WORLD");
    m_regionToCode.insert(QStringLiteral("world"),   world);
    m_regionToCode.insert(QStringLiteral("wld"),     world);
    m_regionToCode.insert(QStringLiteral("worldwide"), world);
    m_regionToCode.insert(QStringLiteral("all"),     world);

    // Other
    const QString aus = QStringLiteral("AUS");
    m_regionToCode.insert(QStringLiteral("australia"), aus);
    m_regionToCode.insert(QStringLiteral("aus"),       aus);

    const QString bra = QStringLiteral("BRA");
    m_regionToCode.insert(QStringLiteral("brazil"), bra);
    m_regionToCode.insert(QStringLiteral("bra"),    bra);
}

// ── Constructor ───────────────────────────────────────────────────────────────

CompendiumNormalizer::CompendiumNormalizer()
{
    buildRegionMap();
    // System map is delegated to SystemResolver::systemIdByDatName at query time.
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString CompendiumNormalizer::canonicalizeToken(const QString &raw)
{
    return raw.trimmed().toLower();
}

// ── Public API ────────────────────────────────────────────────────────────────

int CompendiumNormalizer::resolveSystemId(const QString &rawSystemName) const
{
    if (rawSystemName.isEmpty()) {
        return 0;
    }
    // Delegate to the existing authoritative resolver that knows the full
    // No-Intro/Redump DAT name → system ID mapping.
    return SystemResolver::systemIdByDatName(rawSystemName);
}

QString CompendiumNormalizer::resolveRegionCode(const QString &rawRegion) const
{
    if (rawRegion.isEmpty()) {
        return {};
    }

    // Try the first comma-delimited token (e.g. "USA, Europe" → "USA").
    const QString primary = rawRegion.split(QLatin1Char(',')).first().trimmed();
    const QString key = canonicalizeToken(primary);

    if (m_regionToCode.contains(key)) {
        return m_regionToCode.value(key);
    }

    return {};
}

void CompendiumNormalizer::normalize(SourceRecordEnvelope &record) const
{
    record.resolvedSystemId   = resolveSystemId(record.systemHint);
    record.resolvedRegionCode = resolveRegionCode(record.regionRaw);
}

} // namespace Compendium
} // namespace Remus
