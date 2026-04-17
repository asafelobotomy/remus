#include "rapatches_catalog_builder.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>

#include "../core/constants/constants.h"

namespace Remus {

namespace {

// Directories to skip in the RAPatches repo root
const QStringList kSkipDirs = {
    QStringLiteral("Removed"),
    QStringLiteral("Saves"),
    QStringLiteral("Utilities"),
    QStringLiteral("Unsorted"),
    QStringLiteral("DLC"),
    QStringLiteral("misc"),
    QStringLiteral(".git"),
};

// Patch file extensions we recognise
const QStringList kPatchExtensions = {
    QStringLiteral("bps"),
    QStringLiteral("ips"),
    QStringLiteral("xdelta"),
    QStringLiteral("ups"),
    QStringLiteral("ppf"),
    QStringLiteral("vcdiff"),
};

bool isPatchFile(const QString &filename)
{
    const QString ext = QFileInfo(filename).suffix().toLower();
    return kPatchExtensions.contains(ext);
}

bool isArchiveFile(const QString &filename)
{
    const QString ext = QFileInfo(filename).suffix().toLower();
    return ext == QStringLiteral("zip") || ext == QStringLiteral("7z");
}

// Generate a stable ID from system + type + filename
QString generateId(const QString &system, const QString &type, const QString &filename)
{
    const QString key = system + QStringLiteral("/") + type
                      + QStringLiteral("/") + filename;
    const QByteArray hash = QCryptographicHash::hash(key.toUtf8(),
                                                      QCryptographicHash::Md5);
    return QStringLiteral("ra-") + QString::fromLatin1(hash.toHex().left(12));
}

// Try to extract readme.txt content from a zip using unzip -p
QString extractReadmeFromZip(const QString &zipPath)
{
    QProcess proc;
    proc.setProgram(QStringLiteral("unzip"));
    proc.setArguments({QStringLiteral("-p"), zipPath, QStringLiteral("readme.txt")});
    proc.start();
    if (!proc.waitForFinished(5000)) {
        return {};
    }
    if (proc.exitCode() != 0) {
        // Try case variations
        proc.setArguments({QStringLiteral("-p"), zipPath, QStringLiteral("Readme.txt")});
        proc.start();
        if (!proc.waitForFinished(5000) || proc.exitCode() != 0) {
            proc.setArguments({QStringLiteral("-p"), zipPath, QStringLiteral("README.txt")});
            proc.start();
            if (!proc.waitForFinished(5000) || proc.exitCode() != 0) {
                return {};
            }
        }
    }
    return QString::fromUtf8(proc.readAllStandardOutput());
}

// List files inside a zip
QStringList listZipContents(const QString &zipPath)
{
    QProcess proc;
    proc.setProgram(QStringLiteral("unzip"));
    proc.setArguments({QStringLiteral("-Z1"), zipPath});
    proc.start();
    if (!proc.waitForFinished(5000) || proc.exitCode() != 0) {
        return {};
    }
    const QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (output.isEmpty()) return {};
    return output.split(QStringLiteral("\n"), Qt::SkipEmptyParts);
}

} // anonymous namespace

// ============================================================================
// System & type name normalisation
// ============================================================================

QString RAPatchesCatalogBuilder::normaliseSystemName(const QString &dirName)
{
    // Map RAPatches directory names to Remus system display names
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
        {QStringLiteral("PlayStation"),          QStringLiteral("PlayStation")},
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
    if (it != mapping.constEnd()) {
        return it.value();
    }
    return dirName; // Pass through unmapped names
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
    if (it != mapping.constEnd()) {
        return it.value();
    }
    return dirName.toLower();
}

// ============================================================================
// Filename parsing
// ============================================================================

RAPatchesCatalogBuilder::ParsedFilename
RAPatchesCatalogBuilder::parseFilename(const QString &filename)
{
    ParsedFilename result;

    // Strip extension to get base name
    const int dotPos = filename.lastIndexOf(QLatin1Char('.'));
    const QString baseName = (dotPos > 0) ? filename.left(dotPos) : filename;
    if (dotPos > 0) {
        result.format = filename.mid(dotPos + 1).toLower();
    }

    // Pattern: Title (Region) (Language) (Version) (Author)
    // Each parenthesised group is optional
    // Language codes: 2-letter ISO 639-1 with first letter capitalised (En, Ja, Fr, etc.)
    // Version: v1.0, v0.9, v20240120, Final, Beta, Alpha, Alt 1
    // Author: anything in the last parens that isn't region/lang/version

    // Extract all parenthesised segments
    static const QRegularExpression parenRe(QStringLiteral("\\(([^)]+)\\)"));
    auto it = parenRe.globalMatch(baseName);

    QStringList segments;
    int firstParenPos = -1;
    while (it.hasNext()) {
        auto match = it.next();
        if (firstParenPos < 0) {
            firstParenPos = match.capturedStart();
        }
        segments.append(match.captured(1).trimmed());
    }

    // Title is everything before the first parenthesised segment
    if (firstParenPos > 0) {
        result.title = baseName.left(firstParenPos).trimmed();
    } else {
        result.title = baseName.trimmed();
    }

    // Heuristic classification of each segment
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
            // Last unmatched segment is likely the author
            result.author = seg;
        }
        // Earlier unmatched segments may be additional title qualifiers
    }

    return result;
}

// ============================================================================
// README parsing for base ROM hashes
// ============================================================================

RAPatchesCatalogBuilder::ReadmeHashes
RAPatchesCatalogBuilder::parseReadme(const QString &readmeContent)
{
    ReadmeHashes result;
    if (readmeContent.isEmpty()) return result;

    const QStringList lines = readmeContent.split(QLatin1Char('\n'));

    // Look for MD5 and CRC patterns
    static const QRegularExpression md5Re(
        QStringLiteral("(?:MD5|md5)[:\\s]+([0-9a-fA-F]{32})"));
    static const QRegularExpression crcRe(
        QStringLiteral("(?:CRC|crc|CRC32|crc32)[:\\s]+([0-9a-fA-F]{8})"));
    // ROM name often follows "Use with:" or "[ROM file name]"
    static const QRegularExpression romNameRe(
        QStringLiteral("^\\[?([^\\[\\]]+\\.(?:nes|sfc|smc|gb|gbc|gba|md|bin|gen|n64|z64|nds|iso|cue|chd))\\]?$"),
        QRegularExpression::CaseInsensitiveOption);

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();

        if (result.baseMd5.isEmpty()) {
            auto m = md5Re.match(trimmed);
            if (m.hasMatch()) {
                result.baseMd5 = m.captured(1).toLower();
            }
        }

        if (result.baseCrc32.isEmpty()) {
            auto m = crcRe.match(trimmed);
            if (m.hasMatch()) {
                result.baseCrc32 = m.captured(1).toLower();
            }
        }

        if (result.baseRomName.isEmpty()) {
            auto m = romNameRe.match(trimmed);
            if (m.hasMatch()) {
                result.baseRomName = m.captured(1).trimmed();
            }
        }
    }

    return result;
}

// ============================================================================
// Directory scanning
// ============================================================================

RAPatchesCatalogBuilder::BuildResult
RAPatchesCatalogBuilder::buildFromDirectory(const QString &repoPath) const
{
    BuildResult result;

    QDir rootDir(repoPath);
    if (!rootDir.exists()) {
        result.error = QStringLiteral("RAPatches directory does not exist: ") + repoPath;
        return result;
    }

    // Each top-level directory is a system (except skipped dirs)
    const QStringList systemDirs = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                      QDir::Name);
    for (const QString &dirName : systemDirs) {
        if (kSkipDirs.contains(dirName)) {
            continue;
        }

        // Skip loose files at root level (README.md, etc.)
        const QDir systemDir(rootDir.filePath(dirName));
        const QString system = normaliseSystemName(dirName);
        scanSystemDir(systemDir, system, result);
    }

    return result;
}

void RAPatchesCatalogBuilder::scanSystemDir(const QDir &systemDir,
                                             const QString &system,
                                             BuildResult &result) const
{
    const QStringList entries = systemDir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                     QDir::Name);

    for (const QString &entry : entries) {
        const QString fullPath = systemDir.filePath(entry);
        QFileInfo fi(fullPath);

        if (fi.isDir()) {
            // Type subdirectory (Fix, Hacks, Translation, etc.)
            const QString type = normaliseTypeName(entry);
            scanTypeDir(QDir(fullPath), system, type, result);
        } else if (isArchiveFile(entry)) {
            // Archive directly under system dir (no type subdir)
            auto mod = buildEntryFromZip(fullPath, system, QStringLiteral("hack"));
            if (!mod.id.isEmpty()) {
                result.mods.append(std::move(mod));
            }
            result.filesScanned++;
        } else if (isPatchFile(entry)) {
            auto mod = buildEntryFromPatch(fullPath, system, QStringLiteral("hack"));
            if (!mod.id.isEmpty()) {
                result.mods.append(std::move(mod));
            }
            result.filesScanned++;
        }
    }
}

void RAPatchesCatalogBuilder::scanTypeDir(const QDir &typeDir,
                                           const QString &system,
                                           const QString &type,
                                           BuildResult &result) const
{
    // Type directories can have subdirectories (e.g. Translation/English/)
    QDirIterator it(typeDir.path(),
                    QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        const QString filePath = it.filePath();
        const QString fileName = it.fileName();

        if (isArchiveFile(fileName)) {
            auto mod = buildEntryFromZip(filePath, system, type);
            if (!mod.id.isEmpty()) {
                result.mods.append(std::move(mod));
            }
            result.filesScanned++;
        } else if (isPatchFile(fileName)) {
            auto mod = buildEntryFromPatch(filePath, system, type);
            if (!mod.id.isEmpty()) {
                result.mods.append(std::move(mod));
            }
            result.filesScanned++;
        } else {
            result.filesSkipped++;
        }
    }
}

ModEntry RAPatchesCatalogBuilder::buildEntryFromZip(const QString &zipPath,
                                                     const QString &system,
                                                     const QString &type) const
{
    ModEntry entry;
    const QFileInfo fi(zipPath);
    const QString zipName = fi.completeBaseName(); // e.g. "1234-SonicRussian"

    // List zip contents to find the patch file
    const QStringList contents = listZipContents(zipPath);
    QString patchFileName;
    for (const QString &name : contents) {
        if (isPatchFile(name)) {
            patchFileName = name;
            break;
        }
    }

    if (patchFileName.isEmpty()) {
        // No patch file found inside zip — skip but still generate a basic entry
        // from the zip filename if it follows the GAMEID-Title pattern
        entry.id = generateId(system, type, fi.fileName());
        entry.title = zipName;
        entry.system = system;
        entry.type = type;
        entry.patchUrl = Constants::API::RAPATCHES_RAW_BASE + QStringLiteral("/")
                       + QDir(zipPath).dirName(); // Approximate — will need path relative to repo
        entry.sourceUrl = QStringLiteral("https://github.com/RetroAchievements/RAPatches");
        return entry;
    }

    // Parse the patch filename for metadata
    const auto parsed = parseFilename(patchFileName);
    entry.id = generateId(system, type, fi.fileName());
    entry.title = parsed.title.isEmpty() ? zipName : parsed.title;
    entry.author = parsed.author;
    entry.version = parsed.version;
    entry.system = system;
    entry.type = type;
    entry.format = parsed.format;
    entry.patchSize = fi.size();

    // Build download URL relative to RAPatches repo root
    // The zip is the download unit — path relative to repo
    entry.sourceUrl = QStringLiteral("https://github.com/RetroAchievements/RAPatches");

    // Extract readme for base ROM hashes
    const QString readmeContent = extractReadmeFromZip(zipPath);
    if (!readmeContent.isEmpty()) {
        const auto hashes = parseReadme(readmeContent);
        entry.baseMd5 = hashes.baseMd5;
        entry.baseCrc32 = hashes.baseCrc32;
    }

    // If region/language available, add to description
    QStringList descParts;
    if (!parsed.region.isEmpty()) descParts << parsed.region;
    if (!parsed.language.isEmpty()) descParts << parsed.language;
    if (!descParts.isEmpty()) {
        entry.description = descParts.join(QStringLiteral(", "));
    }

    return entry;
}

ModEntry RAPatchesCatalogBuilder::buildEntryFromPatch(const QString &patchPath,
                                                       const QString &system,
                                                       const QString &type) const
{
    ModEntry entry;
    const QFileInfo fi(patchPath);

    const auto parsed = parseFilename(fi.fileName());
    entry.id = generateId(system, type, fi.fileName());
    entry.title = parsed.title;
    entry.author = parsed.author;
    entry.version = parsed.version;
    entry.system = system;
    entry.type = type;
    entry.format = parsed.format;
    entry.patchSize = fi.size();
    entry.sourceUrl = QStringLiteral("https://github.com/RetroAchievements/RAPatches");

    QStringList descParts;
    if (!parsed.region.isEmpty()) descParts << parsed.region;
    if (!parsed.language.isEmpty()) descParts << parsed.language;
    if (!descParts.isEmpty()) {
        entry.description = descParts.join(QStringLiteral(", "));
    }

    return entry;
}

// ============================================================================
// Catalog JSON output
// ============================================================================

QString RAPatchesCatalogBuilder::writeCatalogJson(const QList<ModEntry> &mods,
                                                   const QString &outputPath)
{
    QJsonArray modsArray;
    for (const ModEntry &mod : mods) {
        QJsonObject obj;
        obj[QStringLiteral("id")]          = mod.id;
        obj[QStringLiteral("title")]       = mod.title;
        obj[QStringLiteral("author")]      = mod.author;
        obj[QStringLiteral("version")]     = mod.version;
        obj[QStringLiteral("description")] = mod.description;
        obj[QStringLiteral("type")]        = mod.type;
        obj[QStringLiteral("system")]      = mod.system;
        obj[QStringLiteral("format")]      = mod.format;
        obj[QStringLiteral("patch_url")]   = mod.patchUrl;
        obj[QStringLiteral("patch_sha1")]  = mod.patchSha1;
        obj[QStringLiteral("patch_size")]  = mod.patchSize;
        obj[QStringLiteral("source_url")]  = mod.sourceUrl;
        obj[QStringLiteral("rating")]      = mod.rating;
        obj[QStringLiteral("downloads")]   = mod.downloads;

        QJsonObject hashes;
        hashes[QStringLiteral("crc32")] = mod.baseCrc32;
        hashes[QStringLiteral("md5")]   = mod.baseMd5;
        hashes[QStringLiteral("sha1")]  = mod.baseSha1;
        obj[QStringLiteral("base_rom_hashes")] = hashes;

        modsArray.append(obj);
    }

    QJsonObject root;
    root[QStringLiteral("catalog_version")] = 1;
    root[QStringLiteral("source")] = QStringLiteral("RAPatches");
    root[QStringLiteral("mods")] = modsArray;

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QStringLiteral("Cannot open output file: ") + outputPath;
    }

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return {};
}

} // namespace Remus
