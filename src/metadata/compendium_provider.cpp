#include "compendium_provider.h"

#include "../core/system_resolver.h"

#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace Remus {

CompendiumProvider::CompendiumProvider(QObject *parent)
    : MetadataProvider(parent)
    , m_connectionName(QStringLiteral("compendium_provider_%1")
                           .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

CompendiumProvider::~CompendiumProvider()
{
    closeConnection();
}

bool CompendiumProvider::openDatabase(const QString &databasePath)
{
    closeConnection();

    const QFileInfo info(databasePath);
    if (!info.exists() || !info.isFile()) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(info.absoluteFilePath());
    if (!db.open()) {
        closeConnection();
        return false;
    }

    m_databasePath = info.absoluteFilePath();
    ensureFts5Index();
    return true;
}

void CompendiumProvider::ensureFts5Index()
{
    QSqlDatabase db = database();
    if (!db.isOpen())
        return;

    // Create trigram FTS5 virtual table if not present (handles existing DBs without rebuild)
    QSqlQuery create(db);
    create.exec(QStringLiteral(
        "CREATE VIRTUAL TABLE IF NOT EXISTS games_search USING fts5("
        "    title,"
        "    game_id UNINDEXED,"
        "    system_id UNINDEXED,"
        "    region_code UNINDEXED,"
        "    tokenize='trigram'"
        ")"));

    // Populate only when the table is empty (first time or after wipe)
    QSqlQuery countQ(db);
    countQ.exec(QStringLiteral("SELECT COUNT(*) FROM games_search"));
    if (countQ.next() && countQ.value(0).toInt() > 0)
        return;

    QSqlQuery populate(db);
    populate.exec(QStringLiteral(
        "INSERT INTO games_search(title, game_id, system_id, region_code) "
        "SELECT canonical_title, game_id, system_id, COALESCE(primary_region_code, '') FROM games "
        "UNION ALL "
        "SELECT gn.name_text, gn.game_id, g.system_id, COALESCE(g.primary_region_code, '') "
        "FROM game_names gn JOIN games g ON gn.game_id = g.game_id"));

    QSqlQuery optimize(db);
    optimize.exec(QStringLiteral("INSERT INTO games_search(games_search) VALUES('optimize')"));
}

bool CompendiumProvider::isAvailable()
{
    QSqlDatabase db = database();
    return db.isValid() && db.isOpen();
}

QSqlDatabase CompendiumProvider::database() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

int CompendiumProvider::resolveSystemId(const QString &system) const
{
    const QString systemName = system.trimmed();
    if (systemName.isEmpty()) {
        return 0;
    }

    int systemId = SystemResolver::systemIdByName(systemName);
    if (systemId != 0) {
        return systemId;
    }

    systemId = SystemResolver::systemIdByDatName(systemName);
    if (systemId != 0) {
        return systemId;
    }

    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return 0;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT system_id FROM systems "
        "WHERE LOWER(internal_name) = LOWER(?) OR LOWER(display_name) = LOWER(?) "
        "LIMIT 1"));
    query.addBindValue(systemName);
    query.addBindValue(systemName);
    if (!query.exec() || !query.next()) {
        return 0;
    }

    return query.value(0).toInt();
}

void CompendiumProvider::closeConnection()
{
    const QString connectionName = m_connectionName;
    if (!QSqlDatabase::contains(connectionName)) {
        m_databasePath.clear();
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(connectionName, false);
        if (db.isValid() && db.isOpen()) {
            db.close();
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
    m_databasePath.clear();
}

QString CompendiumProvider::detectHashType(const QString &hash, QString &normalizedValue)
{
    const QString compact = hash.trimmed().remove(QLatin1Char(' '));
    if (compact.size() == 8) {
        normalizedValue = compact.toUpper();
        return QStringLiteral("crc32");
    }
    if (compact.size() == 32) {
        normalizedValue = compact.toLower();
        return QStringLiteral("md5");
    }
    if (compact.size() == 40) {
        normalizedValue = compact.toLower();
        return QStringLiteral("sha1");
    }

    normalizedValue.clear();
    return {};
}

} // namespace Remus