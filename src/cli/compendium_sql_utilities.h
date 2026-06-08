#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>

namespace CompendiumSqlUtilities {

inline QStringList splitSqlStatements(const QString &content) {
    QStringList statements;
    QString current;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool inLineComment = false;

    for (int i = 0; i < content.size(); ++i) {
        const QChar ch = content.at(i);
        const QChar next = (i + 1 < content.size()) ? content.at(i + 1) : QChar();

        if (inLineComment) {
            if (ch == QChar('\n')) {
                inLineComment = false;
            }
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote && ch == QChar('-') && next == QChar('-')) {
            inLineComment = true;
            ++i;
            continue;
        }

        if (ch == QChar('\'') && !inDoubleQuote) {
            current.append(ch);
            if (inSingleQuote && next == QChar('\'')) {
                current.append(next);
                ++i;
            } else {
                inSingleQuote = !inSingleQuote;
            }
            continue;
        }

        if (ch == QChar('"') && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            current.append(ch);
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote && ch == QChar(';')) {
            const QString statement = current.trimmed();
            if (!statement.isEmpty()) {
                statements.append(statement);
            }
            current.clear();
            continue;
        }

        current.append(ch);
    }

    const QString trailing = current.trimmed();
    if (!trailing.isEmpty()) {
        statements.append(trailing);
    }

    return statements;
}

inline QString reportPathForDatabase(const QString &databasePath) {
    QFileInfo info(databasePath);
    return info.dir().filePath(info.completeBaseName() + QStringLiteral(".report.json"));
}

inline bool executeSqlScript(QSqlDatabase &database, const QString &path, QString &error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QStringLiteral("Failed to open %1: %2").arg(path, file.errorString());
        return false;
    }
    const QString content = QString::fromUtf8(file.readAll());

    const QStringList statements = splitSqlStatements(content);
    for (const QString &statement : statements) {
        if (statement.isEmpty()) {
            continue;
        }

        QSqlQuery query(database);
        if (!query.exec(statement)) {
            error = QStringLiteral("Failed to execute %1: %2").arg(path, query.lastError().text());
            return false;
        }
    }

    return true;
}

inline bool execPrepared(QSqlQuery &query, QString &error, const QString &context) {
    if (!query.exec()) {
        error = QStringLiteral("%1 failed: %2").arg(context, query.lastError().text());
        return false;
    }
    return true;
}

inline int scalarCount(QSqlDatabase &database, const QString &sql, QString &error) {
    QSqlQuery query(database);
    if (!query.exec(sql)) {
        error = query.lastError().text();
        return -1;
    }
    if (!query.next()) {
        error = QStringLiteral("No rows returned for count query");
        return -1;
    }
    return query.value(0).toInt();
}

inline bool integrityCheckOk(QSqlDatabase &database, QString &error) {
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA integrity_check"))) {
        error = query.lastError().text();
        return false;
    }
    if (!query.next()) {
        error = QStringLiteral("PRAGMA integrity_check returned no rows");
        return false;
    }
    if (query.value(0).toString().trimmed().compare(QStringLiteral("ok"), Qt::CaseInsensitive) != 0) {
        error = query.value(0).toString().trimmed();
        return false;
    }
    return true;
}

inline bool writeReport(const QString &path, const QJsonObject &report, QString &error) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        error = QStringLiteral("Failed to write report %1: %2").arg(path, file.errorString());
        return false;
    }

    file.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace CompendiumSqlUtilities
