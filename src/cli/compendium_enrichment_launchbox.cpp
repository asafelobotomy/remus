#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QXmlStreamReader>

using CompendiumEnrichmentSql::EnrichmentBatchWriter;
using CompendiumEnrichmentSql::FactInsertSpec;
using CompendiumEnrichmentSql::FactReplaceQueries;
using CompendiumEnrichmentSql::SnapshotSpec;
using CompendiumEnrichmentSql::SourceSpec;
using namespace CompendiumEnrichmentSql;

namespace CompendiumEnrichment {

namespace {

    static const char kMetadataGapSql[] = "genre IS NULL OR TRIM(genre) = '' "
                                          "   OR developer IS NULL OR TRIM(developer) = '' "
                                          "   OR publisher IS NULL OR TRIM(publisher) = '' "
                                          "   OR release_year IS NULL "
                                          "   OR release_date IS NULL OR TRIM(release_date) = '' "
                                          "   OR description IS NULL OR TRIM(description) = '' "
                                          "   OR players_max IS NULL ";

    struct LaunchBoxEntry {
        QString platform;
        QString filename;
        QString title;
        QString developer;
        QString publisher;
        QString releaseDate;
        QString overview;
        QString genre;
        int maxPlayers = 0;
        QString esrb;
    };

    QString normalizePlatformKey(const QString &platform) {
        QString key = platform.toLower();
        key.remove(QRegularExpression(QStringLiteral("[^a-z0-9]")));
        return key;
    }

    QString normalizeFilenameKey(const QString &path) {
        const QString base = QFileInfo(path.trimmed()).fileName().toLower();
        QString key = base;
        key.remove(QRegularExpression(QStringLiteral("[^a-z0-9.]")));
        return key;
    }

    QString launchBoxLookupKey(const QString &platform, const QString &filename) {
        return normalizePlatformKey(platform) + QLatin1Char('|') + normalizeFilenameKey(filename);
    }

    QString normalizeReleaseDate(const QString &raw) {
        const QString trimmed = raw.trimmed();
        if (trimmed.size() >= 10 && trimmed.at(4) == QLatin1Char('-'))
            return trimmed.left(10);
        bool ok = false;
        const int year = trimmed.toInt(&ok);
        if (ok && year > 1970 && year < 2030)
            return QStringLiteral("%1-01-01").arg(year);
        return QString();
    }

    bool buildLaunchBoxIndex(const QString &xmlPath, QHash<QString, LaunchBoxEntry> &index, QString &error) {
        QFile file(xmlPath);
        if (!file.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("Could not open LaunchBox metadata XML: %1").arg(xmlPath);
            return false;
        }

        QXmlStreamReader xml(&file);
        LaunchBoxEntry current;
        QString currentElement;
        int gameCount = 0;

        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                currentElement = xml.name().toString();
                if (currentElement == QStringLiteral("Game"))
                    current = LaunchBoxEntry { };
            } else if (xml.isCharacters() && !xml.isWhitespace()) {
                const QString text = xml.text().toString().trimmed();
                if (text.isEmpty())
                    continue;
                if (currentElement == QStringLiteral("ApplicationPath") || currentElement == QStringLiteral("FilePath"))
                    current.filename = text;
                else if (currentElement == QStringLiteral("Platform"))
                    current.platform = text;
                else if (currentElement == QStringLiteral("Title"))
                    current.title = text;
                else if (currentElement == QStringLiteral("Developer"))
                    current.developer = text;
                else if (currentElement == QStringLiteral("Publisher"))
                    current.publisher = text;
                else if (currentElement == QStringLiteral("ReleaseDate"))
                    current.releaseDate = text;
                else if (currentElement == QStringLiteral("Overview") || currentElement == QStringLiteral("Description"))
                    current.overview = text;
                else if (currentElement == QStringLiteral("Genre"))
                    current.genre = text;
                else if (currentElement == QStringLiteral("MaxPlayers")) {
                    bool ok = false;
                    const int players = text.toInt(&ok);
                    if (ok && players > 0)
                        current.maxPlayers = players;
                } else if (currentElement == QStringLiteral("ESRB"))
                    current.esrb = text;
            } else if (xml.isEndElement() && xml.name() == QStringLiteral("Game")) {
                if (!current.filename.isEmpty() && !current.platform.isEmpty()) {
                    index.insert(launchBoxLookupKey(current.platform, current.filename), current);
                    ++gameCount;
                    if (gameCount % 25000 == 0)
                        qInfo().noquote() << QStringLiteral("[LaunchBox] Indexed %1 games …").arg(gameCount);
                }
            }
        }

        if (xml.hasError()) {
            error = QStringLiteral("LaunchBox XML parse error at line %1: %2")
                        .arg(xml.lineNumber())
                        .arg(xml.errorString());
            return false;
        }

        qInfo().noquote() << QStringLiteral("[LaunchBox] Indexed %1 games from %2").arg(gameCount).arg(xmlPath);
        return !index.isEmpty();
    }

    bool platformKeysCompatible(const QString &remusPlatform, const QString &launchBoxPlatform) {
        const QString remusKey = normalizePlatformKey(remusPlatform);
        const QString lbKey = normalizePlatformKey(launchBoxPlatform);
        if (remusKey.isEmpty() || lbKey.isEmpty())
            return false;
        if (remusKey == lbKey)
            return true;

        const QStringList parts = remusPlatform.split(QStringLiteral(" / "));
        for (const QString &part : parts) {
            const QString partKey = normalizePlatformKey(part);
            if (!partKey.isEmpty() && (lbKey.contains(partKey) || partKey.contains(lbKey)))
                return true;
        }
        return remusKey.contains(lbKey) || lbKey.contains(remusKey);
    }

    const LaunchBoxEntry *lookupLaunchBoxEntry(
        const QHash<QString, LaunchBoxEntry> &index, const QString &systemName, const QString &romName) {
        const QString filenameKey = normalizeFilenameKey(romName);
        if (filenameKey.isEmpty())
            return nullptr;

        const QString exactKey = launchBoxLookupKey(systemName, romName);
        if (const auto it = index.constFind(exactKey); it != index.constEnd())
            return &(*it);

        for (auto it = index.constBegin(); it != index.constEnd(); ++it) {
            if (!it.key().endsWith(QLatin1Char('|') + filenameKey))
                continue;
            if (platformKeysCompatible(systemName, it->platform))
                return &(*it);
        }
        return nullptr;
    }

} // anonymous namespace

bool enrichFromLaunchBox(
    QSqlDatabase &database, const QString &metadataXmlPath, int &gamesEnriched, int &factsInserted, QString &error) {
    gamesEnriched = 0;
    factsInserted = 0;

    if (metadataXmlPath.isEmpty() || !QFile::exists(metadataXmlPath)) {
        qInfo() << "[LaunchBox] Metadata.xml not found — enrichment skipped";
        return true;
    }

    QHash<QString, LaunchBoxEntry> index;
    if (!buildLaunchBoxIndex(metadataXmlPath, index, error))
        return false;

    QSqlQuery pendingQ(database);
    if (!pendingQ.exec(QStringLiteral("SELECT g.game_id, s.display_name, "
                                      "       (SELECT json_extract(si.payload_json, '$.rom_name') "
                                      "        FROM game_signatures gs "
                                      "        JOIN source_items si ON si.external_key = gs.source_entry_key "
                                      "        WHERE gs.game_id = g.game_id "
                                      "          AND json_extract(si.payload_json, '$.rom_name') IS NOT NULL "
                                      "          AND TRIM(json_extract(si.payload_json, '$.rom_name')) <> '' "
                                      "        LIMIT 1) AS rom_name "
                                      "FROM games g "
                                      "JOIN systems s ON s.system_id = g.system_id "
                                      "WHERE (%1) "
                                      "  AND EXISTS ("
                                      "      SELECT 1 FROM game_signatures gs "
                                      "      JOIN source_items si ON si.external_key = gs.source_entry_key "
                                      "      WHERE gs.game_id = g.game_id "
                                      "        AND json_extract(si.payload_json, '$.rom_name') IS NOT NULL "
                                      "        AND TRIM(json_extract(si.payload_json, '$.rom_name')) <> ''"
                                      "  ) "
                                      "ORDER BY g.game_id")
                .arg(QLatin1String(kMetadataGapSql)))) {
        error = QStringLiteral("Query LaunchBox candidates: %1").arg(pendingQ.lastError().text());
        return false;
    }

    struct PendingGame {
        QString gameId;
        QString systemName;
        QString romName;
    };
    QList<PendingGame> pending;
    while (pendingQ.next()) {
        const QString romName = pendingQ.value(2).toString().trimmed();
        if (romName.isEmpty())
            continue;
        pending.append({ pendingQ.value(0).toString(), pendingQ.value(1).toString(), romName });
    }
    pendingQ.finish();

    if (pending.isEmpty()) {
        qInfo() << "[LaunchBox] No filename-linked games require enrichment";
        return true;
    }

    qInfo().noquote() << QStringLiteral("[LaunchBox] Matching %1 pending games …").arg(pending.size());

    const QString sourceId = QStringLiteral("launchbox");
    const QString snapshotId = QStringLiteral("launchbox-bulk");

    if (!bulkClearSourceFactBlockers(database, sourceId, error))
        return false;

    if (!upsertEnrichmentSource(database,
            SourceSpec {
                sourceId,
                QStringLiteral("LaunchBox Games Database"),
                QStringLiteral("static-file"),
                QStringLiteral("https://gamesdb.launchbox-app.com/"),
                /*attributionRequired=*/true,
                /*priority=*/45,
                QString(),
            },
            SnapshotSpec {
                snapshotId,
                QFileInfo(metadataXmlPath).fileName(),
            },
            error)) {
        return false;
    }

    QSqlQuery updateQ(database);
    updateQ.prepare(QStringLiteral("UPDATE games SET "
                                   "description  = COALESCE(NULLIF(description, ''), ?), "
                                   "genre        = COALESCE(NULLIF(genre, ''), ?), "
                                   "developer    = COALESCE(NULLIF(developer, ''), ?), "
                                   "publisher    = COALESCE(NULLIF(publisher, ''), ?), "
                                   "release_year = COALESCE(release_year, ?), "
                                   "release_date = COALESCE(release_date, ?), "
                                   "players_max  = COALESCE(players_max, ?), "
                                   "age_rating   = COALESCE(NULLIF(age_rating, ''), ?) "
                                   "WHERE game_id = ?"));

    QSqlQuery factQ(database);
    factQ.prepare(QStringLiteral("INSERT INTO game_facts "
                                 "(game_id, field_name, field_value, value_type, source_id, snapshot_id, "
                                 "source_priority, confidence) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    QSqlQuery delQ(database);
    delQ.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

    const FactInsertSpec factSpec {
        sourceId,
        snapshotId,
        45,
        0.70,
    };
    FactReplaceQueries replaceQueries(database);
    EnrichmentBatchWriter batchWriter(database);

    auto insertFact
        = [&](const QString &gameId, const QString &field, const QString &value,
              const QString &type = QStringLiteral("text")) -> bool {
        bool inserted = false;
        if (!insertGameFact(replaceQueries,
                delQ, factQ, factSpec, gameId, field, value, type, error, QStringLiteral("launchbox"), &inserted)) {
            return false;
        }
        if (inserted)
            ++factsInserted;
        return true;
    };

    int matched = 0;
    for (const PendingGame &game : pending) {
        const LaunchBoxEntry *entry = lookupLaunchBoxEntry(index, game.systemName, game.romName);
        if (!entry) {
            if (!batchWriter.onGameProcessed(error))
                return false;
            continue;
        }

        const QString releaseDate = normalizeReleaseDate(entry->releaseDate);
        int releaseYear = 0;
        if (releaseDate.size() >= 4) {
            bool ok = false;
            const int y = releaseDate.left(4).toInt(&ok);
            if (ok && y > 1970 && y < 2030)
                releaseYear = y;
        }

        updateQ.bindValue(0, nullableText(entry->overview));
        updateQ.bindValue(1, nullableText(entry->genre));
        updateQ.bindValue(2, nullableText(entry->developer));
        updateQ.bindValue(3, nullableText(entry->publisher));
        updateQ.bindValue(4, nullableInt(releaseYear));
        updateQ.bindValue(5, nullableText(releaseDate));
        updateQ.bindValue(6, nullableInt(entry->maxPlayers));
        updateQ.bindValue(7, nullableText(entry->esrb));
        updateQ.bindValue(8, game.gameId);
        if (!execPrepared(updateQ, error, QStringLiteral("Update game launchbox")))
            return false;
        if (updateQ.numRowsAffected() > 0)
            ++gamesEnriched;

        const QString yearStr = releaseYear > 0 ? QString::number(releaseYear) : QString();
        const QString playersStr = entry->maxPlayers > 0 ? QString::number(entry->maxPlayers) : QString();
        if (!insertFact(game.gameId, QStringLiteral("description"), entry->overview)
            || !insertFact(game.gameId, QStringLiteral("genre"), entry->genre)
            || !insertFact(game.gameId, QStringLiteral("developer"), entry->developer)
            || !insertFact(game.gameId, QStringLiteral("publisher"), entry->publisher)
            || !insertFact(game.gameId, QStringLiteral("release_year"), yearStr, QStringLiteral("integer"))
            || !insertFact(game.gameId, QStringLiteral("release_date"), releaseDate)
            || !insertFact(game.gameId, QStringLiteral("players_max"), playersStr, QStringLiteral("integer"))
            || !insertFact(game.gameId, QStringLiteral("age_rating"), entry->esrb)) {
            return false;
        }

        ++matched;
        if (!batchWriter.onGameProcessed(error))
            return false;
    }

    if (!batchWriter.finish(error))
        return false;

    qInfo().noquote() << QStringLiteral("[LaunchBox] Matched %1 / %2 pending games").arg(matched).arg(pending.size());
    return true;
}

} // namespace CompendiumEnrichment
