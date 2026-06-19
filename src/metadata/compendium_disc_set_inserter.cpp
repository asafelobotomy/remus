#include "compendium_disc_set_inserter.h"

#include "../core/disc_set_key.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QSqlError>

namespace Remus {
namespace Compendium {

    namespace {

        QString blockKeyForRecord(const SourceRecordEnvelope &rec) {
            const QString blockName = rec.datGameBlockName.isEmpty() ? rec.titleRaw : rec.datGameBlockName;
            return rec.sourceId + QLatin1Char('|') + rec.snapshotId + QLatin1Char('|') + blockName;
        }

        QString setKeyForRecord(const SourceRecordEnvelope &rec) {
            const QString title = rec.datGameBlockName.isEmpty() ? rec.titleRaw : rec.datGameBlockName;
            return DiscSetKey::compute(rec.resolvedSystemId, title, rec.resolvedRegionCode);
        }

    } // namespace

    QHash<QString, int> DiscSetInserter::effectiveDiscCounts(const QList<SourceRecordEnvelope> &records) {
        QHash<QString, int> explicitCount;
        QHash<QString, int> maxDiscNumber;

        for (const SourceRecordEnvelope &rec : records) {
            if (rec.resolvedSystemId <= 0)
                continue;
            if (rec.datGameBlockName.isEmpty() && rec.titleRaw.isEmpty())
                continue;

            const QString setKey = setKeyForRecord(rec);
            maxDiscNumber[setKey] = qMax(maxDiscNumber.value(setKey, 0), rec.parsedDiscNumber);
            if (rec.parsedDiscCount > 0)
                explicitCount[setKey] = qMax(explicitCount.value(setKey, 0), rec.parsedDiscCount);
        }

        QHash<QString, int> result;
        for (auto it = maxDiscNumber.cbegin(), end = maxDiscNumber.cend(); it != end; ++it) {
            const int explicitValue = explicitCount.value(it.key(), 0);
            int count = explicitValue > 0 ? explicitValue : it.value();
            if (count <= 0)
                count = 1;
            result.insert(it.key(), count);
        }
        return result;
    }

    qint64 DiscSetInserter::ensureDiscSet(const SourceRecordEnvelope &blockRepresentative, int effectiveDiscCount,
        qint64 sourceItemId, QSqlDatabase &db, CompilerStats &stats, QString &error) {
        if (blockRepresentative.linkedGameId.isEmpty()) {
            error = QStringLiteral("disc set insert skipped: missing linked game id");
            return -1;
        }
        if (blockRepresentative.resolvedSystemId <= 0) {
            error = QStringLiteral("disc set insert skipped: unresolved system id");
            return -1;
        }

        const QString titleDisc = blockRepresentative.datGameBlockName.isEmpty() ? blockRepresentative.titleRaw
                                                                                 : blockRepresentative.datGameBlockName;
        if (titleDisc.isEmpty()) {
            error = QStringLiteral("disc set insert skipped: empty DAT game block title");
            return -1;
        }

        const QString setKey = setKeyForRecord(blockRepresentative);
        const int discNumber = blockRepresentative.parsedDiscNumber;
        const int discCount
            = effectiveDiscCount > 0 ? effectiveDiscCount : qMax(blockRepresentative.parsedDiscCount, 1);
        const QString setVariant
            = blockRepresentative.parsedSetVariant.isNull() ? QStringLiteral("") : blockRepresentative.parsedSetVariant;
        const QString setRole
            = blockRepresentative.parsedSetRole.isEmpty() ? QStringLiteral("game") : blockRepresentative.parsedSetRole;

        QSqlQuery qInsert(db);
        qInsert.prepare(QStringLiteral("INSERT OR IGNORE INTO game_disc_sets "
                                       "(game_id, set_key, disc_number, disc_count, set_variant, set_role, title_disc, "
                                       " source_id, snapshot_id, source_item_id, primary_content_sha1) "
                                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        qInsert.addBindValue(blockRepresentative.linkedGameId);
        qInsert.addBindValue(setKey);
        qInsert.addBindValue(discNumber);
        qInsert.addBindValue(discCount);
        qInsert.addBindValue(setVariant);
        qInsert.addBindValue(setRole);
        qInsert.addBindValue(titleDisc);
        qInsert.addBindValue(blockRepresentative.sourceId);
        qInsert.addBindValue(blockRepresentative.snapshotId);
        qInsert.addBindValue(sourceItemId > 0 ? QVariant(sourceItemId) : QVariant());
        qInsert.addBindValue(
            blockRepresentative.primaryContentSha1.isEmpty() ? QVariant() : blockRepresentative.primaryContentSha1);

        if (!qInsert.exec()) {
            error = QStringLiteral("disc set insert failed: %1").arg(qInsert.lastError().text());
            return -1;
        }
        if (qInsert.numRowsAffected() > 0) {
            ++stats.discSetsCreated;
            return qInsert.lastInsertId().toLongLong();
        }

        QSqlQuery qSelect(db);
        qSelect.prepare(QStringLiteral("SELECT disc_set_id FROM game_disc_sets "
                                       "WHERE set_key = ? AND disc_number = ? AND set_variant = ? "
                                       "AND source_id = ? AND snapshot_id = ? LIMIT 1"));
        qSelect.addBindValue(setKey);
        qSelect.addBindValue(discNumber);
        qSelect.addBindValue(setVariant);
        qSelect.addBindValue(blockRepresentative.sourceId);
        qSelect.addBindValue(blockRepresentative.snapshotId);
        if (!qSelect.exec() || !qSelect.next()) {
            error = QStringLiteral("disc set upsert missed for set_key=%1 disc=%2 variant=%3 source=%4")
                        .arg(setKey)
                        .arg(discNumber)
                        .arg(setVariant, blockRepresentative.sourceId);
            return -1;
        }
        return qSelect.value(0).toLongLong();
    }

    qint64 DiscSetInserter::lookupPrimarySignatureId(
        const SourceRecordEnvelope &rec, QSqlDatabase &db, QString &error) {
        if (rec.linkedGameId.isEmpty())
            return -1;

        QSqlQuery q(db);
        q.prepare(QStringLiteral(R"(
            SELECT signature_id
            FROM game_signatures
            WHERE game_id = ? AND source_entry_key = ?
            ORDER BY is_primary DESC,
                     CASE hash_type
                         WHEN 'sha256' THEN 0
                         WHEN 'sha1' THEN 1
                         WHEN 'md5' THEN 2
                         WHEN 'crc32' THEN 3
                         ELSE 4
                     END,
                     signature_id ASC
            LIMIT 1
        )"));
        q.addBindValue(rec.linkedGameId);
        q.addBindValue(rec.externalKey);
        if (!q.exec()) {
            error = q.lastError().text();
            return -1;
        }
        if (!q.next())
            return -1;
        return q.value(0).toLongLong();
    }

    bool DiscSetInserter::insertTrack(const SourceRecordEnvelope &rec, qint64 discSetId, qint64 signatureId,
        QSqlQuery &q, CompilerStats &stats, QString &error) {
        if (discSetId <= 0)
            return true;

        const int trackIndex = rec.trackIndex > 0 ? rec.trackIndex : 1;
        const QString romName = rec.datRomName.isEmpty() ? rec.titleRaw : rec.datRomName;
        q.bindValue(0, discSetId);
        q.bindValue(1, trackIndex);
        q.bindValue(2, romName);
        q.bindValue(3, signatureId > 0 ? QVariant(signatureId) : QVariant(QMetaType(QMetaType::LongLong)));
        q.bindValue(4, rec.externalKey);
        if (!q.exec()) {
            error = q.lastError().text();
            return false;
        }
        if (q.numRowsAffected() > 0)
            ++stats.tracksCreated;
        return true;
    }

    bool DiscSetInserter::insertDiscTopologyForRecords(
        const QList<SourceRecordEnvelope> &records, QSqlDatabase &db, CompilerStats &stats, QString &error) {
        if (records.isEmpty())
            return true;

        const QHash<QString, int> discCounts = effectiveDiscCounts(records);
        QHash<QString, qint64> discSetIds;
        QSet<QString> discSetBlocksCreated;

        QSqlQuery qDiscTrack(db);
        QSqlQuery qSourceItem(db);

        if (!qDiscTrack.prepare(QStringLiteral("INSERT OR IGNORE INTO game_disc_tracks "
                                               "(disc_set_id, track_index, rom_name, signature_id, source_entry_key) "
                                               "VALUES (?, ?, ?, ?, ?)"))
            || !qSourceItem.prepare(QStringLiteral("SELECT source_item_id FROM source_items "
                                                   "WHERE source_id = ? AND external_key = ? LIMIT 1"))) {
            error = qDiscTrack.lastError().text();
            return false;
        }

        for (const SourceRecordEnvelope &rec : records) {
            if (rec.resolvedSystemId <= 0 || rec.linkedGameId.isEmpty())
                continue;
            if (rec.datGameBlockName.isEmpty() && rec.titleRaw.isEmpty())
                continue;

            const QString blockKey = blockKeyForRecord(rec);
            qint64 discSetId = discSetIds.value(blockKey, -1);
            if (discSetId < 0 && !discSetBlocksCreated.contains(blockKey)) {
                qSourceItem.bindValue(0, rec.sourceId);
                qSourceItem.bindValue(1, rec.externalKey);
                qint64 sourceItemId = -1;
                if (qSourceItem.exec() && qSourceItem.next())
                    sourceItemId = qSourceItem.value(0).toLongLong();

                const QString setKey = setKeyForRecord(rec);
                const int effectiveDiscCount = discCounts.value(setKey, qMax(rec.parsedDiscCount, 1));
                discSetId = DiscSetInserter::ensureDiscSet(rec, effectiveDiscCount, sourceItemId, db, stats, error);
                if (discSetId < 0) {
                    if (error.isEmpty())
                        error = QStringLiteral("failed to upsert disc set for block %1").arg(blockKey);
                    return false;
                }
                discSetIds.insert(blockKey, discSetId);
                discSetBlocksCreated.insert(blockKey);
            } else if (discSetId < 0) {
                discSetId = discSetIds.value(blockKey, -1);
            }

            if (discSetId <= 0)
                continue;

            const qint64 signatureId = DiscSetInserter::lookupPrimarySignatureId(rec, db, error);
            if (signatureId < 0 && !error.isEmpty())
                return false;
            if (!DiscSetInserter::insertTrack(rec, discSetId, signatureId, qDiscTrack, stats, error))
                return false;
        }

        return true;
    }

} // namespace Compendium
} // namespace Remus
