#include "clrmamepro_parser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

namespace Remus {

QList<ClrMameProEntry> ClrMameProParser::parse(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "ClrMameProParser: Failed to open file:" << filePath;
        return QList<ClrMameProEntry>();
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    return parseGameBlocks(content);
}

QMap<QString, QString> ClrMameProParser::parseHeader(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QMap<QString, QString>();
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    QMap<QString, QString> header;
    
    // Extract clrmamepro header block
    QRegularExpression headerRegex(R"(clrmamepro\s*\(([\s\S]*?)\n\))");
    QRegularExpressionMatch match = headerRegex.match(content);
    
    if (match.hasMatch()) {
        QString headerBlock = match.captured(1);
        header = extractKeyValues(headerBlock);
    }
    
    return header;
}

QList<ClrMameProEntry> ClrMameProParser::parseAll(const QString &filePath,
                                                   QMap<QString, QString> &outHeader)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QTextStream in(&file);
    const QString content = in.readAll();
    file.close();

    // Parse header in the same pass
    QRegularExpression headerRegex(R"(clrmamepro\s*\(([\s\S]*?)\n\))");
    const QRegularExpressionMatch m = headerRegex.match(content);
    if (m.hasMatch()) {
        outHeader = extractKeyValues(m.captured(1));
    }

    return parseGameBlocks(content);
}

QList<ClrMameProEntry> ClrMameProParser::parseGameBlocks(const QString &content)
{
    QList<ClrMameProEntry> entries;
    
    // Find game blocks by matching balanced parentheses.
    // A simple regex cannot handle multi-line rom() sub-blocks
    // (e.g., GameCube/Wii DATs) because the inner closing paren on its
    // own line gets consumed first.
    QList<QString> gameBlocks;
    int searchFrom = 0;
    while (true) {
        int gameIdx = content.indexOf(QStringLiteral("game"), searchFrom);
        if (gameIdx < 0) break;

        // Find the opening paren after "game"
        int openParen = content.indexOf(QLatin1Char('('), gameIdx + 4);
        if (openParen < 0) break;

        // Balance parentheses to find the matching close
        int depth = 1;
        bool inQuote = false;
        int i = openParen + 1;
        for (; i < content.length() && depth > 0; ++i) {
            QChar c = content[i];
            if (c == QLatin1Char('"')) {
                inQuote = !inQuote;
            } else if (!inQuote) {
                if (c == QLatin1Char('(')) ++depth;
                else if (c == QLatin1Char(')')) --depth;
            }
        }

        if (depth == 0) {
            // i is one past the closing paren
            gameBlocks.append(content.mid(openParen + 1, i - openParen - 2));
        }
        searchFrom = i;
    }

    int matchCount = 0;
    for (const QString &gameBlock : gameBlocks) {
        matchCount++;
        
        // Extract ROM block: rom ( ... )
        int romStart = gameBlock.indexOf("rom (");
        
        // Parse game metadata from the portion BEFORE the rom sub-block
        // so that rom-level keys (e.g. "name") don't overwrite game-level keys.
        QString gameMetadata = (romStart != -1) ? gameBlock.left(romStart) : gameBlock;
        QMap<QString, QString> gameData = extractKeyValues(gameMetadata);
        
        if (romStart != -1) {
            int parenCount = 0;
            int i = romStart + 5; // Start after "rom ("
            int romEnd = -1;
            
            bool inQuote = false;
            for (; i < gameBlock.length(); i++) {
                QChar c = gameBlock[i];
                
                if (c == '"') {
                    inQuote = !inQuote;
                } else if (!inQuote) {
                    if (c == '(') {
                        parenCount++;
                    } else if (c == ')') {
                        if (parenCount == 0) {
                            romEnd = i;
                            break;
                        }
                        parenCount--;
                    }
                }
            }
            
            if (romEnd != -1) {
                QString romBlock = gameBlock.mid(romStart + 5, romEnd - romStart - 5).trimmed();
                QMap<QString, QString> romData = parseInlineAttributes(romBlock);
                
                // Create entry
                ClrMameProEntry entry;
                entry.gameName = gameData.value("name");
                entry.description = gameData.value("description", gameData.value("name"));
                entry.serial = gameData.value("serial");
                entry.romName = romData.value("name");
                entry.size = romData.value("size").toLongLong();
                entry.crc32 = romData.value("crc").toUpper();
                entry.md5 = romData.value("md5").toLower();
                entry.sha1 = romData.value("sha1").toLower();

                // Inline metadata from Redump/GameTDB DATs
                entry.publisher = gameData.value("publisher");
                entry.developer = gameData.value("developer");
                entry.releaseYear = gameData.value("releaseyear").toInt();
                entry.users = gameData.value("users").toInt();
                
                // Use game-level region if present, otherwise extract from name
                entry.region = gameData.value("region");
                if (entry.region.isEmpty()) {
                    QRegularExpression regionRegex(R"(\(([^)]+)\))");
                    QRegularExpressionMatch regionMatch = regionRegex.match(entry.gameName);
                    if (regionMatch.hasMatch()) {
                        QString regionText = regionMatch.captured(1);
                        // Take first region if comma-separated
                        if (regionText.contains(',')) {
                            entry.region = regionText.split(',').first().trimmed();
                        } else {
                            entry.region = regionText.trimmed();
                        }
                    }
                }
                
                // Also pull serial from rom block if game-level serial is empty
                if (entry.serial.isEmpty()) {
                    entry.serial = romData.value("serial");
                }

                // Accept entries with either a hash or a serial for identification
                const bool hasHash = !entry.crc32.isEmpty() || !entry.md5.isEmpty() || !entry.sha1.isEmpty();
                const bool hasSerial = !entry.serial.isEmpty();
                if (!entry.gameName.isEmpty() && (hasHash || hasSerial)) {
                    entries.append(entry);
                }
            }
        }
    }
    
    return entries;
}

QMap<QString, QString> ClrMameProParser::extractKeyValues(const QString &block)
{
    QMap<QString, QString> data;
    
    // Match key-value pairs: key "value" or key value
    QRegularExpression kvRegex(R"((\w+)\s+([^\n]+))");
    QRegularExpressionMatchIterator it = kvRegex.globalMatch(block);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString key = match.captured(1).trimmed();
        QString value = match.captured(2).trimmed();
        
        // Remove quotes if present
        if (value.startsWith('"') && value.endsWith('"')) {
            value = value.mid(1, value.length() - 2);
        }
        
        data[key] = value;
    }
    
    return data;
}

QMap<QString, QString> ClrMameProParser::parseInlineAttributes(const QString &line)
{
    QMap<QString, QString> data;
    
    int i = 0;
    while (i < line.length()) {
        // Skip whitespace
        while (i < line.length() && line[i].isSpace()) {
            i++;
        }
        
        if (i >= line.length()) break;
        
        // Extract key (alphanumeric word)
        int keyStart = i;
        while (i < line.length() && (line[i].isLetterOrNumber() || line[i] == '_')) {
            i++;
        }
        QString key = line.mid(keyStart, i - keyStart);
        
        // Skip whitespace after key
        while (i < line.length() && line[i].isSpace()) {
            i++;
        }
        
        if (i >= line.length()) break;
        
        // Extract value (quoted string or unquoted word)
        QString value;
        if (line[i] == '"') {
            // Quoted string
            i++; // Skip opening quote
            int valueStart = i;
            while (i < line.length() && line[i] != '"') {
                i++;
            }
            value = line.mid(valueStart, i - valueStart);
            if (i < line.length()) i++; // Skip closing quote
        } else {
            // Unquoted value (number or hex)
            int valueStart = i;
            while (i < line.length() && !line[i].isSpace()) {
                i++;
            }
            value = line.mid(valueStart, i - valueStart);
        }
        
        if (!key.isEmpty()) {
            data[key] = value;
        }
    }
    
    return data;
}

QString ClrMameProParser::extractQuoted(const QString &text)
{
    QString trimmed = text.trimmed();
    if (trimmed.startsWith('"') && trimmed.endsWith('"')) {
        return trimmed.mid(1, trimmed.length() - 2);
    }
    return trimmed;
}

} // namespace Remus
