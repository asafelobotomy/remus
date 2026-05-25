#include "compendium_identity_linker.h"

#include <QCryptographicHash>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

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

bool IdentityLinker::loadFromDatabase(QSqlDatabase &db, QString &error)
{
    // ── Pass 1: hash signatures ───────────────────────────────────────────────
    {
        QSqlQuery q(db);
        q.setForwardOnly(true);
        if (!q.exec(QStringLiteral(
                "SELECT hash_type, hash_value, game_id FROM game_signatures"))) {
            error = q.lastError().text();
            return false;
        }
        while (q.next()) {
            const QString type  = q.value(0).toString();
            const QString value = q.value(1).toString();
            const QString id    = q.value(2).toString();
            if (type == QLatin1String("sha256")) {
                m_sha256ToId.insert(value, id);
            } else if (type == QLatin1String("sha1")) {
                m_sha1ToId.insert(value, id);
            } else if (type == QLatin1String("md5")) {
                m_md5ToId.insert(value, id);
            } else if (type == QLatin1String("crc32")) {
                m_crc32ToId.insert(value, id);
            }
        }
    }

    // ── Pass 2: serials ───────────────────────────────────────────────────────
    {
        QSqlQuery q(db);
        q.setForwardOnly(true);
        if (!q.exec(QStringLiteral(
                "SELECT gs.serial_value, gs.game_id, g.system_id "
                "FROM game_serials gs JOIN games g ON g.game_id = gs.game_id"))) {
            error = q.lastError().text();
            return false;
        }
        while (q.next()) {
            const QString serial   = q.value(0).toString();
            const QString id       = q.value(1).toString();
            const int     systemId = q.value(2).toInt();
            if (serial.isEmpty() || systemId <= 0) continue;
            const QString key = QString::number(systemId) + QLatin1Char('|') + serial;
            if (!m_serialToId.contains(key)) {
                m_serialToId.insert(key, id);
            }
        }
    }

    // ── Pass 3: title map (game_names stores all raw titleRaw values) ─────────
    // Normalising name_text here reproduces the same keys that link() would have
    // built during the original ingest, so incoming records will match against
    // the same normalised forms.
    {
        QSqlQuery q(db);
        q.setForwardOnly(true);
        if (!q.exec(QStringLiteral(
                "SELECT gn.name_text, gn.game_id, g.system_id, "
                "       COALESCE(g.primary_region_code, '') "
                "FROM game_names gn JOIN games g ON g.game_id = gn.game_id"))) {
            error = q.lastError().text();
            return false;
        }
        while (q.next()) {
            const QString name     = q.value(0).toString();
            const QString id       = q.value(1).toString();
            const int     systemId = q.value(2).toInt();
            const QString region   = q.value(3).toString();
            if (name.isEmpty() || systemId <= 0) continue;
            const QString normTitle = normalizeTitle(name);
            const QString key = QString::number(systemId) + QLatin1Char('|')
                                + normTitle + QLatin1Char('|') + region;
            if (!m_titleToId.contains(key)) {
                m_titleToId.insert(key, id);
            }
        }
    }

    return true;
}

int IdentityLinker::link(QList<SourceRecordEnvelope> &records)
{
    int gamesCreated = 0;

    // Single pass — assign each record to an existing game_id or mint a new one.
    for (SourceRecordEnvelope &rec : records) {
        QString assignedId;
        int confidence = 0;

        // Pass 0 — sha256 (highest hash strength)
        if (!rec.hashes.sha256.isEmpty()) {
            if (m_sha256ToId.contains(rec.hashes.sha256)) {
                assignedId = m_sha256ToId.value(rec.hashes.sha256);
                confidence = 100;
            }
        }

        // Pass 1a — sha1
        if (assignedId.isEmpty() && !rec.hashes.sha1.isEmpty()) {
            if (m_sha1ToId.contains(rec.hashes.sha1)) {
                assignedId = m_sha1ToId.value(rec.hashes.sha1);
                confidence = 100;
            }
        }

        // Pass 1b — md5
        if (assignedId.isEmpty() && !rec.hashes.md5.isEmpty()) {
            if (m_md5ToId.contains(rec.hashes.md5)) {
                assignedId = m_md5ToId.value(rec.hashes.md5);
                confidence = 95;
            }
        }

        // Pass 1c — crc32
        if (assignedId.isEmpty() && !rec.hashes.crc32.isEmpty()) {
            if (m_crc32ToId.contains(rec.hashes.crc32)) {
                assignedId = m_crc32ToId.value(rec.hashes.crc32);
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
                if (m_serialToId.contains(serialKey)) {
                    assignedId = m_serialToId.value(serialKey);
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
            if (m_titleToId.contains(titleKey)) {
                assignedId = m_titleToId.value(titleKey);
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
            m_sha1ToId.insert(rec.hashes.sha1, assignedId);
        }
        if (!rec.hashes.md5.isEmpty()) {
            m_md5ToId.insert(rec.hashes.md5, assignedId);
        }
        if (!rec.hashes.crc32.isEmpty()) {
            m_crc32ToId.insert(rec.hashes.crc32, assignedId);
        }
        if (!rec.hashes.sha256.isEmpty()) {
            m_sha256ToId.insert(rec.hashes.sha256, assignedId);
        }
        if (rec.resolvedSystemId > 0) {
            for (const QString &serial : std::as_const(rec.serials)) {
                if (!serial.isEmpty()) {
                    const QString serialKey = QString::number(rec.resolvedSystemId)
                                              + QLatin1Char('|') + serial;
                    if (!m_serialToId.contains(serialKey)) {
                        m_serialToId.insert(serialKey, assignedId);
                    }
                }
            }
            if (!rec.titleRaw.isEmpty()) {
                const QString normTitle = normalizeTitle(rec.titleRaw);
                const QString titleKey  = QString::number(rec.resolvedSystemId)
                                          + QLatin1Char('|') + normTitle
                                          + QLatin1Char('|') + rec.resolvedRegionCode;
                if (!m_titleToId.contains(titleKey)) {
                    m_titleToId.insert(titleKey, assignedId);
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
