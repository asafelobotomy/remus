#include "compendium_identity_linker.h"

#include <QCryptographicHash>
#include <QRegularExpression>
#include <QSet>

namespace Remus {
namespace Compendium {

// ── Helpers ───────────────────────────────────────────────────────────────────

QString IdentityLinker::generateGameId(const QString &seed)
{
    const QByteArray hash = QCryptographicHash::hash(
        seed.toUtf8(), QCryptographicHash::Sha1);
    return QString::fromLatin1(hash.toHex().left(16));
}

QString IdentityLinker::normalizeTitle(const QString &raw)
{
    // Lowercase, strip leading "the "/"a ", collapse spaces, strip punctuation.
    QString s = raw.toLower().trimmed();

    static const QRegularExpression rePunct(QStringLiteral("[^a-z0-9 ]"));
    s.remove(rePunct);

    static const QRegularExpression reSpaces(QStringLiteral(" {2,}"));
    s.replace(reSpaces, QStringLiteral(" "));

    if (s.startsWith(QStringLiteral("the "))) {
        s = s.mid(4);
    }
    if (s.startsWith(QStringLiteral("a "))) {
        s = s.mid(2);
    }

    return s.trimmed();
}

// ── Main linker ───────────────────────────────────────────────────────────────

int IdentityLinker::link(QList<SourceRecordEnvelope> &records) const
{
    // Maps used to accumulate game IDs across passes.
    // key → assigned game_id
    QMap<QString, QString> sha1ToId;
    QMap<QString, QString> md5ToId;
    QMap<QString, QString> crc32ToId;
    // "<systemId>|<normalizedTitle>|<regionCode>" → game_id  (pass 3)
    QMap<QString, QString> titleToId;
    // "<systemId>|<serial>" → game_id  (pass 2)
    QMap<QString, QString> serialToId;

    int gamesCreated = 0;

    // Single pass — assign each record to an existing game_id or mint a new one.
    for (SourceRecordEnvelope &rec : records) {
        QString assignedId;
        int confidence = 0;

        // Pass 1a — sha1
        if (!rec.hashes.sha1.isEmpty()) {
            if (sha1ToId.contains(rec.hashes.sha1)) {
                assignedId = sha1ToId.value(rec.hashes.sha1);
                confidence = 100;
            }
        }

        // Pass 1b — md5
        if (assignedId.isEmpty() && !rec.hashes.md5.isEmpty()) {
            if (md5ToId.contains(rec.hashes.md5)) {
                assignedId = md5ToId.value(rec.hashes.md5);
                confidence = 95;
            }
        }

        // Pass 1c — crc32
        if (assignedId.isEmpty() && !rec.hashes.crc32.isEmpty()) {
            if (crc32ToId.contains(rec.hashes.crc32)) {
                assignedId = crc32ToId.value(rec.hashes.crc32);
                confidence = 90;
            }
        }

        // Pass 2 — serial (within same system)
        if (assignedId.isEmpty() && rec.resolvedSystemId > 0) {
            for (const QString &serial : std::as_const(rec.serials)) {
                if (serial.isEmpty()) {
                    continue;
                }
                const QString serialKey = QString::number(rec.resolvedSystemId)
                                          + QLatin1Char('|') + serial;
                if (serialToId.contains(serialKey)) {
                    assignedId = serialToId.value(serialKey);
                    confidence = 80;
                    break;
                }
            }
        }

        // Pass 3 — conservative title match (same system, same region)
        if (assignedId.isEmpty() && rec.resolvedSystemId > 0
                && !rec.titleRaw.isEmpty()) {
            const QString normTitle = normalizeTitle(rec.titleRaw);
            const QString titleKey  = QString::number(rec.resolvedSystemId)
                                      + QLatin1Char('|') + normTitle
                                      + QLatin1Char('|') + rec.resolvedRegionCode;
            if (titleToId.contains(titleKey)) {
                assignedId = titleToId.value(titleKey);
                confidence = 60;
            }
        }

        // Mint a new game if no match found
        if (assignedId.isEmpty()) {
            // Seed: prefer sha1, fall back to externalKey
            const QString seed = !rec.hashes.sha1.isEmpty()
                                     ? rec.hashes.sha1
                                     : rec.externalKey;
            assignedId = generateGameId(seed);
            ++gamesCreated;
        }

        // Register this record's identifiers so later records can link to it
        if (!rec.hashes.sha1.isEmpty()) {
            sha1ToId.insert(rec.hashes.sha1, assignedId);
        }
        if (!rec.hashes.md5.isEmpty()) {
            md5ToId.insert(rec.hashes.md5, assignedId);
        }
        if (!rec.hashes.crc32.isEmpty()) {
            crc32ToId.insert(rec.hashes.crc32, assignedId);
        }
        if (rec.resolvedSystemId > 0) {
            for (const QString &serial : std::as_const(rec.serials)) {
                if (!serial.isEmpty()) {
                    const QString serialKey = QString::number(rec.resolvedSystemId)
                                              + QLatin1Char('|') + serial;
                    if (!serialToId.contains(serialKey)) {
                        serialToId.insert(serialKey, assignedId);
                    }
                }
            }
            if (!rec.titleRaw.isEmpty()) {
                const QString normTitle = normalizeTitle(rec.titleRaw);
                const QString titleKey  = QString::number(rec.resolvedSystemId)
                                          + QLatin1Char('|') + normTitle
                                          + QLatin1Char('|') + rec.resolvedRegionCode;
                if (!titleToId.contains(titleKey)) {
                    titleToId.insert(titleKey, assignedId);
                }
            }
        }

        rec.linkedGameId            = assignedId;
        rec.linkedConfidencePercent = confidence > 0 ? confidence : 50;
    }

    return gamesCreated;
}

} // namespace Compendium
} // namespace Remus
