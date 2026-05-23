#include "compendium_enrichment.h"
#include "../metadata/retroachievements_provider.h"
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

bool upsertRaSource(QSqlDatabase &database, const QString &snapshotId, QString &error)
{
    QSqlQuery srcQ(database);
    srcQ.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO sources "
        "(source_id, display_name, source_type, license_id, license_url, "
        "attribution_required, priority, enabled) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    srcQ.addBindValue(QStringLiteral("retroachievements"));
    srcQ.addBindValue(QStringLiteral("RetroAchievements"));
    srcQ.addBindValue(QStringLiteral("online-api"));
    srcQ.addBindValue(QVariant(QMetaType(QMetaType::QString)));
    srcQ.addBindValue(QStringLiteral("https://retroachievements.org"));
    srcQ.addBindValue(1);   // attribution_required
    srcQ.addBindValue(60);  // priority (Constants::Priority::RETROACHIEVEMENTS)
    srcQ.addBindValue(1);   // enabled
    if (!execPrepared(srcQ, error, QStringLiteral("Upsert retroachievements source")))
        return false;

    QSqlQuery snapQ(database);
    snapQ.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO source_snapshots "
        "(snapshot_id, source_id, snapshot_label, snapshot_ref, fetched_at, checksum_sha256) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    snapQ.addBindValue(snapshotId);
    snapQ.addBindValue(QStringLiteral("retroachievements"));
    snapQ.addBindValue(QStringLiteral("RetroAchievements hash enrichment"));
    snapQ.addBindValue(QVariant(QMetaType(QMetaType::QString)));
    snapQ.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    snapQ.addBindValue(QVariant(QMetaType(QMetaType::QString)));
    return execPrepared(snapQ, error, QStringLiteral("Upsert retroachievements snapshot"));
}

} // namespace

namespace CompendiumEnrichment {

bool enrichFromRetroAchievements(QSqlDatabase &database,
                                  const QString &credentialsPath,
                                  int &gamesEnriched,
                                  int &factsInserted,
                                  QString &error)
{
    gamesEnriched = 0;
    factsInserted = 0;

    // Load credentials via CredentialManager (JSON file → env var → QSettings → keychain)
    const QString username = CredentialManager::get(QStringLiteral("retroachievements/username"), credentialsPath);
    const QString apiKey   = CredentialManager::get(QStringLiteral("retroachievements/api_key"),  credentialsPath);
    if (username.isEmpty() || apiKey.isEmpty()) {
        qInfo() << "[RA] Credentials not configured — enrichment skipped";
        return true;
    }

    RetroAchievementsProvider provider;
    provider.setCredentials(username, apiKey);

    // Query all systems that have games with at least one MD5 signature
    QSqlQuery sysQ(database);
    if (!sysQ.exec(QStringLiteral(
            "SELECT DISTINCT g.system_id, s.display_name FROM games g "
            "JOIN systems s ON s.system_id = g.system_id "
            "JOIN game_signatures gs ON gs.game_id = g.game_id "
            "WHERE gs.hash_type = 'md5' "
            "ORDER BY s.display_name"))) {
        error = QStringLiteral("Query systems with MD5 signatures: %1").arg(sysQ.lastError().text());
        return false;
    }

    struct SysInfo { int id; QString name; };
    QList<SysInfo> systems;
    while (sysQ.next())
        systems.append({sysQ.value(0).toInt(), sysQ.value(1).toString()});

    if (systems.isEmpty())
        return true;

    const QString snapshotId = QStringLiteral("retroachievements-") +
                               QDate::currentDate().toString(QStringLiteral("yyyy-MM"));

    for (const SysInfo &sys : systems) {
        const QString raSystemIdStr = SystemResolver::providerName(
            sys.id, QString::fromLatin1(Constants::Providers::RETROACHIEVEMENTS));
        if (raSystemIdStr.isEmpty()) continue;

        bool idOk = false;
        const int raSystemId = raSystemIdStr.toInt(&idOk);
        if (!idOk || raSystemId <= 0) continue;

        // Bulk-fetch all games for this RA system with their MD5 hashes (1 API call).
        const QList<RetroAchievementsProvider::RAGameListEntry> raGames =
            provider.fetchGameListBySystemId(raSystemId);

        if (raGames.isEmpty()) {
            qInfo().noquote() << QStringLiteral("[RA] %1: no games returned — skipping").arg(sys.name);
            continue;
        }

        // Check that the API response actually includes hashes (h=1 support).
        bool apiSupportsHashes = false;
        for (const auto &entry : raGames) {
            if (!entry.md5Hashes.isEmpty()) { apiSupportsHashes = true; break; }
        }
        if (!apiSupportsHashes) {
            qInfo().noquote() << QStringLiteral("[RA] %1: API response missing hashes — skipping").arg(sys.name);
            continue;
        }

        // Build: md5 (lower-case) → {raGameId, achievementCount}
        struct RaEntry { int raGameId = 0; int achievementCount = 0; };
        QHash<QString, RaEntry> md5Map;
        md5Map.reserve(raGames.size() * 2);
        for (const auto &entry : raGames) {
            for (const QString &hash : entry.md5Hashes)
                md5Map.insert(hash.toLower(), {entry.gameId, entry.achievementCount});
        }
        qInfo().noquote() << QStringLiteral("[RA] %1 (system %2): %3 games, %4 hashes indexed")
            .arg(sys.name).arg(raSystemId).arg(raGames.size()).arg(md5Map.size());

        // Find compendium games for this system that have MD5 signatures.
        // Capture whether the description is already filled to avoid unnecessary API calls.
        QSqlQuery hashQ(database);
        hashQ.prepare(QStringLiteral(
            "SELECT gs.hash_value, gs.game_id, g.description FROM game_signatures gs "
            "JOIN games g ON g.game_id = gs.game_id "
            "WHERE gs.hash_type = 'md5' AND g.system_id = ?"));
        hashQ.addBindValue(sys.id);
        if (!hashQ.exec()) {
            error = QStringLiteral("Query MD5 hashes for system %1: %2")
                .arg(sys.name, hashQ.lastError().text());
            return false;
        }

        struct MatchInfo {
            int raGameId         = 0;
            int achievementCount = 0;
            bool descMissing     = false;
        };
        QHash<QString, MatchInfo> matches; // compendiumGameId → MatchInfo
        while (hashQ.next()) {
            const QString hash   = hashQ.value(0).toString().toLower();
            const QString gameId = hashQ.value(1).toString();
            const bool descMissing =
                hashQ.value(2).isNull() || hashQ.value(2).toString().isEmpty();

            const auto it = md5Map.constFind(hash);
            if (it == md5Map.cend()) continue;

            // First matching hash wins; don't overwrite an existing match.
            if (!matches.contains(gameId))
                matches.insert(gameId, {it->raGameId, it->achievementCount, descMissing});
        }

        if (matches.isEmpty()) {
            qInfo().noquote() << QStringLiteral("[RA] %1: no hash matches — skipping").arg(sys.name);
            continue;
        }

        // For games missing descriptions, fetch full metadata via API_GetGame.php.
        // Network calls are done outside the transaction to keep the write lock brief.
        QHash<QString, GameMetadata> fullMetadata; // compendiumGameId → metadata
        for (auto it = matches.constBegin(); it != matches.constEnd(); ++it) {
            if (!it->descMissing) continue;
            const GameMetadata gm = provider.getById(QString::number(it->raGameId));
            if (!gm.description.isEmpty())
                fullMetadata.insert(it.key(), gm);
        }

        // ── Per-system transaction ────────────────────────────────────────────
        if (!database.transaction()) {
            error = QStringLiteral("Failed to start transaction for system %1: %2")
                .arg(sys.name, database.lastError().text());
            return false;
        }

        if (!upsertRaSource(database, snapshotId, error)) {
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
                               const QString &value,
                               const QString &type = QStringLiteral("text")) -> bool {
            if (value.isEmpty()) return true;
            factQ.bindValue(0, gameId);
            factQ.bindValue(1, field);
            factQ.bindValue(2, value);
            factQ.bindValue(3, type);
            factQ.bindValue(4, QStringLiteral("retroachievements"));
            factQ.bindValue(5, snapshotId);
            factQ.bindValue(6, 60);    // source_priority
            factQ.bindValue(7, 0.75);  // confidence
            if (!execPrepared(factQ, error, QStringLiteral("Insert RA fact"))) return false;
            if (factQ.numRowsAffected() > 0) ++factsInserted;
            return true;
        };

        int sysEnriched = 0;
        for (auto it = matches.constBegin(); it != matches.constEnd(); ++it) {
            const QString &gameId = it.key();

            // Always write ra_game_id and achievement_count from the bulk list.
            if (!insertFact(gameId, QStringLiteral("ra_game_id"),
                            QString::number(it->raGameId))
                || !insertFact(gameId, QStringLiteral("achievement_count"),
                               QString::number(it->achievementCount),
                               QStringLiteral("integer"))) {
                database.rollback();
                return false;
            }

            // Write full metadata for games that needed description fill-in.
            const auto metaIt = fullMetadata.constFind(gameId);
            if (metaIt != fullMetadata.cend()) {
                const GameMetadata &gm = metaIt.value();

                int releaseYear = 0;
                if (gm.releaseDate.size() >= 4) {
                    bool okYear = false;
                    const int y = gm.releaseDate.left(4).toInt(&okYear);
                    if (okYear && y > 1970 && y < 2030) releaseYear = y;
                }

                updateQ.bindValue(0, nullStr(gm.description));
                updateQ.bindValue(1, gm.genres.isEmpty() ? nullStr({}) : QVariant(gm.genres.first()));
                updateQ.bindValue(2, nullStr(gm.developer));
                updateQ.bindValue(3, nullStr(gm.publisher));
                updateQ.bindValue(4, releaseYear > 0
                                     ? QVariant(releaseYear)
                                     : QVariant(QMetaType(QMetaType::Int)));
                updateQ.bindValue(5, gameId);
                if (!execPrepared(updateQ, error, QStringLiteral("Update game RA"))) {
                    database.rollback();
                    return false;
                }
                if (updateQ.numRowsAffected() > 0) { ++gamesEnriched; ++sysEnriched; }

                const QString genreStr = gm.genres.isEmpty() ? QString() : gm.genres.first();
                if (!insertFact(gameId, QStringLiteral("description"), gm.description)
                    || !insertFact(gameId, QStringLiteral("genre"),     genreStr)
                    || !insertFact(gameId, QStringLiteral("developer"), gm.developer)
                    || !insertFact(gameId, QStringLiteral("publisher"), gm.publisher)) {
                    database.rollback();
                    return false;
                }
            }
        }

        if (!database.commit()) {
            error = QStringLiteral("Failed to commit RA enrichment for %1: %2")
                .arg(sys.name, database.lastError().text());
            return false;
        }

        qInfo().noquote() << QStringLiteral("[RA] %1: +%2 metadata updated, %3 hash matches")
            .arg(sys.name).arg(sysEnriched).arg(matches.size());
    }

    return true;
}

} // namespace CompendiumEnrichment
