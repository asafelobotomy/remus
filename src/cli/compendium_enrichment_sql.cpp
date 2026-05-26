#include "compendium_enrichment_sql.h"

#include <QDateTime>
#include <QMetaType>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace CompendiumEnrichmentSql {

bool execPrepared(QSqlQuery &query, QString &error, const QString &context)
{
    if (!query.exec()) {
        error = QStringLiteral("%1 failed: %2").arg(context, query.lastError().text());
        return false;
    }
    return true;
}

bool upsertEnrichmentSource(QSqlDatabase &db,
                            const SourceSpec &source,
                            const SnapshotSpec &snapshot,
                            QString &error)
{
    QSqlQuery srcQ(db);
    srcQ.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO sources "
        "(source_id, display_name, source_type, license_id, license_url, "
        "attribution_required, priority, enabled) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    srcQ.addBindValue(source.sourceId);
    srcQ.addBindValue(source.displayName);
    srcQ.addBindValue(source.sourceType);
    srcQ.addBindValue(source.licenseId.isEmpty()
                          ? QVariant(QMetaType(QMetaType::QString))
                          : QVariant(source.licenseId));
    srcQ.addBindValue(source.licenseUrl.isEmpty()
                          ? QVariant(QMetaType(QMetaType::QString))
                          : QVariant(source.licenseUrl));
    srcQ.addBindValue(source.attributionRequired ? 1 : 0);
    srcQ.addBindValue(source.priority);
    srcQ.addBindValue(1);  // enabled
    if (!execPrepared(srcQ, error,
                      QStringLiteral("Upsert source %1").arg(source.sourceId)))
        return false;

    QSqlQuery snapQ(db);
    snapQ.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO source_snapshots "
        "(snapshot_id, source_id, snapshot_label, snapshot_ref, fetched_at, checksum_sha256) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    snapQ.addBindValue(snapshot.snapshotId);
    snapQ.addBindValue(source.sourceId);
    snapQ.addBindValue(snapshot.snapshotLabel);
    snapQ.addBindValue(QVariant(QMetaType(QMetaType::QString)));  // snapshot_ref (NULL)
    snapQ.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    snapQ.addBindValue(QVariant(QMetaType(QMetaType::QString)));  // checksum_sha256 (NULL)
    return execPrepared(snapQ, error,
                        QStringLiteral("Upsert snapshot %1").arg(snapshot.snapshotId));
}

bool insertGameFact(QSqlQuery &delQuery,
                    QSqlQuery &factQuery,
                    const FactInsertSpec &spec,
                    const QString &gameId,
                    const QString &fieldName,
                    const QString &fieldValue,
                    const QString &valueType,
                    QString &error,
                    const QString &contextPrefix,
                    bool *inserted)
{
    if (inserted)
        *inserted = false;
    if (fieldValue.isEmpty())
        return true;

    // Remove any prior fact from this source for this game+field so that
    // re-runs replace stale values rather than accumulating alongside them.
    delQuery.bindValue(0, gameId);
    delQuery.bindValue(1, fieldName);
    delQuery.bindValue(2, spec.sourceId);
    if (!execPrepared(delQuery, error,
                      QStringLiteral("Replace %1 fact %2").arg(contextPrefix, fieldName)))
        return false;

    factQuery.bindValue(0, gameId);
    factQuery.bindValue(1, fieldName);
    factQuery.bindValue(2, fieldValue);
    factQuery.bindValue(3, valueType);
    factQuery.bindValue(4, spec.sourceId);
    factQuery.bindValue(5, spec.snapshotId);
    factQuery.bindValue(6, spec.sourcePriority);
    factQuery.bindValue(7, spec.confidence);
    if (!execPrepared(factQuery, error,
                      QStringLiteral("Insert %1 fact %2").arg(contextPrefix, fieldName))) {
        return false;
    }

    if (inserted)
        *inserted = factQuery.numRowsAffected() > 0;
    return true;
}

QVariant nullableText(const QString &value)
{
    return value.isEmpty()
               ? QVariant(QMetaType(QMetaType::QString))
               : QVariant(value);
}

QVariant nullableInt(int value)
{
    return value > 0
               ? QVariant(value)
               : QVariant(QMetaType(QMetaType::Int));
}

QVariant nullableDouble(double value)
{
    return value > 0.0
               ? QVariant(value)
               : QVariant(QMetaType(QMetaType::Double));
}

QString normalizeMetadataTitle(const QString &title)
{
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
    static const QStringList articles{
        QStringLiteral("the "), QStringLiteral("a "), QStringLiteral("an ")
    };
    for (const QString &art : articles) {
        if (s.startsWith(art)) { s = s.mid(art.size()); break; }
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

}  // namespace CompendiumEnrichmentSql
