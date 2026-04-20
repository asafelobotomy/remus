#include "compendium_dat_extractor.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace Remus {
namespace Compendium {

// ── Helpers ───────────────────────────────────────────────────────────────────

QString DatExtractor::normalizeHash(const QString &raw)
{
    return raw.trimmed().toUpper().remove(QLatin1Char(' '));
}

QString DatExtractor::normalizeSerial(const QString &raw)
{
    return raw.trimmed().toUpper();
}

QString DatExtractor::makeExternalKey(const QString &systemHint,
                                      const ClrMameProEntry &entry)
{
    const QString key = systemHint + QLatin1Char('|')
                        + entry.gameName + QLatin1Char('|')
                        + entry.romName;
    return key.left(512);
}

QString DatExtractor::entryToPayloadJson(const QString &systemHint,
                                         const ClrMameProEntry &entry)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("system_hint"), systemHint);
    obj.insert(QStringLiteral("game_name"),   entry.gameName);
    obj.insert(QStringLiteral("description"), entry.description);
    obj.insert(QStringLiteral("rom_name"),    entry.romName);
    obj.insert(QStringLiteral("region"),      entry.region);
    if (entry.size > 0) {
        obj.insert(QStringLiteral("size"), static_cast<qint64>(entry.size));
    }
    if (!entry.crc32.isEmpty()) {
        obj.insert(QStringLiteral("crc32"), entry.crc32);
    }
    if (!entry.md5.isEmpty()) {
        obj.insert(QStringLiteral("md5"), entry.md5);
    }
    if (!entry.sha1.isEmpty()) {
        obj.insert(QStringLiteral("sha1"), entry.sha1);
    }
    if (!entry.serial.isEmpty()) {
        obj.insert(QStringLiteral("serial"), entry.serial);
    }
    if (!entry.publisher.isEmpty()) {
        obj.insert(QStringLiteral("publisher"), entry.publisher);
    }
    if (!entry.developer.isEmpty()) {
        obj.insert(QStringLiteral("developer"), entry.developer);
    }
    if (entry.releaseYear > 0) {
        obj.insert(QStringLiteral("release_year"), entry.releaseYear);
    }
    if (entry.users > 0) {
        obj.insert(QStringLiteral("users"), entry.users);
    }
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

// ── Main extraction ───────────────────────────────────────────────────────────

QList<SourceRecordEnvelope> DatExtractor::extract(const QString &filePath,
                                                   const QString &sourceId,
                                                   const QString &snapshotId,
                                                   QString &error)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile()) {
        error = QStringLiteral("DAT file not found: %1").arg(filePath);
        return {};
    }

    // Parse the DAT header to get the canonical system name
    const QMap<QString, QString> header = ClrMameProParser::parseHeader(filePath);
    const QString systemHint = header.value(QStringLiteral("name"),
                                            info.completeBaseName());

    // Parse all entries
    const QList<ClrMameProEntry> entries = ClrMameProParser::parse(filePath);
    if (entries.isEmpty()) {
        error = QStringLiteral("DAT file produced no entries: %1").arg(filePath);
        return {};
    }

    QList<SourceRecordEnvelope> records;
    records.reserve(entries.size());

    for (const ClrMameProEntry &entry : entries) {
        SourceRecordEnvelope rec;

        rec.sourceId   = sourceId;
        rec.snapshotId = snapshotId;
        rec.externalKey = makeExternalKey(systemHint, entry);

        rec.systemHint = systemHint;
        rec.titleRaw   = entry.gameName;
        rec.regionRaw  = entry.region;

        // Normalized hashes
        rec.hashes.crc32 = normalizeHash(entry.crc32);
        rec.hashes.md5   = entry.md5.trimmed().toLower();
        rec.hashes.sha1  = entry.sha1.trimmed().toLower();

        // Normalized serials
        if (!entry.serial.isEmpty()) {
            rec.serials.append(normalizeSerial(entry.serial));
        }

        // Candidate metadata facts
        if (!entry.gameName.isEmpty()) {
            rec.fields.insert(QStringLiteral("title"), entry.gameName);
        }
        if (!entry.region.isEmpty()) {
            rec.fields.insert(QStringLiteral("region"), entry.region);
        }
        if (!entry.publisher.isEmpty()) {
            rec.fields.insert(QStringLiteral("publisher"), entry.publisher);
        }
        if (!entry.developer.isEmpty()) {
            rec.fields.insert(QStringLiteral("developer"), entry.developer);
        }
        if (entry.releaseYear > 0) {
            rec.fields.insert(QStringLiteral("release_year"),
                              QString::number(entry.releaseYear));
        }
        if (entry.users > 0) {
            rec.fields.insert(QStringLiteral("players_max"),
                              QString::number(entry.users));
        }
        if (!entry.description.isEmpty()
                && entry.description != entry.gameName) {
            rec.fields.insert(QStringLiteral("description"), entry.description);
        }

        rec.payloadJson = entryToPayloadJson(systemHint, entry);

        records.append(rec);
    }

    return records;
}

} // namespace Compendium
} // namespace Remus
