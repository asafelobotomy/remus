#include "compendium_fact_inserter.h"

#include <QHash>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

namespace Remus {
namespace Compendium {

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool execQuery(QSqlQuery &q, QString &error)
{
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    return true;
}

// Look up the priority for a source; used as source_priority in game_facts.
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

// ── Game upsert ───────────────────────────────────────────────────────────────

bool FactInserter::ensureGame(const SourceRecordEnvelope &rec,
                               QSqlDatabase &db,
                               CompilerStats &stats,
                               QString &error) const
{
    if (rec.linkedGameId.isEmpty()) {
        error = QStringLiteral("Record has no linkedGameId: %1").arg(rec.externalKey);
        return false;
    }

    QSqlQuery qCheck(db);
    qCheck.prepare(QStringLiteral("SELECT 1 FROM games WHERE game_id = ? LIMIT 1"));
    qCheck.addBindValue(rec.linkedGameId);
    if (!execQuery(qCheck, error)) {
        return false;
    }

    const bool gameExists = qCheck.next();
    if (!gameExists) {
        // canonical_title must be non-empty per schema (NOT NULL).
        const QString title = rec.titleRaw.isEmpty()
                                  ? QStringLiteral("[unknown]")
                                  : rec.titleRaw;
        // system_id is NOT NULL per schema — skip insert if unresolved.
        if (rec.resolvedSystemId <= 0) {
            return true; // skip silently; record will still be in source_items
        }

        QSqlQuery qGame(db);
        qGame.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO games "
            "(game_id, canonical_title, system_id, primary_region_code) "
            "VALUES (?, ?, ?, ?)"));
        qGame.addBindValue(rec.linkedGameId);
        qGame.addBindValue(title);
        qGame.addBindValue(rec.resolvedSystemId);
        qGame.addBindValue(rec.resolvedRegionCode.isEmpty()
                               ? QVariant(QMetaType(QMetaType::QString))
                               : rec.resolvedRegionCode);
        if (!execQuery(qGame, error)) {
            return false;
        }
        if (qGame.numRowsAffected() > 0) {
            ++stats.gamesCreated;
        }
    }

    // Always attempt to insert the name (IGNORE handles duplicates).
    if (!rec.titleRaw.isEmpty()) {
        QSqlQuery qName(db);
        qName.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_names "
            "(game_id, name_text, alias_type, locale, source_id, snapshot_id, confidence) "
            "VALUES (?, ?, 'official', '', ?, ?, 1.0)"));
        qName.addBindValue(rec.linkedGameId);
        qName.addBindValue(rec.titleRaw);
        qName.addBindValue(rec.sourceId);
        qName.addBindValue(rec.snapshotId);
        if (!execQuery(qName, error)) {
            return false;
        }
    }

    return true;
}

// ── Source item ───────────────────────────────────────────────────────────────

bool FactInserter::insertSourceItem(const SourceRecordEnvelope &rec,
                                     QSqlDatabase &db,
                                     CompilerStats &stats,
                                     QString &error) const
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO source_items "
        "(source_id, snapshot_id, external_key, system_hint, title_raw, region_raw, payload_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(rec.sourceId);
    q.addBindValue(rec.snapshotId);
    q.addBindValue(rec.externalKey);
    q.addBindValue(rec.systemHint.isEmpty()
                       ? QVariant(QMetaType(QMetaType::QString))
                       : rec.systemHint);
    q.addBindValue(rec.titleRaw.isEmpty()
                       ? QVariant(QMetaType(QMetaType::QString))
                       : rec.titleRaw);
    q.addBindValue(rec.regionRaw.isEmpty()
                       ? QVariant(QMetaType(QMetaType::QString))
                       : rec.regionRaw);
    q.addBindValue(rec.payloadJson.isEmpty()
                       ? QVariant(QMetaType(QMetaType::QString))
                       : rec.payloadJson);
    if (!execQuery(q, error)) {
        return false;
    }
    if (q.numRowsAffected() > 0) {
        ++stats.recordsIngested;
    }
    return true;
}

// ── Signatures ────────────────────────────────────────────────────────────────

bool FactInserter::insertSignatures(const SourceRecordEnvelope &rec,
                                     QSqlDatabase &db,
                                     CompilerStats &stats,
                                     QString &error) const
{
    if (rec.linkedGameId.isEmpty() || rec.resolvedSystemId <= 0) {
        // No canonical game row was created for this record; skip signatures.
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

    bool isPrimary = true; // first hash (sha1 preferred) is marked primary
    for (const auto &h : hashes) {
        if (h.value.isEmpty()) {
            continue;
        }
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_signatures "
            "(game_id, hash_type, hash_value, source_id, snapshot_id, "
            " source_entry_key, confidence, is_primary) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
        q.addBindValue(rec.linkedGameId);
        q.addBindValue(QLatin1String(h.type));
        q.addBindValue(h.value);
        q.addBindValue(rec.sourceId);
        q.addBindValue(rec.snapshotId);
        q.addBindValue(rec.externalKey);
        q.addBindValue(confidence);
        q.addBindValue(isPrimary ? 1 : 0);
        if (!execQuery(q, error)) {
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
                                  QSqlDatabase &db,
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
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_serials "
            "(game_id, serial_value, source_id, snapshot_id, source_entry_key, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?)"));
        q.addBindValue(rec.linkedGameId);
        q.addBindValue(serial);
        q.addBindValue(rec.sourceId);
        q.addBindValue(rec.snapshotId);
        q.addBindValue(rec.externalKey);
        q.addBindValue(confidence);
        if (!execQuery(q, error)) {
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
                                QSqlDatabase &db,
                                CompilerStats &stats,
                                QString &error,
                                int sourcePriority) const
{
    if (rec.linkedGameId.isEmpty() || rec.resolvedSystemId <= 0) {
        return true;
    }

    const int    priority   = sourcePriority;
    const double confidence = rec.linkedConfidencePercent / 100.0;

    for (auto it = rec.fields.constBegin(); it != rec.fields.constEnd(); ++it) {
        if (it.value().isEmpty()) {
            continue;
        }
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_facts "
            "(game_id, field_name, field_value, value_type, "
            " source_id, snapshot_id, source_priority, confidence) "
            "VALUES (?, ?, ?, 'text', ?, ?, ?, ?)"));
        q.addBindValue(rec.linkedGameId);
        q.addBindValue(it.key());
        q.addBindValue(it.value());
        q.addBindValue(rec.sourceId);
        q.addBindValue(rec.snapshotId);
        q.addBindValue(priority);
        q.addBindValue(confidence);
        if (!execQuery(q, error)) {
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
    // Cache source priorities so fetchSourcePriority is called once per source,
    // not once per record (avoids an O(N) SELECT loop across large batches).
    QHash<QString, int> priorityCache;

    for (const SourceRecordEnvelope &rec : records) {
        if (!ensureGame(rec, db, stats, error)) {
            return false;
        }
        if (!insertSourceItem(rec, db, stats, error)) {
            return false;
        }
        if (!insertSignatures(rec, db, stats, error)) {
            return false;
        }
        if (!insertSerials(rec, db, stats, error)) {
            return false;
        }
        auto it = priorityCache.constFind(rec.sourceId);
        if (it == priorityCache.constEnd()) {
            it = priorityCache.insert(rec.sourceId, fetchSourcePriority(rec.sourceId, db));
        }
        if (!insertFacts(rec, db, stats, error, it.value())) {
            return false;
        }
    }
    return true;
}

} // namespace Compendium
} // namespace Remus
