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

    // Korea
    const QString kor = QStringLiteral("KOR");
    m_regionToCode.insert(QStringLiteral("korea"),       kor);
    m_regionToCode.insert(QStringLiteral("kor"),         kor);
    m_regionToCode.insert(QStringLiteral("south korea"), kor);
    m_regionToCode.insert(QStringLiteral("ntsc-k"),      kor);

    // China
    const QString chn = QStringLiteral("CHN");
    m_regionToCode.insert(QStringLiteral("china"),          chn);
    m_regionToCode.insert(QStringLiteral("chn"),            chn);
    m_regionToCode.insert(QStringLiteral("mainland china"), chn);

    // Taiwan
    const QString twn = QStringLiteral("TWN");
    m_regionToCode.insert(QStringLiteral("taiwan"), twn);
    m_regionToCode.insert(QStringLiteral("twn"),    twn);
    m_regionToCode.insert(QStringLiteral("tw"),     twn);

    // Asia (generic)
    const QString asia = QStringLiteral("ASIA");
    m_regionToCode.insert(QStringLiteral("asia"), asia);

    // France
    const QString fra = QStringLiteral("FRA");
    m_regionToCode.insert(QStringLiteral("france"),  fra);
    m_regionToCode.insert(QStringLiteral("fra"),     fra);
    m_regionToCode.insert(QStringLiteral("french"),  fra);

    // Germany
    const QString deu = QStringLiteral("DEU");
    m_regionToCode.insert(QStringLiteral("germany"),     deu);
    m_regionToCode.insert(QStringLiteral("ger"),         deu);
    m_regionToCode.insert(QStringLiteral("german"),      deu);
    m_regionToCode.insert(QStringLiteral("deutschland"), deu);

    // Italy
    const QString ita = QStringLiteral("ITA");
    m_regionToCode.insert(QStringLiteral("italy"),   ita);
    m_regionToCode.insert(QStringLiteral("ita"),     ita);
    m_regionToCode.insert(QStringLiteral("italian"), ita);

    // Spain
    const QString esp = QStringLiteral("ESP");
    m_regionToCode.insert(QStringLiteral("spain"),   esp);
    m_regionToCode.insert(QStringLiteral("spa"),     esp);
    m_regionToCode.insert(QStringLiteral("spanish"), esp);

    // Sweden
    const QString swe = QStringLiteral("SWE");
    m_regionToCode.insert(QStringLiteral("sweden"),  swe);
    m_regionToCode.insert(QStringLiteral("swe"),     swe);

    // Netherlands
    const QString nld = QStringLiteral("NLD");
    m_regionToCode.insert(QStringLiteral("netherlands"), nld);
    m_regionToCode.insert(QStringLiteral("holland"),     nld);
    m_regionToCode.insert(QStringLiteral("hol"),         nld);
    m_regionToCode.insert(QStringLiteral("nld"),         nld);

    // Portugal
    const QString por = QStringLiteral("POR");
    m_regionToCode.insert(QStringLiteral("portugal"), por);
    m_regionToCode.insert(QStringLiteral("por"),      por);

    // Scandinavia
    const QString sca = QStringLiteral("SCA");
    m_regionToCode.insert(QStringLiteral("scandinavia"), sca);
    m_regionToCode.insert(QStringLiteral("sca"),         sca);

    // Russia
    const QString rus = QStringLiteral("RUS");
    m_regionToCode.insert(QStringLiteral("russia"), rus);
    m_regionToCode.insert(QStringLiteral("rus"),    rus);

    // Latin America
    const QString latam = QStringLiteral("LATAM");
    m_regionToCode.insert(QStringLiteral("latin america"), latam);
    m_regionToCode.insert(QStringLiteral("latam"),         latam);
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

    // Normalize the 'region' field stored in game_facts to a canonical
    // region_code. The games.primary_region_code column has an FK to
    // regions(region_code), so the merge resolver's UPDATE would violate that
    // constraint if a raw string like "Korea" reached the column. Either map it
    // to the canonical code ("KOR") or remove it when it cannot be mapped.
    static const QString kRegionField = QStringLiteral("region");
    const auto it = record.fields.find(kRegionField);
    if (it != record.fields.end()) {
        const QString code = resolveRegionCode(it.value());
        if (!code.isEmpty()) {
            it.value() = code;
        } else {
            record.fields.erase(it);
        }
    }
}

} // namespace Compendium
} // namespace Remus
