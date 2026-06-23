#include "compendium_enrichment_sql.h"

#include <QDateTime>
#include <QMetaType>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace CompendiumEnrichmentSql {

bool execPrepared(QSqlQuery &query, QString &error, const QString &context) {
    if (!query.exec()) {
        error = QStringLiteral("%1 failed: %2").arg(context, query.lastError().text());
        return false;
    }
    return true;
}

FactReplaceQueries::FactReplaceQueries(QSqlDatabase &db)
    : database(db)
    , existsQ(db)
    , clearFieldCanonQ(db)
    , clearFieldConflictQ(db) {
    existsQ.prepare(QStringLiteral("SELECT fact_id, field_value FROM game_facts "
                                   "WHERE game_id = ? AND field_name = ? AND source_id = ? LIMIT 1"));
    clearFieldCanonQ.prepare(QStringLiteral("DELETE FROM canonical_resolution WHERE selected_fact_id IN "
                                            "(SELECT fact_id FROM game_facts "
                                            " WHERE game_id = ? AND field_name = ? AND source_id = ?)"));
    clearFieldConflictQ.prepare(QStringLiteral("UPDATE merge_conflicts SET chosen_fact_id = NULL "
                                               "WHERE chosen_fact_id IN "
                                               "(SELECT fact_id FROM game_facts "
                                               " WHERE game_id = ? AND field_name = ? AND source_id = ?)"));
}

namespace {

    bool compendiumTableExists(QSqlDatabase &db, const QString &tableName) {
        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ? LIMIT 1"));
        query.addBindValue(tableName);
        return query.exec() && query.next();
    }

    bool clearFieldFactDeletionBlockers(FactReplaceQueries &replaceQ, const QString &gameId, const QString &fieldName,
        const QString &sourceId, QString &error) {
        if (compendiumTableExists(replaceQ.database, QStringLiteral("canonical_resolution"))) {
            replaceQ.clearFieldCanonQ.bindValue(0, gameId);
            replaceQ.clearFieldCanonQ.bindValue(1, fieldName);
            replaceQ.clearFieldCanonQ.bindValue(2, sourceId);
            if (!execPrepared(
                    replaceQ.clearFieldCanonQ, error, QStringLiteral("Clear canonical resolution for field replace")))
                return false;
        }

        if (compendiumTableExists(replaceQ.database, QStringLiteral("merge_conflicts"))) {
            replaceQ.clearFieldConflictQ.bindValue(0, gameId);
            replaceQ.clearFieldConflictQ.bindValue(1, fieldName);
            replaceQ.clearFieldConflictQ.bindValue(2, sourceId);
            if (!execPrepared(
                    replaceQ.clearFieldConflictQ, error, QStringLiteral("Clear merge conflict refs for field replace")))
                return false;
        }

        return true;
    }

} // namespace

bool upsertEnrichmentSource(QSqlDatabase &db, const SourceSpec &source, const SnapshotSpec &snapshot, QString &error) {
    QSqlQuery srcQ(db);
    srcQ.prepare(QStringLiteral("INSERT OR IGNORE INTO sources "
                                "(source_id, display_name, source_type, license_id, license_url, "
                                "attribution_required, priority, enabled) "
                                "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    srcQ.addBindValue(source.sourceId);
    srcQ.addBindValue(source.displayName);
    srcQ.addBindValue(source.sourceType);
    srcQ.addBindValue(
        source.licenseId.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(source.licenseId));
    srcQ.addBindValue(
        source.licenseUrl.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(source.licenseUrl));
    srcQ.addBindValue(source.attributionRequired ? 1 : 0);
    srcQ.addBindValue(source.priority);
    srcQ.addBindValue(1); // enabled
    if (!execPrepared(srcQ, error, QStringLiteral("Upsert source %1").arg(source.sourceId)))
        return false;

    QSqlQuery snapQ(db);
    snapQ.prepare(QStringLiteral("INSERT OR IGNORE INTO source_snapshots "
                                 "(snapshot_id, source_id, snapshot_label, snapshot_ref, fetched_at, checksum_sha256) "
                                 "VALUES (?, ?, ?, ?, ?, ?)"));
    snapQ.addBindValue(snapshot.snapshotId);
    snapQ.addBindValue(source.sourceId);
    snapQ.addBindValue(snapshot.snapshotLabel);
    snapQ.addBindValue(QVariant(QMetaType(QMetaType::QString))); // snapshot_ref (NULL)
    snapQ.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    snapQ.addBindValue(QVariant(QMetaType(QMetaType::QString))); // checksum_sha256 (NULL)
    return execPrepared(snapQ, error, QStringLiteral("Upsert snapshot %1").arg(snapshot.snapshotId));
}

bool insertGameFact(FactReplaceQueries &replaceQueries, QSqlQuery &delQuery, QSqlQuery &factQuery,
    const FactInsertSpec &spec, const QString &gameId, const QString &fieldName, const QString &fieldValue,
    const QString &valueType, QString &error, const QString &contextPrefix, bool *inserted) {
    if (inserted)
        *inserted = false;
    if (fieldValue.isEmpty())
        return true;

    replaceQueries.existsQ.bindValue(0, gameId);
    replaceQueries.existsQ.bindValue(1, fieldName);
    replaceQueries.existsQ.bindValue(2, spec.sourceId);
    if (!execPrepared(replaceQueries.existsQ, error, QStringLiteral("Lookup fact for replace")))
        return false;

    if (replaceQueries.existsQ.next()) {
        if (replaceQueries.existsQ.value(1).toString() == fieldValue)
            return true;
    }

    if (!clearFieldFactDeletionBlockers(replaceQueries, gameId, fieldName, spec.sourceId, error))
        return false;

    // Remove any prior fact from this source for this game+field so that
    // re-runs replace stale values rather than accumulating alongside them.
    delQuery.bindValue(0, gameId);
    delQuery.bindValue(1, fieldName);
    delQuery.bindValue(2, spec.sourceId);
    if (!execPrepared(delQuery, error, QStringLiteral("Replace %1 fact %2").arg(contextPrefix, fieldName)))
        return false;

    factQuery.bindValue(0, gameId);
    factQuery.bindValue(1, fieldName);
    factQuery.bindValue(2, fieldValue);
    factQuery.bindValue(3, valueType);
    factQuery.bindValue(4, spec.sourceId);
    factQuery.bindValue(5, spec.snapshotId);
    factQuery.bindValue(6, spec.sourcePriority);
    factQuery.bindValue(7, spec.confidence);
    if (!execPrepared(factQuery, error, QStringLiteral("Insert %1 fact %2").arg(contextPrefix, fieldName))) {
        return false;
    }

    if (inserted)
        *inserted = factQuery.numRowsAffected() > 0;
    return true;
}

QVariant nullableText(const QString &value) {
    return value.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(value);
}

QVariant nullableInt(int value) {
    return value > 0 ? QVariant(value) : QVariant(QMetaType(QMetaType::Int));
}

QVariant nullableDouble(double value) {
    return value > 0.0 ? QVariant(value) : QVariant(QMetaType(QMetaType::Double));
}

QString normalizeMetadataTitle(const QString &title) {
    QString s = title.trimmed();
    // Strip all trailing parenthetical groups, e.g.
    // "Foo Game (Europe) (En,Fr,De) (Rev 1)" → "Foo Game".
    // DAT titles routinely carry 2–4 stacked region/language/revision
    // suffixes that metadata sources (IGDB, OpenVGDB) don't include.
    while (s.endsWith(QLatin1Char(')'))) {
        const int paren = s.lastIndexOf(QLatin1Char('('));
        if (paren <= 0)
            break;
        s = s.left(paren).trimmed();
    }
    s = s.toLower();
    static const QStringList articles { QStringLiteral("the "), QStringLiteral("a "), QStringLiteral("an ") };
    for (const QString &art : articles) {
        if (s.startsWith(art)) {
            s = s.mid(art.size());
            break;
        }
    }
    // Keep only letters and digits (drop spaces for consistent token comparison).
    QString out;
    out.reserve(s.size());
    for (const QChar &c : s) {
        if (c.isLetterOrNumber())
            out.append(c);
    }
    return out;
}

bool bulkClearSourceFactBlockers(QSqlDatabase &db, const QString &sourceId, QString &error) {
    QSqlQuery clearCanon(db);
    clearCanon.prepare(QStringLiteral("DELETE FROM canonical_resolution WHERE selected_fact_id IN "
                                      "(SELECT fact_id FROM game_facts WHERE source_id = ?)"));
    clearCanon.addBindValue(sourceId);
    if (!execPrepared(clearCanon, error, QStringLiteral("Bulk clear canonical for source")))
        return false;

    QSqlQuery clearConflict(db);
    clearConflict.prepare(QStringLiteral("UPDATE merge_conflicts SET chosen_fact_id = NULL "
                                         "WHERE chosen_fact_id IN "
                                         "(SELECT fact_id FROM game_facts WHERE source_id = ?)"));
    clearConflict.addBindValue(sourceId);
    return execPrepared(clearConflict, error, QStringLiteral("Bulk clear merge conflicts for source"));
}

QSet<QString> loadGamesWithMinSourceFieldFacts(
    QSqlDatabase &db, const QString &sourceId, int minDistinctFields, QString &error) {
    QSet<QString> gameIds;
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT game_id FROM game_facts WHERE source_id = ? "
                             "GROUP BY game_id HAVING COUNT(DISTINCT field_name) >= ?"));
    q.addBindValue(sourceId);
    q.addBindValue(minDistinctFields);
    if (!execPrepared(q, error, QStringLiteral("Load source-satisfied games")))
        return gameIds;
    while (q.next())
        gameIds.insert(q.value(0).toString());
    return gameIds;
}

EnrichmentBatchWriter::EnrichmentBatchWriter(QSqlDatabase &db, int batchSize)
    : m_db(db)
    , m_batchSize(batchSize > 0 ? batchSize : kEnrichmentBatchCommitGames) { }

bool EnrichmentBatchWriter::begin(QString &error) {
    if (m_active)
        return true;
    if (!m_db.transaction()) {
        error = QStringLiteral("Failed to start enrichment batch: %1").arg(m_db.lastError().text());
        return false;
    }
    m_active = true;
    m_gamesInBatch = 0;
    return true;
}

bool EnrichmentBatchWriter::commitCurrent(QString &error) {
    if (!m_active)
        return true;
    if (!m_db.commit()) {
        error = QStringLiteral("Failed to commit enrichment batch: %1").arg(m_db.lastError().text());
        m_db.rollback();
        m_active = false;
        m_gamesInBatch = 0;
        return false;
    }
    m_active = false;
    m_gamesInBatch = 0;
    return true;
}

bool EnrichmentBatchWriter::onGameProcessed(QString &error) {
    if (!m_active) {
        if (!begin(error))
            return false;
    }
    ++m_gamesInBatch;
    if (m_gamesInBatch < m_batchSize)
        return true;
    return commitCurrent(error) && begin(error);
}

bool EnrichmentBatchWriter::finish(QString &error) {
    return commitCurrent(error);
}

EnrichmentBatchWriter::~EnrichmentBatchWriter() {
    if (m_active)
        m_db.rollback();
}

} // namespace CompendiumEnrichmentSql
