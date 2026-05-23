#include "compendium_enrichment.h"
#include "../metadata/igdb_provider.h"
#include "../core/system_resolver.h"
#include "../core/constants/providers.h"
#include "../services/credential_manager.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

using namespace Remus;

namespace {

bool execPrepared(QSqlQuery &query, QString &error, const QString &context)
{
    if (!query.exec()) {
        error = QStringLiteral("%1 failed: %2").arg(context, query.lastError().text());
        return false;
    }
    return true;
}

bool upsertIgdbSource(QSqlDatabase &database, const QString &snapshotId, QString &error)
{
    QSqlQuery srcQ(database);
    srcQ.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO sources "
        "(source_id, display_name, source_type, license_id, license_url, "
        "attribution_required, priority, enabled) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    srcQ.addBindValue(QStringLiteral("igdb"));
    srcQ.addBindValue(QStringLiteral("IGDB"));
    srcQ.addBindValue(QStringLiteral("online-api"));
    srcQ.addBindValue(QVariant(QMetaType(QMetaType::QString)));
    srcQ.addBindValue(QStringLiteral("https://www.igdb.com"));
    srcQ.addBindValue(1);
    srcQ.addBindValue(70);
    srcQ.addBindValue(1);
    if (!execPrepared(srcQ, error, QStringLiteral("Upsert igdb source")))
        return false;

    QSqlQuery snapQ(database);
    snapQ.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO source_snapshots "
        "(snapshot_id, source_id, snapshot_label, snapshot_ref, fetched_at, checksum_sha256) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    snapQ.addBindValue(snapshotId);
    snapQ.addBindValue(QStringLiteral("igdb"));
    snapQ.addBindValue(QStringLiteral("IGDB bulk enrichment"));
    snapQ.addBindValue(QVariant(QMetaType(QMetaType::QString)));
    snapQ.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    snapQ.addBindValue(QVariant(QMetaType(QMetaType::QString)));
    return execPrepared(snapQ, error, QStringLiteral("Upsert igdb snapshot"));
}

// Lowercase, strip leading articles, keep only letters/digits/spaces.
QString normalizeTitle(const QString &title)
{
    QString s = title.toLower().trimmed();
    static const QStringList articles{
        QStringLiteral("the "), QStringLiteral("a "), QStringLiteral("an ")
    };
    for (const QString &art : articles) {
        if (s.startsWith(art)) { s = s.mid(art.size()); break; }
    }
    QString out;
    out.reserve(s.size());
    for (const QChar &c : s) {
        if (c.isLetterOrNumber() || c.isSpace()) out += c;
    }
    return out.simplified();
}

} // namespace

namespace CompendiumEnrichment {

bool enrichFromIGDB(QSqlDatabase &database,
                    const QString &credentialsPath,
                    int &gamesEnriched,
                    int &factsInserted,
                    QString &error)
{
    gamesEnriched = 0;
    factsInserted = 0;

    // Load credentials via CredentialManager (JSON file → env var → QSettings → keychain)
    const QString clientId     = CredentialManager::get(QStringLiteral("igdb/client_id"),     credentialsPath);
    const QString clientSecret = CredentialManager::get(QStringLiteral("igdb/client_secret"), credentialsPath);
    if (clientId.isEmpty() || clientSecret.isEmpty()) {
        qInfo() << "[IGDB] Credentials not configured — enrichment skipped";
        return true;
    }

    IGDBProvider provider;
    provider.setCredentials(clientId, clientSecret);

    // Systems that still have games with no description
    QSqlQuery sysQ(database);
    if (!sysQ.exec(QStringLiteral(
            "SELECT DISTINCT g.system_id, s.display_name FROM games g "
            "JOIN systems s ON s.system_id = g.system_id "
            "WHERE g.description IS NULL OR g.description = '' "
            "ORDER BY s.display_name"))) {
        error = QStringLiteral("Query systems: %1").arg(sysQ.lastError().text());
        return false;
    }
    struct SysInfo { int id; QString name; };
    QList<SysInfo> systems;
    while (sysQ.next())
        systems.append({sysQ.value(0).toInt(), sysQ.value(1).toString()});

    if (systems.isEmpty())
        return true;

    const QString snapshotId   = QStringLiteral("igdb-") + QDate::currentDate().toString(QStringLiteral("yyyy-MM"));
    static const int PAGE_SIZE = 500;

    for (const SysInfo &sys : systems) {
        const QString igdbSlug = SystemResolver::providerName(sys.id, Constants::Providers::IGDB);
        if (igdbSlug.isEmpty()) continue;

        // Bulk-fetch all IGDB games for this platform
        QHash<QString, GameMetadata> igdbIndex;
        int offset = 0;
        while (true) {
            const QList<GameMetadata> page = provider.fetchGamesByPlatformSlug(igdbSlug, offset, PAGE_SIZE);
            for (const GameMetadata &gm : page) {
                if (!gm.description.isEmpty())
                    igdbIndex[normalizeTitle(gm.title)] = gm;
            }
            if (page.size() < PAGE_SIZE) break;
            offset += PAGE_SIZE;
        }

        if (igdbIndex.isEmpty()) continue;
        qInfo().noquote() << QStringLiteral("[IGDB] %1 (%2): %3 entries indexed")
            .arg(sys.name, igdbSlug).arg(igdbIndex.size());

        // Per-system transaction — keeps lock duration short relative to network time
        if (!database.transaction()) {
            error = QStringLiteral("Failed to start transaction for system %1: %2")
                .arg(sys.name, database.lastError().text());
            return false;
        }

        if (!upsertIgdbSource(database, snapshotId, error)) {
            database.rollback();
            return false;
        }

        QSqlQuery updateQ(database);
        updateQ.prepare(QStringLiteral(
            "UPDATE games SET "
            "description  = COALESCE(description, ?), "
            "genre        = COALESCE(genre, ?), "
            "developer    = COALESCE(developer, ?), "
            "publisher    = COALESCE(publisher, ?), "
            "release_year = COALESCE(release_year, ?) "
            "WHERE game_id = ?"));

        QSqlQuery factQ(database);
        factQ.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_facts "
            "(game_id, field_name, field_value, value_type, source_id, snapshot_id, "
            "source_priority, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

        auto nullStr = [](const QString &s) -> QVariant {
            return s.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(s);
        };

        auto insertFact = [&](const QString &gameId, const QString &field,
                               const QString &value) -> bool {
            if (value.isEmpty()) return true;
            factQ.bindValue(0, gameId);
            factQ.bindValue(1, field);
            factQ.bindValue(2, value);
            factQ.bindValue(3, QStringLiteral("text"));
            factQ.bindValue(4, QStringLiteral("igdb"));
            factQ.bindValue(5, snapshotId);
            factQ.bindValue(6, 70);
            factQ.bindValue(7, 0.80);
            if (!execPrepared(factQ, error, QStringLiteral("Insert igdb fact"))) return false;
            if (factQ.numRowsAffected() > 0) ++factsInserted;
            return true;
        };

        QSqlQuery gamesQ(database);
        gamesQ.prepare(QStringLiteral(
            "SELECT game_id, canonical_title FROM games "
            "WHERE system_id = ? AND (description IS NULL OR description = '')"));
        gamesQ.addBindValue(sys.id);
        if (!gamesQ.exec()) {
            database.rollback();
            error = QStringLiteral("Query games for system %1: %2")
                .arg(sys.name, gamesQ.lastError().text());
            return false;
        }

        int sysEnriched = 0;
        while (gamesQ.next()) {
            const QString gameId = gamesQ.value(0).toString();
            const QString norm   = normalizeTitle(gamesQ.value(1).toString());
            const auto it        = igdbIndex.constFind(norm);
            if (it == igdbIndex.cend()) continue;
            const GameMetadata &gm = it.value();

            int releaseYear = 0;
            if (gm.releaseDate.size() >= 4) {
                bool ok = false;
                const int y = gm.releaseDate.left(4).toInt(&ok);
                if (ok && y > 1970 && y < 2030) releaseYear = y;
            }

            updateQ.bindValue(0, nullStr(gm.description));
            updateQ.bindValue(1, gm.genres.isEmpty() ? nullStr({}) : QVariant(gm.genres.first()));
            updateQ.bindValue(2, nullStr(gm.developer));
            updateQ.bindValue(3, nullStr(gm.publisher));
            updateQ.bindValue(4, releaseYear > 0
                                 ? QVariant(releaseYear)
                                 : QVariant(QMetaType(QMetaType::Int)));
            updateQ.bindValue(5, gameId);
            if (!execPrepared(updateQ, error, QStringLiteral("Update game igdb"))) {
                database.rollback();
                return false;
            }
            if (updateQ.numRowsAffected() > 0) { ++gamesEnriched; ++sysEnriched; }

            const QString genreStr = gm.genres.isEmpty() ? QString() : gm.genres.first();
            if (!insertFact(gameId, QStringLiteral("description"), gm.description)
                || !insertFact(gameId, QStringLiteral("genre"),        genreStr)
                || !insertFact(gameId, QStringLiteral("developer"),    gm.developer)
                || !insertFact(gameId, QStringLiteral("publisher"),    gm.publisher)) {
                database.rollback();
                return false;
            }
        }

        if (!database.commit()) {
            error = QStringLiteral("Failed to commit enrichment for %1: %2")
                .arg(sys.name, database.lastError().text());
            return false;
        }

        qInfo().noquote() << QStringLiteral("[IGDB] %1: +%2 games enriched").arg(sys.name).arg(sysEnriched);
    }

    return true;
}

} // namespace CompendiumEnrichment
