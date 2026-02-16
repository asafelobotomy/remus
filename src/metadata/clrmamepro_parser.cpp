#include "clrmamepro_parser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

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

QList<ClrMameProEntry> ClrMameProParser::parseGameBlocks(const QString &content)
{
    QList<ClrMameProEntry> entries;
    
    qDebug() << "ClrMameProParser: Content length:" << content.length();
    
    // Match game blocks: game (\n ... \n)
    // The closing paren is on its own line
    QRegularExpression gameRegex(R"(game\s*\(([^{}]*?)\n\s*\))");
    gameRegex.setPatternOptions(QRegularExpression::DotMatchesEverythingOption);
    
    qDebug() << "ClrMameProParser: Regex pattern:" << gameRegex.pattern();
    qDebug() << "ClrMameProParser: Searching for game blocks...";
    
    QRegularExpressionMatchIterator it = gameRegex.globalMatch(content);
    
    int matchCount = 0;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        matchCount++;
        QString gameBlock = match.captured(1);
        
        // Parse game metadata (multi-line format)
        QMap<QString, QString> gameData = extractKeyValues(gameBlock);
        
        // Extract ROM block: rom ( ... )
        // ROM blocks are single-line with space-separated attributes
        int romStart = gameBlock.indexOf("rom (");
        
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
                
                if (!entry.gameName.isEmpty() && !entry.crc32.isEmpty()) {
                    entries.append(entry);
                    
                    if (matchCount <= 3) {
                        qDebug() << "ClrMameProParser: Entry" << matchCount 
                                 << "game:" << entry.gameName.left(30)
                                 << "crc32:" << entry.crc32
                                 << "size:" << entry.size;
                    }
                }
            }
        }
    }
    
    qDebug() << "ClrMameProParser: Found" << matchCount << "game blocks, parsed" << entries.size() << "entries";
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
