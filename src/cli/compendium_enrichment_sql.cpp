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

QString normalizeMetadataTitle(const QString &title)
{
    QString s = title.trimmed();
    // Strip trailing parenthetical suffix: "(USA)", "(128K)", "(Rev 1)", etc.
    // Only strip if a matching ')' closes the expression at the very end.
    const int paren = s.lastIndexOf(QLatin1Char('('));
    if (paren > 0 && s.endsWith(QLatin1Char(')')))
        s = s.left(paren).trimmed();
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
