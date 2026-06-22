#pragma once

#include "constants/database_schema.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>

namespace Remus::CompendiumSql {

inline void execPragma(QSqlDatabase &database, const QString &pragma) {
    QSqlQuery query(database);
    if (!query.exec(pragma))
        qWarning() << "Compendium PRAGMA failed (non-fatal):" << pragma << query.lastError().text();
}

inline void applyWritePragmas(QSqlDatabase &database) {
    execPragma(database, QStringLiteral("PRAGMA journal_mode = WAL"));
    execPragma(database,
        QStringLiteral("PRAGMA busy_timeout = %1").arg(Constants::DatabaseSchema::Compendium::BUSY_TIMEOUT_WRITE_MS));
    execPragma(database, Constants::DatabaseSchema::PRAGMA_FOREIGN_KEYS);
}

inline void applyReadOnlyPragmas(QSqlDatabase &database) {
    execPragma(database,
        QStringLiteral("PRAGMA busy_timeout = %1").arg(Constants::DatabaseSchema::Compendium::BUSY_TIMEOUT_READ_MS));
    execPragma(database, QStringLiteral("PRAGMA query_only = ON"));
}

inline bool beginImmediateTransaction(QSqlDatabase &database, QString &error) {
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("BEGIN IMMEDIATE"))) {
        error = query.lastError().text();
        return false;
    }
    return true;
}

inline void finalizeDatabasePragmas(QSqlDatabase &database) {
    execPragma(database, QStringLiteral("PRAGMA synchronous = NORMAL"));
    execPragma(database, QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));
}

inline bool tableExists(QSqlDatabase &database, const QString &tableName) {
    QSqlQuery query(database);
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?"));
    query.addBindValue(tableName);
    if (!query.exec() || !query.next())
        return false;
    return query.value(0).toInt() > 0;
}

} // namespace Remus::CompendiumSql
