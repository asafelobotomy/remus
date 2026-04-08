#include "database.h"
#include "constants/constants.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QFileInfo>
#include <QUuid>

namespace Remus {

Database::Database(QObject *parent)
    : QObject(parent)
{
}

Database::~Database()
{
    close();
}

bool Database::initialize(const QString &dbPath, const QString &connectionName)
{
    m_dbPath = dbPath;
    m_connectionName = connectionName.isEmpty()
        ? QStringLiteral("remus-") + QUuid::createUuid().toString(QUuid::Id128)
        : connectionName;

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        logError(Constants::Errors::Database::FAILED_TO_OPEN);
        return false;
    }

    QSqlQuery pragmaQuery(m_db);
    if (!pragmaQuery.exec(Constants::DatabaseSchema::PRAGMA_FOREIGN_KEYS)) {
        logError("Failed to enable SQLite foreign keys: " + pragmaQuery.lastError().text());
        close();
        return false;
    }

    qInfo() << "Database opened:" << dbPath;

    // Check if schema exists
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT name FROM sqlite_master WHERE type='table' AND name=?"));
    query.addBindValue(QString::fromLatin1(Constants::DatabaseSchema::Tables::SYSTEMS));
    if (!query.exec()) {
        logError(QString("Failed to query database schema: %1").arg(query.lastError().text()));
        return false;
    }

    bool isNewDatabase = !query.next();
    
    if (isNewDatabase) {
        // Schema doesn't exist, create it
        if (!createSchema()) {
            logError(Constants::Errors::Database::FAILED_TO_CREATE_SCHEMA);
            return false;
        }
        
        // Populate default systems
        if (!populateDefaultSystems()) {
            logError(Constants::Errors::Database::FAILED_TO_POPULATE_SYSTEMS);
            return false;
        }
    }
    
    // Run migrations for new columns
    if (!runMigrations()) {
        close();
        return false;
    }

    return true;
}

void Database::close()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    if (!m_connectionName.isEmpty()) {
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

int Database::insertLibrary(const QString &path, const QString &name)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO libraries (path, name) VALUES (?, ?)");
    query.addBindValue(path);
    query.addBindValue(name.isEmpty() ? QFileInfo(path).fileName() : name);

    if (!query.exec()) {
        logError("Failed to insert library: " + query.lastError().text());
        return 0;
    }

    // Get library ID
    query.prepare("SELECT id FROM libraries WHERE path = ?");
    query.addBindValue(path);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

bool Database::deleteLibrary(int libraryId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM libraries WHERE id = ?");
    query.addBindValue(libraryId);

    if (!query.exec()) {
        logError("Failed to delete library: " + query.lastError().text());
        return false;
    }

    return query.numRowsAffected() > 0;
}

QString Database::getLibraryPath(int libraryId)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT path FROM libraries WHERE id = ?");
    query.addBindValue(libraryId);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }

    return QString();
}

int Database::getLibraryCount()
{
    QSqlQuery query(m_db);
    if (!query.exec("SELECT COUNT(*) FROM libraries")) {
        logError("Failed to count libraries: " + query.lastError().text());
        return 0;
    }

    if (query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

bool Database::deleteFilesForLibrary(int libraryId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM files WHERE library_id = ?");
    query.addBindValue(libraryId);

    if (!query.exec()) {
        logError("Failed to delete library files: " + query.lastError().text());
        return false;
    }

    return true;
}

int Database::insertSystem(const SystemInfo &system)
{
    if (system.name.isEmpty()) {
        logError("Cannot insert system with empty name");
        return 0;
    }
    if (system.extensions.isEmpty()) {
        logError("Cannot insert system '" + system.name + "' with empty extensions list");
        return 0;
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO systems 
        (id, name, display_name, manufacturer, generation, extensions, preferred_hash)
        VALUES (?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(name) DO UPDATE SET
            display_name = excluded.display_name,
            manufacturer = excluded.manufacturer,
            generation = excluded.generation,
            extensions = excluded.extensions,
            preferred_hash = excluded.preferred_hash
    )");
    query.addBindValue(system.id > 0 ? system.id : QVariant());
    query.addBindValue(system.name);
    query.addBindValue(system.displayName);
    query.addBindValue(system.manufacturer);
    query.addBindValue(system.generation);
    query.addBindValue(system.extensions.join(","));
    query.addBindValue(system.preferredHash);

    if (!query.exec()) {
        logError("Failed to insert system: " + query.lastError().text());
        return 0;
    }

    return getSystemId(system.name);
}

int Database::getSystemId(const QString &name)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM systems WHERE name = ?");
    query.addBindValue(name);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

QString Database::getSystemDisplayName(int systemId)
{
    // Use SystemResolver for consistent name resolution across all layers
    return SystemResolver::displayName(systemId);
}

void Database::logError(const QString &message)
{
    qCritical() << message;
    emit databaseError(message);
}

} // namespace Remus
