#include "verification_engine.h"
#include "compendium_disc_bridge.h"

#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>

namespace Remus {

namespace {

    QSqlDatabase compendiumDatabase(const QString &connectionName) {
        if (connectionName.isEmpty())
            return { };
        return QSqlDatabase::database(connectionName);
    }

    QList<int> collectOwnedFileIdsForSetKey(Database *libraryDb, const QString &setKey) {
        QList<int> ids;
        if (setKey.isEmpty())
            return ids;

        QSqlQuery query(libraryDb->database());
        query.prepare(QStringLiteral("SELECT id FROM files WHERE disc_set_key = ? AND is_primary = 1"));
        query.addBindValue(setKey);
        if (!query.exec())
            return ids;

        while (query.next())
            ids.append(query.value(0).toInt());
        return ids;
    }

} // namespace

QList<DiscSetCompletenessReport> VerificationEngine::discSetCompleteness(
    const QString &compendiumGameId, const QList<int> &ownedFileIds) const {
    QSqlDatabase compendiumDb = compendiumDatabase(m_compendiumConnectionName);
    if (!compendiumDb.isOpen() || compendiumGameId.isEmpty())
        return { };

    return computeDiscSetCompleteness(compendiumDb, m_database->database(), compendiumGameId, ownedFileIds);
}

DiscSetCompletenessReport VerificationEngine::discSetCompletenessBySetKey(
    const QString &setKey, const QList<int> &ownedFileIds) const {
    QSqlDatabase compendiumDb = compendiumDatabase(m_compendiumConnectionName);
    if (!compendiumDb.isOpen() || setKey.isEmpty())
        return { };

    return computeDiscSetCompletenessBySetKey(compendiumDb, m_database->database(), setKey, ownedFileIds);
}

QList<DiscSetCompletenessReport> VerificationEngine::discSetCompletenessForLibrary(const QString &systemFilter) const {
    QList<DiscSetCompletenessReport> reports;
    QSqlDatabase compendiumDb = compendiumDatabase(m_compendiumConnectionName);
    if (!compendiumDb.isOpen() || !compendiumDiscSetsAvailable(compendiumDb))
        return reports;

    QSqlQuery query(m_database->database());
    QString sql = QStringLiteral("SELECT DISTINCT disc_set_key FROM files WHERE disc_set_key IS NOT NULL "
                                 "AND disc_set_key != '' AND is_primary = 1");
    if (!systemFilter.isEmpty())
        sql += QStringLiteral(" AND system_id = (SELECT id FROM systems WHERE name = ? LIMIT 1)");
    sql += QStringLiteral(" ORDER BY disc_set_key");

    query.prepare(sql);
    if (!systemFilter.isEmpty())
        query.addBindValue(systemFilter);
    if (!query.exec())
        return reports;

    while (query.next()) {
        const QString setKey = query.value(0).toString();
        const QList<int> ownedIds = collectOwnedFileIdsForSetKey(m_database, setKey);
        const DiscSetCompletenessReport report
            = computeDiscSetCompletenessBySetKey(compendiumDb, m_database->database(), setKey, ownedIds);
        if (!report.missingDiscNumbers.isEmpty() || !report.trackGaps.isEmpty() || !report.warnings.isEmpty())
            reports.append(report);
    }
    return reports;
}

} // namespace Remus
