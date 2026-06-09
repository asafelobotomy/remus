#include "compendium_dat_extractor.h"

#include "../core/dat_parser.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace Remus {
namespace Compendium {

    // ── Helpers ───────────────────────────────────────────────────────────────────

    QString DatExtractor::normalizeHash(const QString &raw) {
        return raw.trimmed().toUpper().remove(QLatin1Char(' '));
    }

    QString DatExtractor::normalizeSerial(const QString &raw) {
        return raw.trimmed().toUpper();
    }

    // Returns true for file extensions that are metadata-only tracks (.cue, .m3u,
    // etc.) — these should not be used as the canonical ROM identity entry.
    static bool isMetaTrack(const QString &romName) {
        static const QStringList kMetaExtensions = {
            QStringLiteral(".cue"),
            QStringLiteral(".m3u"),
            QStringLiteral(".gdi"),
            QStringLiteral(".toc"),
            QStringLiteral(".sbi"),
            QStringLiteral(".sub"),
            QStringLiteral(".ccd"),
            QStringLiteral(".mds"),
        };
        const QString lower = romName.toLower();
        for (const QString &ext : kMetaExtensions) {
            if (lower.endsWith(ext))
                return true;
        }
        return false;
    }

    // Given a list of entries that all share the same gameName (i.e., multiple
    // rom-track entries from one game block), select the canonical data-track entry.
    // Prefers the first non-meta-track; falls back to the first entry overall.
    static ClrMameProEntry selectDataTrack(const QList<ClrMameProEntry> &group) {
        for (const ClrMameProEntry &e : group) {
            if (!isMetaTrack(e.romName))
                return e;
        }
        return group.first();
    }

    // Convert a Logiqx DatRomEntry to a ClrMameProEntry so XML-format DATs can
    // feed the same downstream pipeline.
    static ClrMameProEntry datRomEntryToClrMame(const Remus::DatRomEntry &src) {
        ClrMameProEntry entry;
        entry.gameName = src.gameName;
        entry.description = src.description;
        entry.romName = src.romName;
        entry.size = src.size;
        entry.crc32 = src.crc32.toUpper();
        entry.md5 = src.md5.toLower();
        entry.sha1 = src.sha1.toLower();
        entry.sha256 = src.sha256.toLower();
        entry.serial = src.serial;
        return entry;
    }

    QString DatExtractor::makeExternalKey(const QString &systemHint, const ClrMameProEntry &entry) {
        const QString key = systemHint + QLatin1Char('|') + entry.gameName + QLatin1Char('|') + entry.romName;
        return key.left(512);
    }

    QString DatExtractor::entryToPayloadJson(const QString &systemHint, const ClrMameProEntry &entry) {
        QJsonObject obj;
        obj.insert(QStringLiteral("system_hint"), systemHint);
        obj.insert(QStringLiteral("game_name"), entry.gameName);
        obj.insert(QStringLiteral("description"), entry.description);
        obj.insert(QStringLiteral("rom_name"), entry.romName);
        obj.insert(QStringLiteral("region"), entry.region);
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
        if (!entry.sha256.isEmpty()) {
            obj.insert(QStringLiteral("sha256"), entry.sha256);
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

    QList<SourceRecordEnvelope> DatExtractor::extract(
        const QString &filePath, const QString &sourceId, const QString &snapshotId, QString &error) {
        const QFileInfo info(filePath);
        if (!info.exists() || !info.isFile()) {
            error = QStringLiteral("DAT file not found: %1").arg(filePath);
            return { };
        }

        // Parse header and all entries in one file pass
        QMap<QString, QString> header;
        QList<ClrMameProEntry> entries = ClrMameProParser::parseAll(filePath, header);
        QString systemHint = header.value(QStringLiteral("name"), info.completeBaseName());

        if (entries.isEmpty()) {
            // XML fallback: try Logiqx XML format (used by Redump, No-Intro XML exports)
            QFile peekFile(filePath);
            bool isXml = false;
            if (peekFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QByteArray peek = peekFile.read(6);
                peekFile.close();
                isXml = peek.startsWith("<?xml") || peek.startsWith("<dataf");
            }

            if (isXml) {
                Remus::DatParser xmlParser;
                const Remus::DatParseResult xmlResult = xmlParser.parse(filePath);
                if (!xmlResult.success || xmlResult.entries.isEmpty()) {
                    error = QStringLiteral("DAT file produced no entries: %1").arg(filePath);
                    return { };
                }
                if (!xmlResult.header.name.isEmpty())
                    systemHint = xmlResult.header.name;
                entries.reserve(xmlResult.entries.size());
                for (const Remus::DatRomEntry &re : xmlResult.entries)
                    entries.append(datRomEntryToClrMame(re));
            } else {
                error = QStringLiteral("DAT file produced no entries: %1").arg(filePath);
                return { };
            }
        }

        // Group entries by gameName and select the canonical data-track per game.
        // For single-entry games this is a no-op; for multi-track disc games
        // (Redump PS1/PS2/Saturn) it discards .cue/.m3u metadata entries.
        QMap<QString, QList<ClrMameProEntry>> groups;
        for (const ClrMameProEntry &e : entries)
            groups[e.gameName].append(e);

        QList<ClrMameProEntry> canonical;
        canonical.reserve(groups.size());
        for (auto it = groups.cbegin(), end = groups.cend(); it != end; ++it)
            canonical.append(it->size() == 1 ? it->first() : selectDataTrack(*it));

        QList<SourceRecordEnvelope> records;
        records.reserve(canonical.size());

        for (const ClrMameProEntry &entry : canonical) {
            SourceRecordEnvelope rec;

            rec.sourceId = sourceId;
            rec.snapshotId = snapshotId;
            rec.externalKey = makeExternalKey(systemHint, entry);

            rec.systemHint = systemHint;
            rec.titleRaw = entry.gameName;
            rec.regionRaw = entry.region;

            // Normalized hashes
            rec.hashes.crc32 = normalizeHash(entry.crc32);
            rec.hashes.md5 = entry.md5.trimmed().toLower();
            rec.hashes.sha1 = entry.sha1.trimmed().toLower();
            rec.hashes.sha256 = entry.sha256.trimmed().toLower();

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
                rec.fields.insert(QStringLiteral("release_year"), QString::number(entry.releaseYear));
                const int month = entry.releaseMonth > 0 ? entry.releaseMonth : 1;
                const int day = entry.releaseDay > 0 ? entry.releaseDay : 1;
                rec.fields.insert(QStringLiteral("release_date"),
                    QStringLiteral("%1-%2-%3")
                        .arg(entry.releaseYear, 4, 10, QChar('0'))
                        .arg(month, 2, 10, QChar('0'))
                        .arg(day, 2, 10, QChar('0')));
            }
            if (entry.users > 0) {
                rec.fields.insert(QStringLiteral("players_max"), QString::number(entry.users));
            }
            // Require a minimum length to filter out machine-type codes (e.g. MAME
            // driver labels like "PC", "XT") that are too short to be useful descriptions.
            if (!entry.description.isEmpty() && entry.description != entry.gameName
                && entry.description.length() >= 20) {
                rec.fields.insert(QStringLiteral("description"), entry.description);
            }

            rec.payloadJson = entryToPayloadJson(systemHint, entry);

            records.append(rec);
        }

        return records;
    }

} // namespace Compendium
} // namespace Remus
