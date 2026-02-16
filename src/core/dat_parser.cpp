#include "dat_parser.h"
#include <QFile>
#include <QXmlStreamReader>
#include <QDebug>
#include <QFileInfo>

namespace Remus {

DatParser::DatParser(QObject *parent)
    : QObject(parent)
{
}

DatParseResult DatParser::parse(const QString &filePath)
{
    DatParseResult result;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = QString("Failed to open DAT file: %1").arg(filePath);
        emit parseError(result.error);
        return result;
    }

    QString content = file.readAll();
    file.close();

    return parseContent(content);
}

DatParseResult DatParser::parseContent(const QString &content)
{
    DatParseResult result;
    QXmlStreamReader xml(content);

    // Skip to root element
    while (!xml.atEnd() && !xml.isStartElement()) {
        xml.readNext();
    }

    if (xml.hasError()) {
        result.error = QString("XML parse error: %1 at line %2")
            .arg(xml.errorString())
            .arg(xml.lineNumber());
        emit parseError(result.error);
        return result;
    }

    // Expect <datafile> root element
    if (xml.name() != QString("datafile")) {
        // Try to continue anyway - some DAT files have different root elements
        qWarning() << "Expected <datafile> root, found:" << xml.name();
    }

    xml.readNext();

    while (!xml.atEnd()) {
        if (xml.isStartElement()) {
            if (xml.name() == QString("header")) {
                if (!parseHeader(xml, result.header)) {
                    qWarning() << "Failed to parse header";
                }
            } else if (xml.name() == QString("game") || xml.name() == QString("machine")) {
                if (!parseGame(xml, result.entries)) {
                    qWarning() << "Failed to parse game entry";
                }
                emit parseProgress(result.entries.size(), 0);
            }
        }
        xml.readNext();
    }

    if (xml.hasError()) {
        result.error = QString("XML parse error: %1 at line %2")
            .arg(xml.errorString())
            .arg(xml.lineNumber());
        emit parseError(result.error);
        return result;
    }

    result.entryCount = result.entries.size();
    result.success = true;

    qInfo() << "Parsed DAT file:" << result.header.name 
            << "with" << result.entryCount << "entries";

    return result;
}

bool DatParser::parseHeader(QXmlStreamReader &xml, DatHeader &header)
{
    xml.readNext();
    
    while (!xml.atEnd()) {
        if (xml.isEndElement() && xml.name() == QString("header")) {
            return true;
        }

        if (xml.isStartElement()) {
            QString elementName = xml.name().toString();
            QString text = xml.readElementText();

            if (elementName == "name") {
                header.name = text;
            } else if (elementName == "description") {
                header.description = text;
            } else if (elementName == "version") {
                header.version = text;
            } else if (elementName == "author") {
                header.author = text;
            } else if (elementName == "category") {
                header.category = text;
            } else if (elementName == "url") {
                header.url = text;
            } else if (elementName == "date") {
                header.date = QDateTime::fromString(text, Qt::ISODate);
                if (!header.date.isValid()) {
                    // Try alternative format: "20240115" or "2024-01-15"
                    header.date = QDateTime::fromString(text, "yyyyMMdd");
                }
            }
        }
        xml.readNext();
    }

    return true;
}

bool DatParser::parseGame(QXmlStreamReader &xml, QList<DatRomEntry> &entries)
{
    DatRomEntry baseEntry;
    
    // Get game name from attribute
    QXmlStreamAttributes attrs = xml.attributes();
    baseEntry.gameName = attrs.value("name").toString();

    xml.readNext();

    while (!xml.atEnd()) {
        if (xml.isEndElement() && 
            (xml.name() == QString("game") || xml.name() == QString("machine"))) {
            return true;
        }

        if (xml.isStartElement()) {
            QString elementName = xml.name().toString();

            if (elementName == "description") {
                baseEntry.description = xml.readElementText();
            } else if (elementName == "rom" || elementName == "disk") {
                // Parse ROM/disk entry
                DatRomEntry romEntry = baseEntry;
                QXmlStreamAttributes romAttrs = xml.attributes();

                romEntry.romName = romAttrs.value("name").toString();
                
                QString sizeStr = romAttrs.value("size").toString();
                if (!sizeStr.isEmpty()) {
                    romEntry.size = sizeStr.toLongLong();
                }

                romEntry.crc32 = normalizeHash(romAttrs.value("crc").toString());
                romEntry.md5 = normalizeHash(romAttrs.value("md5").toString());
                romEntry.sha1 = normalizeHash(romAttrs.value("sha1").toString());
                romEntry.status = romAttrs.value("status").toString();
                romEntry.serial = romAttrs.value("serial").toString();

                entries.append(romEntry);
            }
        }
        xml.readNext();
    }

    return true;
}

QString DatParser::normalizeHash(const QString &hash)
{
    // Convert to lowercase and remove any whitespace
    return hash.toLower().trimmed();
}

QMap<QString, DatRomEntry> DatParser::indexByHash(
    const QList<DatRomEntry> &entries, 
    const QString &hashType)
{
    QMap<QString, DatRomEntry> index;

    for (const DatRomEntry &entry : entries) {
        QString hash;
        
        if (hashType == "crc32" || hashType == "crc") {
            hash = entry.crc32;
        } else if (hashType == "md5") {
            hash = entry.md5;
        } else if (hashType == "sha1") {
            hash = entry.sha1;
        }

        if (!hash.isEmpty()) {
            index.insert(hash, entry);
        }
    }

    return index;
}

QString DatParser::detectSource(const DatHeader &header)
{
    QString nameLower = header.name.toLower();
    QString descLower = header.description.toLower();

    if (descLower.contains("no-intro") || nameLower.contains("no-intro")) {
        return "no-intro";
    }
    if (descLower.contains("redump") || nameLower.contains("redump")) {
        return "redump";
    }
    if (descLower.contains("tosec") || nameLower.contains("tosec")) {
        return "tosec";
    }
    if (descLower.contains("gametdb") || nameLower.contains("gametdb")) {
        return "gametdb";
    }

    return "unknown";
}

} // namespace Remus
