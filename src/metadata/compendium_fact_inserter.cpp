#include "compendium_fact_inserter.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

namespace Remus {
namespace Compendium {

// ── Helpers ───────────────────────────────────────────────────────────────────

// Look up the priority for a source; called once per batch (not per record).
static int fetchSourcePriority(const QString &sourceId, QSqlDatabase &db)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT priority FROM sources WHERE source_id = ? LIMIT 1"));
    q.addBindValue(sourceId);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

// ── ensureGame ────────────────────────────────────────────────────────────────
// The redundant SELECT existence check has been removed — INSERT OR IGNORE is
// sufficient and avoids an extra round-trip per record.

bool FactInserter::ensureGame(const SourceRecordEnvelope &rec,
                               QSqlQuery &qGame, QSqlQuery &qName,
                               CompilerStats &stats,
                               QString &error) const
{
    if (rec.linkedGameId.isEmpty()) {
        error = QStringLiteral("Record has no linkedGameId: %1").arg(rec.externalKey);
        return false;
    }

    // Attempt game row creation. OR IGNORE is a no-op when the row already exists.
    // Skip when system_id is unresolved — the schema requires it NOT NULL.
    if (rec.resolvedSystemId > 0) {
        const QString title = rec.titleRaw.isEmpty()
                                  ? QStringLiteral("[unknown]")
                                  : rec.titleRaw;
        qGame.bindValue(0, rec.linkedGameId);
        qGame.bindValue(1, title);
        qGame.bindValue(2, rec.resolvedSystemId);
        qGame.bindValue(3, rec.resolvedRegionCode.isEmpty()
                              ? QVariant(QMetaType(QMetaType::QString))
                              : rec.resolvedRegionCode);
        if (!qGame.exec()) {
            error = qGame.lastError().text();
            return false;
        }
        if (qGame.numRowsAffected() > 0) {
            ++stats.gamesCreated;
        }
    }

    // Always attempt name insertion. FK OR IGNORE absorbs the case where the
    // game row doesn't exist yet (unresolved system_id carried over from a prior source).
    if (!rec.titleRaw.isEmpty()) {
        qName.bindValue(0, rec.linkedGameId);
        qName.bindValue(1, rec.titleRaw);
        qName.bindValue(2, rec.sourceId);
        qName.bindValue(3, rec.snapshotId);
        if (!qName.exec()) {
            error = qName.lastError().text();
            return false;
        }
    }

    return true;
}

// ── Source item ───────────────────────────────────────────────────────────────

bool FactInserter::insertSourceItem(const SourceRecordEnvelope &rec,
                                     QSqlQuery &q,
                                     CompilerStats &stats,
                                     QString &error) const
{
    q.bindValue(0, rec.sourceId);
    q.bindValue(1, rec.snapshotId);
    q.bindValue(2, rec.externalKey);
    q.bindValue(3, rec.systemHint.isEmpty()
                       ? QVariant(QMetaType(QMetaType::QString))
                       : rec.systemHint);
    q.bindValue(4, rec.titleRaw.isEmpty()
                       ? QVariant(QMetaType(QMetaType::QString))
                       : rec.titleRaw);
    q.bindValue(5, rec.regionRaw.isEmpty()
                       ? QVariant(QMetaType(QMetaType::QString))
                       : rec.regionRaw);
    q.bindValue(6, rec.payloadJson.isEmpty()
                       ? QVariant(QMetaType(QMetaType::QString))
                       : rec.payloadJson);
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() > 0) {
        ++stats.recordsIngested;
    }
    return true;
}

// ── Signatures ────────────────────────────────────────────────────────────────

bool FactInserter::insertSignatures(const SourceRecordEnvelope &rec,
                                     QSqlQuery &q,
                                     CompilerStats &stats,
                                     QString &error) const
{
    if (rec.linkedGameId.isEmpty() || rec.resolvedSystemId <= 0) {
        return true;
    }

    const double confidence = rec.linkedConfidencePercent / 100.0;
    const struct {
        const char *type;
        QString     value;
    } hashes[] = {
        {"sha1",  rec.hashes.sha1},
        {"md5",   rec.hashes.md5},
        {"crc32", rec.hashes.crc32},
    };

    bool isPrimary = true; // first non-empty hash (sha1 preferred) is marked primary
    for (const auto &h : hashes) {
        if (h.value.isEmpty()) {
            continue;
        }
        q.bindValue(0, rec.linkedGameId);
        q.bindValue(1, QLatin1String(h.type));
        q.bindValue(2, h.value);
        q.bindValue(3, rec.sourceId);
        q.bindValue(4, rec.snapshotId);
        q.bindValue(5, rec.externalKey);
        q.bindValue(6, confidence);
        q.bindValue(7, isPrimary ? 1 : 0);
        if (!q.exec()) {
            error = q.lastError().text();
            return false;
        }
        if (q.numRowsAffected() > 0) {
            ++stats.signaturesCreated;
        }
        isPrimary = false;
    }
    return true;
}

// ── Serials ───────────────────────────────────────────────────────────────────

bool FactInserter::insertSerials(const SourceRecordEnvelope &rec,
                                  QSqlQuery &q,
                                  CompilerStats &stats,
                                  QString &error) const
{
    if (rec.linkedGameId.isEmpty() || rec.resolvedSystemId <= 0) {
        return true;
    }

    const double confidence = rec.linkedConfidencePercent / 100.0;
    for (const QString &serial : rec.serials) {
        if (serial.isEmpty()) {
            continue;
        }
        q.bindValue(0, rec.linkedGameId);
        q.bindValue(1, serial);
        q.bindValue(2, rec.sourceId);
        q.bindValue(3, rec.snapshotId);
        q.bindValue(4, rec.externalKey);
        q.bindValue(5, confidence);
        if (!q.exec()) {
            error = q.lastError().text();
            return false;
        }
        if (q.numRowsAffected() > 0) {
            ++stats.serialsCreated;
        }
    }
    return true;
}

// ── Facts ─────────────────────────────────────────────────────────────────────

bool FactInserter::insertFacts(const SourceRecordEnvelope &rec,
                                QSqlQuery &q,
                                CompilerStats &stats,
                                QString &error,
                                int sourcePriority) const
{
    if (rec.linkedGameId.isEmpty() || rec.resolvedSystemId <= 0) {
        return true;
    }

    const double confidence = rec.linkedConfidencePercent / 100.0;
    for (auto it = rec.fields.constBegin(); it != rec.fields.constEnd(); ++it) {
        if (it.value().isEmpty()) {
            continue;
        }
        q.bindValue(0, rec.linkedGameId);
        q.bindValue(1, it.key());
        q.bindValue(2, it.value());
        q.bindValue(3, rec.sourceId);
        q.bindValue(4, rec.snapshotId);
        q.bindValue(5, sourcePriority);
        q.bindValue(6, confidence);
        if (!q.exec()) {
            error = q.lastError().text();
            return false;
        }
        if (q.numRowsAffected() > 0) {
            ++stats.factsCreated;
        }
    }
    return true;
}

// ── Public entry point ────────────────────────────────────────────────────────

bool FactInserter::insert(const QList<SourceRecordEnvelope> &records,
                           QSqlDatabase &db,
                           CompilerStats &stats,
                           QString &error) const
{
    if (records.isEmpty()) {
        return true;
    }

    // Source priority is constant within a single-source batch — fetch it once.
    const int sourcePriority = fetchSourcePriority(records.first().sourceId, db);

    // Prepare all statements once; reused (via bindValue) across every record.
    // This avoids calling sqlite3_prepare_v2() once per record per statement.
    QSqlQuery qGame(db);
    QSqlQuery qName(db);
    QSqlQuery qSourceItem(db);
    QSqlQuery qSig(db);
    QSqlQuery qSerial(db);
    QSqlQuery qFact(db);

    if (!qGame.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO games "
            "(game_id, canonical_title, system_id, primary_region_code) "
            "VALUES (?, ?, ?, ?)"))
        || !qName.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_names "
            "(game_id, name_text, alias_type, locale, source_id, snapshot_id, confidence) "
            "VALUES (?, ?, 'official', '', ?, ?, 1.0)"))
        || !qSourceItem.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO source_items "
            "(source_id, snapshot_id, external_key, system_hint, title_raw, region_raw, payload_json) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)"))
        || !qSig.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_signatures "
            "(game_id, hash_type, hash_value, source_id, snapshot_id, "
            " source_entry_key, confidence, is_primary) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"))
        || !qSerial.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_serials "
            "(game_id, serial_value, source_id, snapshot_id, source_entry_key, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?)"))
        || !qFact.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_facts "
            "(game_id, field_name, field_value, value_type, "
            " source_id, snapshot_id, source_priority, confidence) "
            "VALUES (?, ?, ?, 'text', ?, ?, ?, ?)")))
    {
        error = qGame.lastError().text();
        return false;
    }

    for (const SourceRecordEnvelope &rec : records) {
        if (!ensureGame(rec, qGame, qName, stats, error)) {
            return false;
        }
        if (!insertSourceItem(rec, qSourceItem, stats, error)) {
            return false;
        }
        if (!insertSignatures(rec, qSig, stats, error)) {
            return false;
        }
        if (!insertSerials(rec, qSerial, stats, error)) {
            return false;
        }
        if (!insertFacts(rec, qFact, stats, error, sourcePriority)) {
            return false;
        }
    }
    return true;
}

} // namespace Compendium
} // namespace Remus
