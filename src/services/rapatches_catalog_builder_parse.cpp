#include "rapatches_catalog_builder.h"

namespace Remus {

QString RAPatchesCatalogBuilder::normaliseSystemName(const QString &dirName)
{
    static const QMap<QString, QString> mapping = {
        {QStringLiteral("3DO"),                 QStringLiteral("3DO")},
        {QStringLiteral("Amstrad CPC"),         QStringLiteral("Amstrad CPC")},
        {QStringLiteral("Apple II"),            QStringLiteral("Apple II")},
        {QStringLiteral("Arcadia 2001"),        QStringLiteral("Arcadia 2001")},
        {QStringLiteral("Atari 2600"),          QStringLiteral("Atari 2600")},
        {QStringLiteral("Atari Jaguar"),        QStringLiteral("Atari Jaguar")},
        {QStringLiteral("Dreamcast"),           QStringLiteral("Dreamcast")},
        {QStringLiteral("Famicom Disk System"), QStringLiteral("Famicom Disk System")},
        {QStringLiteral("GBA"),                 QStringLiteral("Game Boy Advance")},
        {QStringLiteral("GBC"),                 QStringLiteral("Game Boy Color")},
        {QStringLiteral("Game Boy"),            QStringLiteral("Game Boy")},
        {QStringLiteral("Game Gear"),           QStringLiteral("Game Gear")},
        {QStringLiteral("GameCube"),            QStringLiteral("GameCube")},
        {QStringLiteral("MD"),                  QStringLiteral("Sega Genesis / Mega Drive")},
        {QStringLiteral("MSX"),                 QStringLiteral("MSX")},
        {QStringLiteral("Master System"),       QStringLiteral("Master System")},
        {QStringLiteral("N64"),                 QStringLiteral("Nintendo 64")},
        {QStringLiteral("NDS"),                 QStringLiteral("Nintendo DS")},
        {QStringLiteral("NES"),                 QStringLiteral("NES")},
        {QStringLiteral("Neo Geo CD"),          QStringLiteral("Neo Geo CD")},
        {QStringLiteral("Neo Geo Pocket"),      QStringLiteral("Neo Geo Pocket")},
        {QStringLiteral("Nintendo 64DD"),       QStringLiteral("Nintendo 64DD")},
        {QStringLiteral("PC Engine CD"),        QStringLiteral("PC Engine CD")},
        {QStringLiteral("PC Engine"),           QStringLiteral("PC Engine")},
        {QStringLiteral("PC-8801"),             QStringLiteral("PC-8801")},
        {QStringLiteral("PC-FX"),               QStringLiteral("PC-FX")},
        {QStringLiteral("PS2"),                 QStringLiteral("PlayStation 2")},
        {QStringLiteral("PlayStation Portable"),QStringLiteral("PlayStation Portable")},
        {QStringLiteral("PlayStation"),         QStringLiteral("PlayStation")},
        {QStringLiteral("Pokemon Mini"),        QStringLiteral("Pokemon Mini")},
        {QStringLiteral("SG-1000"),             QStringLiteral("SG-1000")},
        {QStringLiteral("SNES"),                QStringLiteral("Super Nintendo")},
        {QStringLiteral("Saturn"),              QStringLiteral("Saturn")},
        {QStringLiteral("Sega 32X"),            QStringLiteral("Sega 32X")},
        {QStringLiteral("Sega CD"),             QStringLiteral("Sega CD")},
        {QStringLiteral("Vectrex"),             QStringLiteral("Vectrex")},
        {QStringLiteral("Virtual Boy"),         QStringLiteral("Virtual Boy")},
        {QStringLiteral("Watara Supervision"),  QStringLiteral("Watara Supervision")},
        {QStringLiteral("Wii"),                 QStringLiteral("Wii")},
        {QStringLiteral("WonderSwan"),          QStringLiteral("WonderSwan")},
    };

    auto it = mapping.constFind(dirName);
    return it != mapping.constEnd() ? it.value() : dirName;
}

QString RAPatchesCatalogBuilder::normaliseTypeName(const QString &dirName)
{
    static const QMap<QString, QString> mapping = {
        {QStringLiteral("Fix"),           QStringLiteral("fix")},
        {QStringLiteral("Hacks"),         QStringLiteral("hack")},
        {QStringLiteral("Translation"),   QStringLiteral("translation")},
        {QStringLiteral("Improvement"),   QStringLiteral("improvement")},
        {QStringLiteral("MSU-1"),         QStringLiteral("enhancement")},
        {QStringLiteral("MSU-1-Older"),   QStringLiteral("enhancement")},
        {QStringLiteral("Subset"),        QStringLiteral("subset")},
        {QStringLiteral("GTConversion"),  QStringLiteral("conversion")},
    };

    auto it = mapping.constFind(dirName);
    return it != mapping.constEnd() ? it.value() : dirName.toLower();
}

RAPatchesCatalogBuilder::ParsedFilename RAPatchesCatalogBuilder::parseFilename(const QString &filename)
{
    ParsedFilename result;

    const int dotPos = filename.lastIndexOf(QLatin1Char('.'));
    const QString baseName = dotPos > 0 ? filename.left(dotPos) : filename;
    if (dotPos > 0) {
        result.format = filename.mid(dotPos + 1).toLower();
    }

    static const QRegularExpression parenRe(QStringLiteral("\\(([^)]+)\\)"));
    auto it = parenRe.globalMatch(baseName);

    QStringList segments;
    int firstParenPos = -1;
    while (it.hasNext()) {
        const auto match = it.next();
        if (firstParenPos < 0) {
            firstParenPos = match.capturedStart();
        }
        segments.append(match.captured(1).trimmed());
    }

    result.title = firstParenPos > 0 ? baseName.left(firstParenPos).trimmed() : baseName.trimmed();

    static const QRegularExpression regionRe(
        QStringLiteral("^(USA|Europe|Japan|World|Korea|Australia|Brazil|China|France|Germany"
                       "|Italy|Spain|Netherlands|Scandinavia|Asia|Canada"
                       "|USA,\\s*Europe|USA,\\s*Europe,\\s*Korea)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression langRe(
        QStringLiteral("^(En|Ja|Fr|De|Es|It|Nl|Pt|Ru|Zh|Ko|Sv|Da|No|Fi|Pl|Cs|Hu|Ro|Tr|Ar|He|Th|Vi)$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression versionRe(
        QStringLiteral("^(v\\d|Final|Alpha|Beta|Alt\\s*\\d|Demo|RC\\d|v\\d{8})"),
        QRegularExpression::CaseInsensitiveOption);

    for (int i = 0; i < segments.size(); ++i) {
        const QString &seg = segments[i];
        if (result.region.isEmpty() && regionRe.match(seg).hasMatch()) {
            result.region = seg;
        } else if (result.language.isEmpty() && langRe.match(seg).hasMatch()) {
            result.language = seg;
        } else if (result.version.isEmpty() && versionRe.match(seg).hasMatch()) {
            result.version = seg;
        } else if (i == segments.size() - 1 && result.author.isEmpty()) {
            result.author = seg;
        }
    }

    return result;
}

RAPatchesCatalogBuilder::ReadmeHashes RAPatchesCatalogBuilder::parseReadme(const QString &readmeContent)
{
    ReadmeHashes result;
    if (readmeContent.isEmpty()) {
        return result;
    }

    const QStringList lines = readmeContent.split(QLatin1Char('\n'));
    static const QRegularExpression md5Re(QStringLiteral("(?:MD5|md5)[:\\s]+([0-9a-fA-F]{32})"));
    static const QRegularExpression crcRe(QStringLiteral("(?:CRC|crc|CRC32|crc32)[:\\s]+([0-9a-fA-F]{8})"));
    static const QRegularExpression romNameRe(
        QStringLiteral("^\\[?([^\\[\\]]+\\.(?:nes|sfc|smc|gb|gbc|gba|md|bin|gen|n64|z64|nds|iso|cue|chd))\\]?$"),
        QRegularExpression::CaseInsensitiveOption);

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (result.baseMd5.isEmpty()) {
            const auto match = md5Re.match(trimmed);
            if (match.hasMatch()) {
                result.baseMd5 = match.captured(1).toLower();
            }
        }
        if (result.baseCrc32.isEmpty()) {
            const auto match = crcRe.match(trimmed);
            if (match.hasMatch()) {
                result.baseCrc32 = match.captured(1).toLower();
            }
        }
        if (result.baseRomName.isEmpty()) {
            const auto match = romNameRe.match(trimmed);
            if (match.hasMatch()) {
                result.baseRomName = match.captured(1).trimmed();
            }
        }
    }

    return result;
}

} // namespace Remus