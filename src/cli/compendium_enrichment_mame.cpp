#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "compendium_enrichment_mame_common.h"
#include "../core/constants/system_ids.h"

#include <QDebug>
#include <QFile>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

using namespace Remus;
using namespace CompendiumEnrichmentSql;
using namespace CompendiumEnrichmentMame;

namespace {

// Parse the [Category] section of catver.ini into a romname→genre map.
// Stops reading when a section other than [Category] is encountered.
QHash<QString, QString> parseCatverIni(const QString &path, QString &error) {
    QHash<QString, QString> index;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = QStringLiteral("Cannot open catver.ini: %1").arg(file.errorString());
        return index;
    }

    QTextStream in(&file);
    bool inCategory = false;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')))
            continue;
        if (line.startsWith(QLatin1Char('['))) {
            if (inCategory) // We just left [Category] — no need to read further
                break;
            inCategory = (line == QStringLiteral("[Category]"));
            continue;
        }
        if (!inCategory)
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq);
        const QString value = line.mid(eq + 1).trimmed();
        if (!value.isEmpty() && !index.contains(key))
            index.insert(key, value);
    }
    return index;
}

QString lookupCatverGenre(const QHash<QString, QString> &catver, const QStringList &candidates) {
    for (const QString &candidate : candidates) {
        const auto it = catver.constFind(candidate);
        if (it != catver.cend())
            return it.value();
    }
    return QString();
}

} // namespace

namespace CompendiumEnrichment {

bool enrichFromMameCatver(
    QSqlDatabase &database, const QString &catverPath, int &gamesEnriched, int &factsInserted, QString &error) {
    gamesEnriched = 0;
    factsInserted = 0;

    QHash<QString, QString> catver = parseCatverIni(catverPath, error);
    if (catver.isEmpty()) {
        if (!error.isEmpty())
            return false;
        qInfo() << "[MAME-catver] No entries parsed — pass skipped";
        return true;
    }

    qInfo().noquote() << QStringLiteral("[MAME-catver] Parsed %1 entries from catver.ini").arg(catver.size());

    const QString sourceId = QStringLiteral("mame-catver");
    const QString snapshotId = QStringLiteral("mame-catver-bulk");
    if (!upsertEnrichmentSource(database,
            SourceSpec {
                sourceId,
                QStringLiteral("MAME catver.ini"),
                QStringLiteral("static-file"),
                QStringLiteral("https://github.com/AntoPISA/MAME_SupportFiles"),
                /*attributionRequired=*/false,
                /*priority=*/50,
                QString(),
            },
            SnapshotSpec {
                snapshotId,
                QStringLiteral("MAME catver.ini genre enrichment"),
            },
            error))
        return false;

    if (!bulkClearSourceFactBlockers(database, sourceId, error))
        return false;

    const QSet<QString> skipGameIds = loadGamesWithMinSourceFieldFacts(database, sourceId, 1, error);
    if (!error.isEmpty())
        return false;

    QSqlQuery updateQ(database);
    updateQ.prepare(QStringLiteral("UPDATE games SET genre = COALESCE(NULLIF(genre, ''), ?) WHERE game_id = ?"));

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
        50,
        0.90,
    };
    FactReplaceQueries replaceQueries(database);
    EnrichmentBatchWriter batchWriter(database);

    QSqlQuery gamesQ(database);
    if (!gamesQ.exec(QStringLiteral("SELECT g.game_id, g.canonical_title, gn.name_text "
                                    "FROM games g "
                                    "LEFT JOIN game_names gn ON gn.game_id = g.game_id "
                                    "WHERE g.system_id = %1 "
                                    "  AND (g.genre IS NULL OR g.genre = '') "
                                    "  AND g.canonical_title NOT LIKE '%%[ZX Spectrum]%%' "
                                    "  AND g.canonical_title NOT LIKE '%%(PSP)%%' "
                                    "  AND g.canonical_title NOT LIKE '%%in-1 %%' "
                                    "  AND g.canonical_title NOT LIKE 'Atari Flashback%%' "
                                    "ORDER BY g.game_id")
                .arg(Constants::Systems::ID_ARCADE))) {
        error = QStringLiteral("Query MAME games: %1").arg(gamesQ.lastError().text());
        return false;
    }

    QString currentGameId;
    QStringList romNameCandidates;

    auto applyGenre = [&](const QString &gameId, const QString &genre) -> bool {
        updateQ.bindValue(0, genre);
        updateQ.bindValue(1, gameId);
        if (!execPrepared(updateQ, error, QStringLiteral("Update MAME genre")))
            return false;
        if (updateQ.numRowsAffected() > 0)
            ++gamesEnriched;

        bool inserted = false;
        if (!insertGameFact(replaceQueries, delQ, factQ, factSpec, gameId, QStringLiteral("genre"), genre,
                QStringLiteral("text"), error, QStringLiteral("mame-catver"), &inserted))
            return false;
        if (inserted)
            ++factsInserted;
        return batchWriter.onGameProcessed(error);
    };

    auto flushCurrentGame = [&]() -> bool {
        if (currentGameId.isEmpty())
            return true;
        const QString gameId = currentGameId;
        const QStringList candidates = romNameCandidates;
        currentGameId.clear();
        romNameCandidates.clear();

        if (skipGameIds.contains(gameId))
            return batchWriter.onGameProcessed(error);

        const QString genre = lookupCatverGenre(catver, candidates);
        if (genre.isEmpty())
            return batchWriter.onGameProcessed(error);
        return applyGenre(gameId, genre);
    };

    while (gamesQ.next()) {
        const QString gameId = gamesQ.value(0).toString();
        const QString title = gamesQ.value(1).toString().trimmed();
        const QString alias = gamesQ.value(2).toString().trimmed();

        if (gameId != currentGameId) {
            if (!flushCurrentGame())
                return false;
            currentGameId = gameId;
            romNameCandidates.clear();
            appendRomNameCandidate(romNameCandidates, title);
        }
        appendRomNameCandidate(romNameCandidates, alias);
    }
    if (!flushCurrentGame())
        return false;

    if (!batchWriter.finish(error))
        return false;

    qInfo().noquote()
        << QStringLiteral("[MAME-catver] +%1 games enriched, +%2 facts").arg(gamesEnriched).arg(factsInserted);
    return true;
}

} // namespace CompendiumEnrichment
