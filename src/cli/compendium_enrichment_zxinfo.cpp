#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "../metadata/zxinfo_provider.h"
#include "../metadata/http_metadata_provider.h"
#include "../core/constants/system_ids.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QList>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

using namespace Remus;
using namespace CompendiumEnrichmentSql;

namespace {

struct ZXMatch {
    QString gameId;
    int releaseYear = 0;
    QString releaseDate;
    QString publisher;
    QString developer;
    QString genre;
    QString description;
};

const GameMetadata *pickZXInfoMatch(const QList<GameMetadata> &results, const QString &normQuery) {
    for (const GameMetadata &gm : results) {
        if (normalizeMetadataTitle(gm.title) == normQuery)
            return &gm;
    }
    if (results.size() == 1 && !results.first().title.isEmpty())
        return &results.first();
    return nullptr;
}

QString normalizeZXReleaseDate(const QString &releaseDate) {
    const QString trimmed = releaseDate.trimmed();
    if (trimmed.isEmpty())
        return QString();
    if (trimmed.size() >= 10 && trimmed.at(4) == QLatin1Char('-'))
        return trimmed.left(10);
    bool ok = false;
    const int year = trimmed.toInt(&ok);
    if (ok && year > 1970 && year < 2030)
        return QStringLiteral("%1-01-01").arg(year);
    return QString();
}

} // namespace

namespace CompendiumEnrichment {

bool enrichFromZXInfo(QSqlDatabase &database, int &gamesEnriched, int &factsInserted, QString &error) {
    gamesEnriched = 0;
    factsInserted = 0;

    // --- Phase 1: collect ZX Spectrum games that need enrichment ----------------
    struct GameEntry {
        QString id;
        QString title;
    };
    QList<GameEntry> pending;

    {
        QSqlQuery gamesQ(database);
        if (!gamesQ.exec(QStringLiteral("SELECT game_id, canonical_title FROM games "
                                        "WHERE system_id = %1 "
                                        "  AND (genre IS NULL OR genre = '' "
                                        "    OR publisher IS NULL OR publisher = '' "
                                        "    OR release_year IS NULL "
                                        "    OR release_date IS NULL OR release_date = '' "
                                        "    OR developer IS NULL OR developer = '' "
                                        "    OR description IS NULL OR description = '')")
                    .arg(Constants::Systems::ID_ZX_SPECTRUM))) {
            error = QStringLiteral("Query ZX Spectrum games: %1").arg(gamesQ.lastError().text());
            return false;
        }
        while (gamesQ.next())
            pending.append({ gamesQ.value(0).toString(), gamesQ.value(1).toString() });
    } // gamesQ goes out of scope — no open cursor during network calls

    if (pending.isEmpty()) {
        qInfo() << "[ZXInfo] No ZX Spectrum games require enrichment";
        return true;
    }

    qInfo().noquote() << QStringLiteral("[ZXInfo] Enriching %1 ZX Spectrum games via API …").arg(pending.size());

    // --- Phase 2: network calls (no open transaction) ---------------------------
    ZXInfoProvider provider;
    QList<ZXMatch> matches;
    matches.reserve(pending.size());
    int searched = 0;

    for (const GameEntry &entry : pending) {
        ++searched;
        if (searched % 200 == 0)
            qInfo().noquote() << QStringLiteral("[ZXInfo] %1 / %2 (%3%) …")
                                     .arg(searched)
                                     .arg(pending.size())
                                     .arg(searched * 100 / pending.size());

        const QList<GameMetadata> results = provider.searchAndFetch(entry.title, 3);
        const QString normQuery = normalizeMetadataTitle(entry.title);

        const GameMetadata *gm = pickZXInfoMatch(results, normQuery);
        if (!gm)
            continue;

        ZXMatch m;
        m.gameId = entry.id;
        m.genre = gm->genres.isEmpty() ? QString() : gm->genres.first();
        m.publisher = gm->publisher;
        m.developer = gm->developer;
        m.description = gm->description;
        m.releaseDate = normalizeZXReleaseDate(gm->releaseDate);

        bool ok = false;
        const int year = gm->releaseDate.toInt(&ok);
        m.releaseYear = (ok && year > 1970 && year < 2030) ? year : 0;
        if (m.releaseYear == 0 && !m.releaseDate.isEmpty()) {
            const int derivedYear = m.releaseDate.left(4).toInt(&ok);
            m.releaseYear = (ok && derivedYear > 1970 && derivedYear < 2030) ? derivedYear : 0;
        }

        matches.append(m);
    }

    // Flush deleteLater() from network replies before opening a transaction
    HttpMetadataProvider::processNetworkEvents();

    qInfo().noquote() << QStringLiteral("[ZXInfo] %1 matches found; writing to database …").arg(matches.size());

    // --- Phase 3: batched writes ----------------------------------------------
    const QString snapshotId = QStringLiteral("zxinfo-bulk");
    const QString sourceId = QStringLiteral("zxinfo");

    if (!bulkClearSourceFactBlockers(database, sourceId, error))
        return false;

    if (!upsertEnrichmentSource(database,
            SourceSpec {
                sourceId,
                QStringLiteral("ZXInfo / ZXDB"),
                QStringLiteral("online-api"),
                QStringLiteral("https://api.zxinfo.dk/v3/"),
                /*attributionRequired=*/true,
                /*priority=*/65,
                QString(),
            },
            SnapshotSpec {
                snapshotId,
                QStringLiteral("ZXInfo API enrichment"),
            },
            error)) {
        database.rollback();
        return false;
    }

    QSqlQuery updateQ(database);
    updateQ.prepare(QStringLiteral("UPDATE games SET "
                                   "genre        = COALESCE(genre, ?), "
                                   "developer    = COALESCE(developer, ?), "
                                   "publisher    = COALESCE(publisher, ?), "
                                   "release_year = COALESCE(release_year, ?), "
                                   "release_date = COALESCE(release_date, ?), "
                                   "description  = COALESCE(description, ?) "
                                   "WHERE game_id = ?"));

    QSqlQuery factQ(database);
    factQ.prepare(QStringLiteral("INSERT INTO game_facts "
                                 "(game_id, field_name, field_value, value_type, source_id, snapshot_id, "
                                 "source_priority, confidence) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    QSqlQuery delQ(database);
    delQ.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

    static constexpr double CONFIDENCE = 0.80;
    static constexpr int PRIORITY = 65;

    const FactInsertSpec factSpec {
        sourceId,
        snapshotId,
        PRIORITY,
        CONFIDENCE,
    };
    FactReplaceQueries replaceQueries(database);
    EnrichmentBatchWriter batchWriter(database);

    auto insertFact
        = [&](const QString &gameId, const QString &field, const QString &value, const QString &type) -> bool {
        bool inserted = false;
        if (!insertGameFact(replaceQueries,
                delQ, factQ, factSpec, gameId, field, value, type, error, QStringLiteral("zxinfo"), &inserted)) {
            return false;
        }
        if (inserted)
            ++factsInserted;
        return true;
    };

    for (const ZXMatch &m : matches) {
        const QString yearStr = m.releaseYear > 0 ? QString::number(m.releaseYear) : QString();

        updateQ.bindValue(0, nullableText(m.genre));
        updateQ.bindValue(1, nullableText(m.developer));
        updateQ.bindValue(2, nullableText(m.publisher));
        updateQ.bindValue(3, nullableInt(m.releaseYear));
        updateQ.bindValue(4, nullableText(m.releaseDate));
        updateQ.bindValue(5, nullableText(m.description));
        updateQ.bindValue(6, m.gameId);

        if (!updateQ.exec()) {
            error = QStringLiteral("Update ZXInfo game %1: %2").arg(m.gameId, updateQ.lastError().text());
            return false;
        }
        if (updateQ.numRowsAffected() > 0)
            ++gamesEnriched;

        if (!insertFact(m.gameId, QStringLiteral("genre"), m.genre, QStringLiteral("text"))
            || !insertFact(m.gameId, QStringLiteral("developer"), m.developer, QStringLiteral("text"))
            || !insertFact(m.gameId, QStringLiteral("publisher"), m.publisher, QStringLiteral("text"))
            || !insertFact(m.gameId, QStringLiteral("release_year"), yearStr, QStringLiteral("integer"))
            || !insertFact(m.gameId, QStringLiteral("release_date"), m.releaseDate, QStringLiteral("text"))
            || !insertFact(m.gameId, QStringLiteral("description"), m.description, QStringLiteral("text"))) {
            if (error.isEmpty()) {
                error = QStringLiteral("Insert ZXInfo fact for game %1 failed").arg(m.gameId);
            }
            return false;
        }

        if (!batchWriter.onGameProcessed(error))
            return false;
    }

    if (!batchWriter.finish(error))
        return false;

    qInfo().noquote() << QStringLiteral("[ZXInfo] +%1 games enriched, +%2 facts").arg(gamesEnriched).arg(factsInserted);
    return true;
}

} // namespace CompendiumEnrichment
