#include "clrmamepro_parser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

namespace Remus {

QList<ClrMameProEntry> ClrMameProParser::parse(const QString &filePath) {
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

QMap<QString, QString> ClrMameProParser::parseHeader(const QString &filePath) {
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

QList<ClrMameProEntry> ClrMameProParser::parseAll(const QString &filePath, QMap<QString, QString> &outHeader) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return { };
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

QList<ClrMameProEntry> ClrMameProParser::parseGameBlocks(const QString &content) {
    QList<ClrMameProEntry> entries;

    // Find game blocks by matching balanced parentheses.
    // A simple regex cannot handle multi-line rom() sub-blocks
    // (e.g., GameCube/Wii DATs) because the inner closing paren on its
    // own line gets consumed first.
    QList<QString> gameBlocks;
    int searchFrom = 0;
    while (true) {
        int gameIdx = content.indexOf(QStringLiteral("game"), searchFrom);
        if (gameIdx < 0)
            break;

        // Find the opening paren after "game"
        int openParen = content.indexOf(QLatin1Char('('), gameIdx + 4);
        if (openParen < 0)
            break;

        // Balance parentheses to find the matching close
        int depth = 1;
        bool inQuote = false;
        int i = openParen + 1;
        for (; i < content.length() && depth > 0; ++i) {
            QChar c = content[i];
            if (c == QLatin1Char('"')) {
                inQuote = !inQuote;
            } else if (!inQuote) {
                if (c == QLatin1Char('('))
                    ++depth;
                else if (c == QLatin1Char(')'))
                    --depth;
            }
        }

        if (depth == 0) {
            // i is one past the closing paren
            gameBlocks.append(content.mid(openParen + 1, i - openParen - 2));
        }
        searchFrom = i;
    }

    for (const QString &gameBlock : gameBlocks) {
        // Find the first rom block to determine where game-level metadata ends.
        const int firstRomStart = gameBlock.indexOf(QStringLiteral("rom ("));
        const QString gameMetadata = (firstRomStart != -1) ? gameBlock.left(firstRomStart) : gameBlock;
        const QMap<QString, QString> gameData = extractKeyValues(gameMetadata);

        // Derive region once from game-level data or from the title parenthetical.
        QString baseRegion = gameData.value(QStringLiteral("region"));
        if (baseRegion.isEmpty()) {
            const QString gameName = gameData.value(QStringLiteral("name"));
            static const QRegularExpression regionRegex(R"(\(([^)]+)\))");
            const QRegularExpressionMatch rm = regionRegex.match(gameName);
            if (rm.hasMatch()) {
                const QString regionText = rm.captured(1);
                baseRegion = regionText.contains(QLatin1Char(','))
                    ? regionText.split(QLatin1Char(',')).first().trimmed()
                    : regionText.trimmed();
            }
        }

        if (firstRomStart == -1)
            continue; // no rom blocks in this game

        // Iterate ALL rom ( ... ) blocks so multi-track disc games yield one
        // entry per ROM file.  The DatExtractor will then select the canonical
        // data-track entry.
        int searchPos = firstRomStart;
        while (searchPos < gameBlock.length()) {
            const int romStart = gameBlock.indexOf(QStringLiteral("rom ("), searchPos);
            if (romStart == -1)
                break;

            // Balance parentheses to find the matching closing paren.
            int parenCount = 0;
            int i = romStart + 5; // start after "rom ("
            int romEnd = -1;
            bool inQuote = false;
            for (; i < gameBlock.length(); ++i) {
                const QChar c = gameBlock[i];
                if (c == QLatin1Char('"')) {
                    inQuote = !inQuote;
                } else if (!inQuote) {
                    if (c == QLatin1Char('('))
                        ++parenCount;
                    else if (c == QLatin1Char(')')) {
                        if (parenCount == 0) {
                            romEnd = i;
                            break;
                        }
                        --parenCount;
                    }
                }
            }

            if (romEnd == -1)
                break; // malformed block — stop

            const QString romBlock = gameBlock.mid(romStart + 5, romEnd - romStart - 5).trimmed();
            const QMap<QString, QString> romData = parseInlineAttributes(romBlock);

            ClrMameProEntry entry;
            entry.gameName = gameData.value(QStringLiteral("name"));
            entry.description = gameData.value(QStringLiteral("description"), gameData.value(QStringLiteral("name")));
            entry.serial = gameData.value(QStringLiteral("serial"));
            entry.publisher = gameData.value(QStringLiteral("publisher"));
            entry.developer = gameData.value(QStringLiteral("developer"));
            entry.releaseYear = gameData.value(QStringLiteral("releaseyear")).toInt();
            entry.releaseMonth = gameData.value(QStringLiteral("releasemonth")).toInt();
            entry.releaseDay = gameData.value(QStringLiteral("releaseday")).toInt();
            entry.users = gameData.value(QStringLiteral("users")).toInt();
            entry.region = baseRegion;

            entry.romName = romData.value(QStringLiteral("name"));
            entry.size = romData.value(QStringLiteral("size")).toLongLong();
            entry.crc32 = romData.value(QStringLiteral("crc")).toUpper();
            entry.md5 = romData.value(QStringLiteral("md5")).toLower();
            entry.sha1 = romData.value(QStringLiteral("sha1")).toLower();
            entry.sha256 = romData.value(QStringLiteral("sha256")).toLower();

            if (entry.serial.isEmpty())
                entry.serial = romData.value(QStringLiteral("serial"));

            const bool hasHash
                = !entry.crc32.isEmpty() || !entry.md5.isEmpty() || !entry.sha1.isEmpty() || !entry.sha256.isEmpty();
            const bool hasSerial = !entry.serial.isEmpty();
            if (!entry.gameName.isEmpty() && (hasHash || hasSerial))
                entries.append(entry);

            searchPos = romEnd + 1;
        }
    }

    return entries;
}

QMap<QString, QString> ClrMameProParser::extractKeyValues(const QString &block) {
    QMap<QString, QString> data;

    // Match key-value pairs: key "value" or key value
    static const QRegularExpression kvRegex(R"((\w+)\s+([^\n]+))");
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

QMap<QString, QString> ClrMameProParser::parseInlineAttributes(const QString &line) {
    QMap<QString, QString> data;

    int i = 0;
    while (i < line.length()) {
        // Skip whitespace
        while (i < line.length() && line[i].isSpace()) {
            i++;
        }

        if (i >= line.length())
            break;

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

        if (i >= line.length())
            break;

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
            if (i < line.length())
                i++; // Skip closing quote
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

QString ClrMameProParser::extractQuoted(const QString &text) {
    QString trimmed = text.trimmed();
    if (trimmed.startsWith('"') && trimmed.endsWith('"')) {
        return trimmed.mid(1, trimmed.length() - 2);
    }
    return trimmed;
}

} // namespace Remus
