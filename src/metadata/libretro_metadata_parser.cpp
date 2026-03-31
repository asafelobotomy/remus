#include "libretro_metadata_parser.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QDebug>

namespace Remus {

int LibretroMetadataParser::loadAll(const QString &metadataDir)
{
    QDir dir(metadataDir);
    if (!dir.exists()) {
        qWarning() << "LibretroMetadataParser: Directory not found:" << metadataDir;
        return 0;
    }

    static const QStringList types = {
        "genre", "developer", "publisher", "maxusers", "releaseyear"
    };

    int totalParsed = 0;
    for (const QString &type : types) {
        QString subDir = dir.filePath(type);
        if (QDir(subDir).exists()) {
            int n = loadType(subDir, type);
            totalParsed += n;
            qDebug() << "LibretroMetadataParser:" << type << "->" << n << "entries";
        }
    }

    qDebug() << "LibretroMetadataParser: Loaded metadata for"
             << m_index.size() << "unique CRCs from" << totalParsed << "entries";
    return m_index.size();
}

int LibretroMetadataParser::loadType(const QString &dir, const QString &type)
{
    QDir d(dir);
    QStringList filters;
    filters << "*.dat";
    QFileInfoList files = d.entryInfoList(filters, QDir::Files);

    int total = 0;
    for (const QFileInfo &fi : files) {
        total += parseFile(fi.absoluteFilePath(), type);
    }
    return total;
}

// Merge a single type's value from src into dst
static void mergeField(LibretroMetadata &dst, const LibretroMetadata &src, const QString &type)
{
    if (type == "genre" && dst.genre.isEmpty())
        dst.genre = src.genre;
    else if (type == "developer" && dst.developer.isEmpty())
        dst.developer = src.developer;
    else if (type == "publisher" && dst.publisher.isEmpty())
        dst.publisher = src.publisher;
    else if (type == "maxusers" && dst.maxUsers == 0)
        dst.maxUsers = src.maxUsers;
    else if (type == "releaseyear" && dst.releaseYear == 0)
        dst.releaseYear = src.releaseYear;
}

// Merge all fields from src into dst (fills gaps)
static void mergeAllFields(LibretroMetadata &dst, const LibretroMetadata &src)
{
    if (dst.genre.isEmpty() && !src.genre.isEmpty())
        dst.genre = src.genre;
    if (dst.developer.isEmpty() && !src.developer.isEmpty())
        dst.developer = src.developer;
    if (dst.publisher.isEmpty() && !src.publisher.isEmpty())
        dst.publisher = src.publisher;
    if (dst.maxUsers == 0 && src.maxUsers != 0)
        dst.maxUsers = src.maxUsers;
    if (dst.releaseYear == 0 && src.releaseYear != 0)
        dst.releaseYear = src.releaseYear;
}

int LibretroMetadataParser::parseFile(const QString &filePath, const QString &type)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "LibretroMetadataParser: Cannot open" << filePath;
        return 0;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    // Match game blocks:  game ( ... )
    // Each block contains: comment, the metadata field, and rom ( crc XXXX )
    static const QRegularExpression gameRegex(
        R"(game\s*\(([^{}]*?)\n\s*\))",
        QRegularExpression::DotMatchesEverythingOption);

    // Extract CRC from rom line:  rom ( crc ABCD1234 )
    static const QRegularExpression crcRegex(
        R"(rom\s*\(\s*crc\s+([0-9A-Fa-f]+)\s*\))");

    // Extract serial from rom line:  rom ( serial SLUS-01272 ... )
    static const QRegularExpression romSerialRegex(
        R"re(rom\s*\([^)]*serial\s+"([^"]+)")re");

    // Extract game-level serial:  serial "SLUS-01272"
    static const QRegularExpression gameSerialRegex(
        R"re(serial\s+"([^"]+)")re");

    // Extract game name:  name "Silent Hill (USA)"
    static const QRegularExpression nameRegex(
        R"re(name\s+"([^"]+)")re");
    // Value extractors per type — match: fieldname "value" or fieldname value
    static const QRegularExpression genreRegex(R"re(genre\s+"([^"]+)")re");
    static const QRegularExpression developerRegex(R"re(developer\s+"([^"]+)")re");
    static const QRegularExpression publisherRegex(R"re(publisher\s+"([^"]+)")re");
    static const QRegularExpression usersRegex(R"re(users\s+(\d+))re");
    static const QRegularExpression yearRegex(R"re(releaseyear\s+"(\d+)")re");
    int parsed = 0;
    auto it = gameRegex.globalMatch(content);

    while (it.hasNext()) {
        auto match = it.next();
        QString block = match.captured(1);

        // Extract CRC (primary key for cartridge-based systems)
        auto crcMatch = crcRegex.match(block);
        QString crc;
        if (crcMatch.hasMatch()) {
            crc = crcMatch.captured(1).toUpper();
        }

        // Extract serial (primary key for disc-based systems like PSX)
        QString serial;
        auto serialMatch = romSerialRegex.match(block);
        if (serialMatch.hasMatch()) {
            serial = serialMatch.captured(1).toUpper().trimmed();
        } else {
            auto gameSerialMatch = gameSerialRegex.match(block);
            if (gameSerialMatch.hasMatch()) {
                serial = gameSerialMatch.captured(1).toUpper().trimmed();
            }
        }

        // Extract game name (fallback key)
        QString gameName;
        auto nameMatch = nameRegex.match(block);
        if (nameMatch.hasMatch()) {
            gameName = nameMatch.captured(1);
        }

        // Must have at least one key
        if (crc.isEmpty() && serial.isEmpty() && gameName.isEmpty())
            continue;

        // Parse ALL metadata fields from this block (disc system files
        // like PSXDataCenter contain all fields in a single DAT file)
        LibretroMetadata parsedMeta;
        bool found = false;

        auto gm = genreRegex.match(block);
        if (gm.hasMatch()) { parsedMeta.genre = gm.captured(1); found = true; }

        auto dm = developerRegex.match(block);
        if (dm.hasMatch()) { parsedMeta.developer = dm.captured(1); found = true; }

        auto pm = publisherRegex.match(block);
        if (pm.hasMatch()) { parsedMeta.publisher = pm.captured(1); found = true; }

        auto um = usersRegex.match(block);
        if (um.hasMatch()) { parsedMeta.maxUsers = um.captured(1).toInt(); found = true; }

        auto ym = yearRegex.match(block);
        if (ym.hasMatch()) { parsedMeta.releaseYear = ym.captured(1).toInt(); found = true; }

        if (!found)
            continue;

        // Merge into CRC index
        if (!crc.isEmpty()) {
            LibretroMetadata &meta = m_index[crc];
            mergeAllFields(meta, parsedMeta);
            parsed++;
        }

        // Merge into serial index (for disc systems without CRC)
        if (!serial.isEmpty()) {
            LibretroMetadata &meta = m_serialIndex[serial];
            mergeAllFields(meta, parsedMeta);
            if (crc.isEmpty()) parsed++; // count if not already counted via CRC
        }

        // Merge into name index (ultimate fallback)
        if (!gameName.isEmpty()) {
            LibretroMetadata &meta = m_nameIndex[gameName];
            mergeAllFields(meta, parsedMeta);
        }
    }

    return parsed;
}

LibretroMetadata LibretroMetadataParser::lookup(const QString &crc32) const
{
    return m_index.value(crc32.toUpper());
}

LibretroMetadata LibretroMetadataParser::lookupBySerial(const QString &serial) const
{
    return m_serialIndex.value(serial.toUpper().trimmed());
}

LibretroMetadata LibretroMetadataParser::lookupByName(const QString &name) const
{
    return m_nameIndex.value(name);
}

bool LibretroMetadataParser::contains(const QString &crc32) const
{
    return m_index.contains(crc32.toUpper());
}

int LibretroMetadataParser::size() const
{
    return m_index.size();
}

void LibretroMetadataParser::clear()
{
    m_index.clear();
    m_serialIndex.clear();
    m_nameIndex.clear();
}

} // namespace Remus
