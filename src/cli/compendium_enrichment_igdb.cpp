#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "../metadata/igdb_provider.h"
#include "../metadata/http_metadata_provider.h"
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
using namespace CompendiumEnrichmentSql;

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

    // Systems that still have games missing any enrichable field
    QSqlQuery sysQ(database);
    if (!sysQ.exec(QStringLiteral(
            "SELECT DISTINCT g.system_id, s.display_name FROM games g "
            "JOIN systems s ON s.system_id = g.system_id "
            "WHERE g.description IS NULL OR g.description = '' "
            "   OR g.genre IS NULL OR g.genre = '' "
            "   OR g.developer IS NULL OR g.developer = '' "
            "   OR g.publisher IS NULL OR g.publisher = '' "
            "   OR g.release_year IS NULL "
            "   OR g.rating IS NULL "
            "   OR g.players_max IS NULL "
            "ORDER BY s.display_name"))) {
        error = QStringLiteral("Query systems: %1").arg(sysQ.lastError().text());
        return false;
    }
    struct SysInfo { int id; QString name; };
    QList<SysInfo> systems;
    while (sysQ.next())
        systems.append({sysQ.value(0).toInt(), sysQ.value(1).toString()});
    // Release the cursor/prepared-statement before starting per-system write
    // transactions — an open cursor can cause SQLITE_LOCKED in some SQLite
    // journal modes when the nested QEventLoop (waitForReply) re-enters.
    sysQ.finish();

    if (systems.isEmpty())
        return true;

    // Single provider (and therefore single QNAM) shared across all systems.
    // A per-system QNAM caused crashes during QNAM destruction because Qt's SSL
    // teardown is asynchronous; tearing it down at the end of every loop iteration
    // races with background socket cleanup. Instead, we flush all pending
    // deleteLater() calls at the START of each iteration (while the QNAM is still
    // alive) so reply cleanup happens before new requests are issued.
    IGDBProvider provider;
    provider.setCredentials(clientId, clientSecret);

    const QString snapshotId   = QStringLiteral("igdb-") + QDate::currentDate().toString(QStringLiteral("yyyy-MM"));
    static const int PAGE_SIZE = 500;

    for (const SysInfo &sys : systems) {
        // Flush any deleteLater() events posted by the previous system's network
        // replies before issuing new requests on the shared QNAM.
        HttpMetadataProvider::processNetworkEvents();

        const QString igdbSlug = SystemResolver::providerName(sys.id, Constants::Providers::IGDB);
        if (igdbSlug.isEmpty()) continue;

        // Bulk-fetch all IGDB games for this platform
        QHash<QString, GameMetadata> igdbIndex;
        int offset = 0;
        while (true) {
            const QList<GameMetadata> page = provider.fetchGamesByPlatformSlug(igdbSlug, offset, PAGE_SIZE);
            for (const GameMetadata &gm : page) {
                if (!gm.title.isEmpty())
                    igdbIndex[normalizeMetadataTitle(gm.title)] = gm;
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

        if (!upsertEnrichmentSource(
                database,
                SourceSpec{
                    QStringLiteral("igdb"),
                    QStringLiteral("IGDB"),
                    QStringLiteral("online-api"),
                    QStringLiteral("https://www.igdb.com"),
                    /*attributionRequired=*/true,
                    /*priority=*/70,
                    QString(),
                },
                SnapshotSpec{
                    snapshotId,
                    QStringLiteral("IGDB bulk enrichment"),
                },
                error)) {
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
            "release_year = COALESCE(release_year, ?), "
            "rating       = COALESCE(rating, ?), "
            "players_max  = COALESCE(players_max, ?) "
            "WHERE game_id = ?"));

        QSqlQuery factQ(database);
        factQ.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO game_facts "
            "(game_id, field_name, field_value, value_type, source_id, snapshot_id, "
            "source_priority, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

        const FactInsertSpec factSpec{
            QStringLiteral("igdb"),
            snapshotId,
            70,
            0.80,
        };

        auto nullStr = [](const QString &s) -> QVariant {
            return s.isEmpty() ? QVariant(QMetaType(QMetaType::QString)) : QVariant(s);
        };

        auto insertFact = [&](const QString &gameId, const QString &field,
                               const QString &value,
                               const QString &type = QStringLiteral("text")) -> bool {
            bool inserted = false;
            if (!insertGameFact(factQ,
                                factSpec,
                                gameId,
                                field,
                                value,
                                type,
                                error,
                                QStringLiteral("igdb"),
                                &inserted)) return false;
            if (inserted) ++factsInserted;
            return true;
        };

        QSqlQuery gamesQ(database);
        gamesQ.prepare(QStringLiteral(
            "SELECT game_id, canonical_title FROM games "
            "WHERE system_id = ? "
            "  AND (description IS NULL OR description = '' "
            "    OR genre IS NULL OR genre = '' "
            "    OR developer IS NULL OR developer = '' "
            "    OR publisher IS NULL OR publisher = '' "
            "    OR release_year IS NULL "
            "    OR rating IS NULL "
            "    OR players_max IS NULL)"));
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
            const QString norm   = normalizeMetadataTitle(gamesQ.value(1).toString());
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
            updateQ.bindValue(5, gm.rating > 0.0f
                                 ? QVariant(static_cast<double>(gm.rating))
                                 : QVariant(QMetaType(QMetaType::Double)));
            updateQ.bindValue(6, gm.players > 0
                                 ? QVariant(gm.players)
                                 : QVariant(QMetaType(QMetaType::Int)));
            updateQ.bindValue(7, gameId);
            if (!execPrepared(updateQ, error, QStringLiteral("Update game igdb"))) {
                database.rollback();
                return false;
            }
            if (updateQ.numRowsAffected() > 0) { ++gamesEnriched; ++sysEnriched; }

            const QString genreStr   = gm.genres.isEmpty() ? QString() : gm.genres.first();
            const QString yearStr    = releaseYear > 0 ? QString::number(releaseYear) : QString();
            const QString ratingStr  = gm.rating > 0.0f
                ? QString::number(static_cast<double>(gm.rating), 'f', 2) : QString();
            const QString playersStr = gm.players > 0 ? QString::number(gm.players) : QString();
            if (!insertFact(gameId, QStringLiteral("description"), gm.description)
                || !insertFact(gameId, QStringLiteral("genre"),        genreStr)
                || !insertFact(gameId, QStringLiteral("developer"),    gm.developer)
                || !insertFact(gameId, QStringLiteral("publisher"),    gm.publisher)
                || !insertFact(gameId, QStringLiteral("release_year"), yearStr, QStringLiteral("integer"))
                || !insertFact(gameId, QStringLiteral("rating"),       ratingStr, QStringLiteral("decimal"))
                || !insertFact(gameId, QStringLiteral("players_max"),  playersStr, QStringLiteral("integer"))) {
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

    // Flush any pending deleteLater() events from the last system's replies before
    // the single shared IGDBProvider (and its QNAM) goes out of scope at function return.
    HttpMetadataProvider::processNetworkEvents();

    return true;
}

} // namespace CompendiumEnrichment
