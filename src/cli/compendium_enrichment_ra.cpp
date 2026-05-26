#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "../metadata/retroachievements_provider.h"
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

bool enrichFromRetroAchievements(QSqlDatabase &database,
                                  const QString &credentialsPath,
                                  int &gamesEnriched,
                                  int &factsInserted,
                                  QString &error,
                                  int *apiCallsNeededOut,
                                  int *apiCallsPerformedOut,
                                  int *apiCallsSuppressedOut)
{
    gamesEnriched = 0;
    factsInserted = 0;
    int totalApiCallsNeeded = 0;
    int totalApiCallsPerformed = 0;
    int totalApiCallsSuppressed = 0;

    // Load credentials via CredentialManager (JSON file → env var → QSettings → keychain)
    const QString username = CredentialManager::get(QStringLiteral("retroachievements/username"), credentialsPath);
    const QString apiKey   = CredentialManager::get(QStringLiteral("retroachievements/api_key"),  credentialsPath);
    if (username.isEmpty() || apiKey.isEmpty()) {
        qInfo() << "[RA] Credentials not configured — enrichment skipped";
        return true;
    }

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
    // Release cursor before per-system write transactions (same reason as IGDB).
    sysQ.finish();

    if (systems.isEmpty())
        return true;

const QString snapshotId = QStringLiteral("retroachievements-bulk");

    for (const SysInfo &sys : systems) {
        // Fresh provider (and therefore fresh QNetworkAccessManager) per system.
        // This prevents QNAM internal-state accumulation from crashing the
        // process silently when processing large systems (e.g. SNES).
        RetroAchievementsProvider provider;
        provider.setCredentials(username, apiKey);

        // RAII guard: calls HttpMetadataProvider::processNetworkEvents() before the
        // provider (and its QNAM) destructs at end of scope. Declared after `provider`
        // so it destructs first in C++ LIFO order. Without this, `continue` branches
        // (e.g. "no games", "no hash matches") exit the loop body before any per-50
        // flush, leaving deferred-delete events that fire after QNAM has already freed
        // those reply objects → use-after-free crash.
        HttpMetadataProvider::DeferredDeleteFlushGuard flushGuard;

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
        // Capture all enrichable fields to gate the per-game metadata API call.
        QSqlQuery hashQ(database);
        hashQ.prepare(QStringLiteral(
            "SELECT gs.hash_value, gs.game_id, g.genre, g.developer, g.publisher, "
            "       g.release_year, "
            "       EXISTS(SELECT 1 FROM game_facts gf "
            "              WHERE gf.game_id = g.game_id "
            "                AND gf.source_id = 'retroachievements' "
            "                AND gf.field_name = 'ra_game_id') "
            "FROM game_signatures gs "
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
            bool metaMissing     = false;
            bool conflicted      = false; // true if multiple hashes map to different RA IDs
        };
        QHash<QString, MatchInfo> matches; // compendiumGameId → MatchInfo
        while (hashQ.next()) {
            const QString hash   = hashQ.value(0).toString().toLower();
            const QString gameId = hashQ.value(1).toString();
            const bool alreadyProcessed = hashQ.value(6).toInt() != 0;
            // Fetch full metadata if any enrichable field is absent.
            auto isBlank = [&](int col) {
                return hashQ.value(col).isNull() || hashQ.value(col).toString().isEmpty();
            };
            bool metaMissing = isBlank(2)          // genre
                            || isBlank(3)          // developer
                            || isBlank(4)          // publisher
                            || hashQ.value(5).isNull(); // release_year
            // Do NOT suppress retries for already-processed games: ra_game_id may have
            // been written while the metadata API call failed, leaving enrichable fields
            // blank.  Let metaMissing stand so the API call is attempted again.
            if (alreadyProcessed && !metaMissing)
                ++totalApiCallsSuppressed;

            const auto it = md5Map.constFind(hash);
            if (it == md5Map.cend()) continue;

            auto existingIt = matches.find(gameId);
            if (existingIt == matches.end()) {
                matches.insert(gameId, {it->raGameId, it->achievementCount, metaMissing, false});
            } else if (existingIt->raGameId != it->raGameId) {
                // Same canonical game owns hashes that map to different RA games —
                // mark as conflicted so we don't write an arbitrary ra_game_id.
                existingIt->conflicted = true;
            }
        }

        if (matches.isEmpty()) {
            qInfo().noquote() << QStringLiteral("[RA] %1: no hash matches — skipping").arg(sys.name);
            continue;
        }

        // For games missing any enrichable field, fetch full metadata via API_GetGame.php.
        // Network calls are done outside the transaction to keep the write lock brief.
        // Gate on title (not description) — RA's API doesn't return narrative descriptions;
        // a non-empty title confirms the call succeeded and the metadata is usable.
        int apiCallsNeeded = 0;
        for (auto it = matches.constBegin(); it != matches.constEnd(); ++it)
            if (it->metaMissing) ++apiCallsNeeded;
        totalApiCallsNeeded += apiCallsNeeded;
        if (apiCallsNeeded > 0)
            qInfo().noquote() << QStringLiteral("[RA] %1: %2 hash matches, %3 need API calls")
                .arg(sys.name).arg(matches.size()).arg(apiCallsNeeded);

        QHash<QString, GameMetadata> fullMetadata; // compendiumGameId → metadata
        int apiCallsDone = 0;
        for (auto it = matches.constBegin(); it != matches.constEnd(); ++it) {
            if (!it->metaMissing) continue;
            ++apiCallsDone;
            if (apiCallsDone % 100 == 0)
                qInfo().noquote() << QStringLiteral("[RA] %1: API call %2/%3 ...")
                    .arg(sys.name).arg(apiCallsDone).arg(apiCallsNeeded);
            // Flush pending deleteLater() events from previous replies every 50 calls.
            // This prevents deferred-delete accumulation from triggering a use-after-free
            // in Qt's SSL background workers when making many sequential API calls on
            // the same QNAM (e.g. Game Boy: 373 calls, GBA: 232 calls).
            if (apiCallsDone % 50 == 0)
                HttpMetadataProvider::processNetworkEvents();
            const GameMetadata gm = provider.getById(QString::number(it->raGameId));
            ++totalApiCallsPerformed;
            if (!gm.title.isEmpty())
                fullMetadata.insert(it.key(), gm);
        }

        // ── Per-system transaction ────────────────────────────────────────────
        if (!database.transaction()) {
            error = QStringLiteral("Failed to start transaction for system %1: %2")
                .arg(sys.name, database.lastError().text());
            return false;
        }

        if (!upsertEnrichmentSource(
                database,
                SourceSpec{
                    QStringLiteral("retroachievements"),
                    QStringLiteral("RetroAchievements"),
                    QStringLiteral("online-api"),
                    QStringLiteral("https://retroachievements.org"),
                    /*attributionRequired=*/true,
                    /*priority=*/60,
                    QString(),
                },
                SnapshotSpec{
                    snapshotId,
                    QStringLiteral("RetroAchievements hash enrichment"),
                },
                error)) {
            database.rollback();
            return false;
        }

        QSqlQuery updateQ(database);
        updateQ.prepare(QStringLiteral(
            "UPDATE games SET "
            "description  = COALESCE(NULLIF(description, ''), ?), "
            "genre        = COALESCE(NULLIF(genre, ''), ?), "
            "developer    = COALESCE(NULLIF(developer, ''), ?), "
            "publisher    = COALESCE(NULLIF(publisher, ''), ?), "
            "release_year = COALESCE(release_year, ?) "
            "WHERE game_id = ?"));

        QSqlQuery factQ(database);
        factQ.prepare(QStringLiteral(
            "INSERT INTO game_facts "
            "(game_id, field_name, field_value, value_type, source_id, snapshot_id, "
            "source_priority, confidence) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

        QSqlQuery delQ(database);
        delQ.prepare(QStringLiteral(
            "DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

        const FactInsertSpec factSpec{
            QStringLiteral("retroachievements"),
            snapshotId,
            60,
            0.75,
        };

        auto insertFact = [&](const QString &gameId, const QString &field,
                               const QString &value,
                               const QString &type = QStringLiteral("text")) -> bool {
            bool inserted = false;
            if (!insertGameFact(delQ,
                                factQ,
                                factSpec,
                                gameId,
                                field,
                                value,
                                type,
                                error,
                                QStringLiteral("retroachievements"),
                                &inserted)) return false;
            if (inserted) ++factsInserted;
            return true;
        };

        int sysEnriched = 0;
        int sysConflicts = 0;
        for (auto it = matches.constBegin(); it != matches.constEnd(); ++it) {
            const QString &gameId = it.key();

            if (it->conflicted) {
                ++sysConflicts;
                qWarning().noquote()
                    << QStringLiteral("[RA] %1: skipping game %2 — conflicting RA hash matches")
                           .arg(sys.name, gameId);
                continue;
            }

            // Always write ra_game_id and achievement_count from the bulk list.
            if (!insertFact(gameId, QStringLiteral("ra_game_id"),
                            QString::number(it->raGameId))
                || !insertFact(gameId, QStringLiteral("achievement_count"),
                               QString::number(it->achievementCount),
                               QStringLiteral("integer"))) {
                database.rollback();
                return false;
            }

            // Write full metadata for games missing any enrichable field.
            const auto metaIt = fullMetadata.constFind(gameId);
            if (metaIt != fullMetadata.cend()) {
                const GameMetadata &gm = metaIt.value();

                int releaseYear = 0;
                if (gm.releaseDate.size() >= 4) {
                    bool okYear = false;
                    const int y = gm.releaseDate.left(4).toInt(&okYear);
                    if (okYear && y > 1970 && y < 2030) releaseYear = y;
                }

                updateQ.bindValue(0, nullableText(gm.description));
                updateQ.bindValue(1, gm.genres.isEmpty() ? nullableText(QString()) : QVariant(gm.genres.first()));
                updateQ.bindValue(2, nullableText(gm.developer));
                updateQ.bindValue(3, nullableText(gm.publisher));
                updateQ.bindValue(4, nullableInt(releaseYear));
                updateQ.bindValue(5, gameId);
                if (!execPrepared(updateQ, error, QStringLiteral("Update game RA"))) {
                    database.rollback();
                    return false;
                }
                if (updateQ.numRowsAffected() > 0) { ++gamesEnriched; ++sysEnriched; }

                const QString genreStr = gm.genres.isEmpty() ? QString() : gm.genres.first();
                const QString yearFactStr = releaseYear > 0 ? QString::number(releaseYear) : QString();
                if (!insertFact(gameId, QStringLiteral("description"),  gm.description)
                    || !insertFact(gameId, QStringLiteral("genre"),        genreStr)
                    || !insertFact(gameId, QStringLiteral("developer"),    gm.developer)
                    || !insertFact(gameId, QStringLiteral("publisher"),    gm.publisher)
                    || !insertFact(gameId, QStringLiteral("release_year"), yearFactStr, QStringLiteral("integer"))) {
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

        // Flush all pending deleteLater() calls for this system's QNetworkReply
        // objects before the RetroAchievementsProvider (and its QNAM) goes out of
        // scope. (DeferredDeleteFlushGuard above also handles early-exit paths.)
        HttpMetadataProvider::processNetworkEvents();
    }

    if (apiCallsNeededOut) *apiCallsNeededOut = totalApiCallsNeeded;
    if (apiCallsPerformedOut) *apiCallsPerformedOut = totalApiCallsPerformed;
    if (apiCallsSuppressedOut) *apiCallsSuppressedOut = totalApiCallsSuppressed;

    return true;
}

} // namespace CompendiumEnrichment
