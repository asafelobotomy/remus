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
    int     releaseYear = 0;
    QString publisher;
    QString developer;
    QString genre;
};

} // namespace

namespace CompendiumEnrichment {

bool enrichFromZXInfo(QSqlDatabase &database,
                      int &gamesEnriched,
                      int &factsInserted,
                      QString &error)
{
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
        if (!gamesQ.exec(QStringLiteral(
                "SELECT game_id, canonical_title FROM games "
                "WHERE system_id = %1 "
                "  AND (genre IS NULL OR genre = '' "
                "    OR publisher IS NULL OR publisher = '' "
                "    OR release_year IS NULL)")
                .arg(Constants::Systems::ID_ZX_SPECTRUM))) {
            error = QStringLiteral("Query ZX Spectrum games: %1")
                        .arg(gamesQ.lastError().text());
            return false;
        }
        while (gamesQ.next())
            pending.append({gamesQ.value(0).toString(), gamesQ.value(1).toString()});
    } // gamesQ goes out of scope — no open cursor during network calls

    if (pending.isEmpty()) {
        qInfo() << "[ZXInfo] No ZX Spectrum games require enrichment";
        return true;
    }

    qInfo().noquote() << QStringLiteral("[ZXInfo] Enriching %1 ZX Spectrum games via API …")
                             .arg(pending.size());

    // --- Phase 2: network calls (no open transaction) ---------------------------
    ZXInfoProvider provider;
    QList<ZXMatch> matches;
    matches.reserve(pending.size());
    int searched = 0;

    for (const GameEntry &entry : pending) {
        ++searched;
        if (searched % 200 == 0)
            qInfo().noquote() << QStringLiteral("[ZXInfo] %1 / %2 …").arg(searched).arg(pending.size());

        const QList<GameMetadata> results = provider.searchAndFetch(entry.title, 3);
        const QString normQuery = normalizeMetadataTitle(entry.title);

        for (const GameMetadata &gm : results) {
            if (normalizeMetadataTitle(gm.title) != normQuery)
                continue;

            ZXMatch m;
            m.gameId    = entry.id;
            m.genre     = gm.genres.isEmpty() ? QString() : gm.genres.first();
            m.publisher = gm.publisher;
            m.developer = gm.developer;

            bool ok  = false;
            const int year = gm.releaseDate.toInt(&ok);
            m.releaseYear = (ok && year > 1970 && year < 2030) ? year : 0;

            matches.append(m);
            break;  // first exact match wins
        }
    }

    // Flush deleteLater() from network replies before opening a transaction
    HttpMetadataProvider::processNetworkEvents();

    qInfo().noquote() << QStringLiteral("[ZXInfo] %1 matches found; writing to database …")
                             .arg(matches.size());

    // --- Phase 3: single write transaction --------------------------------------
    if (!database.transaction()) {
        error = QStringLiteral("Begin ZXInfo transaction: %1")
                    .arg(database.lastError().text());
        return false;
    }

    const QString snapshotId = QStringLiteral("zxinfo-bulk");
    if (!upsertEnrichmentSource(
            database,
            SourceSpec{
                QStringLiteral("zxinfo"),
                QStringLiteral("ZXInfo / ZXDB"),
                QStringLiteral("online-api"),
                QStringLiteral("https://api.zxinfo.dk/v3/"),
                /*attributionRequired=*/true,
                /*priority=*/65,
                QString(),
            },
            SnapshotSpec{
                snapshotId,
                QStringLiteral("ZXInfo API enrichment"),
            },
            error)) {
        database.rollback();
        return false;
    }

    QSqlQuery updateQ(database);
    updateQ.prepare(QStringLiteral(
        "UPDATE games SET "
        "genre        = COALESCE(genre, ?), "
        "developer    = COALESCE(developer, ?), "
        "publisher    = COALESCE(publisher, ?), "
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

    static constexpr double CONFIDENCE = 0.80;
    static constexpr int    PRIORITY   = 65;

    const FactInsertSpec factSpec{
        QStringLiteral("zxinfo"),
        snapshotId,
        PRIORITY,
        CONFIDENCE,
    };

    auto insertFact = [&](const QString &gameId,
                          const QString &field,
                          const QString &value,
                          const QString &type) -> bool {
        bool inserted = false;
        if (!insertGameFact(delQ,
                            factQ,
                            factSpec,
                            gameId,
                            field,
                            value,
                            type,
                            error,
                            QStringLiteral("zxinfo"),
                            &inserted)) {
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
        updateQ.bindValue(4, m.gameId);

        if (!updateQ.exec()) {
            error = QStringLiteral("Update ZXInfo game %1: %2")
                        .arg(m.gameId, updateQ.lastError().text());
            database.rollback();
            return false;
        }
        if (updateQ.numRowsAffected() > 0)
            ++gamesEnriched;

        if (!insertFact(m.gameId, QStringLiteral("genre"),        m.genre,      QStringLiteral("text"))
         || !insertFact(m.gameId, QStringLiteral("developer"),    m.developer,  QStringLiteral("text"))
         || !insertFact(m.gameId, QStringLiteral("publisher"),    m.publisher,  QStringLiteral("text"))
         || !insertFact(m.gameId, QStringLiteral("release_year"), yearStr,      QStringLiteral("integer"))) {
            if (error.isEmpty()) {
                error = QStringLiteral("Insert ZXInfo fact for game %1 failed")
                            .arg(m.gameId);
            }
            database.rollback();
            return false;
        }
    }

    if (!database.commit()) {
        error = QStringLiteral("Commit ZXInfo transaction: %1")
                    .arg(database.lastError().text());
        database.rollback();
        return false;
    }

    qInfo().noquote() << QStringLiteral("[ZXInfo] +%1 games enriched, +%2 facts")
                             .arg(gamesEnriched).arg(factsInserted);
    return true;
}

} // namespace CompendiumEnrichment
