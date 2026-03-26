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

        // Extract CRC
        auto crcMatch = crcRegex.match(block);
        if (!crcMatch.hasMatch())
            continue;
        QString crc = crcMatch.captured(1).toUpper();

        // Extract the metadata value based on type
        LibretroMetadata &meta = m_index[crc];

        if (type == "genre") {
            auto m = genreRegex.match(block);
            if (m.hasMatch()) {
                meta.genre = m.captured(1);
                parsed++;
            }
        } else if (type == "developer") {
            auto m = developerRegex.match(block);
            if (m.hasMatch()) {
                meta.developer = m.captured(1);
                parsed++;
            }
        } else if (type == "publisher") {
            auto m = publisherRegex.match(block);
            if (m.hasMatch()) {
                meta.publisher = m.captured(1);
                parsed++;
            }
        } else if (type == "maxusers") {
            auto m = usersRegex.match(block);
            if (m.hasMatch()) {
                meta.maxUsers = m.captured(1).toInt();
                parsed++;
            }
        } else if (type == "releaseyear") {
            auto m = yearRegex.match(block);
            if (m.hasMatch()) {
                meta.releaseYear = m.captured(1).toInt();
                parsed++;
            }
        }
    }

    return parsed;
}

LibretroMetadata LibretroMetadataParser::lookup(const QString &crc32) const
{
    return m_index.value(crc32.toUpper());
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
}

} // namespace Remus
