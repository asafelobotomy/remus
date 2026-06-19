#include "compendium_provider.h"

#include "../core/constants/confidence.h"
#include "../core/disc_title_parser.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace Remus {

namespace {

    bool discSetsTableExists(QSqlDatabase &db) {
        QSqlQuery query(db);
        if (!query.exec(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'game_disc_sets' "
                                       "LIMIT 1"))) {
            return false;
        }
        return query.next();
    }

    CompendiumDiscSet discSetFromQuery(const QSqlQuery &query) {
        CompendiumDiscSet discSet;
        discSet.discSetId = query.value(0).toLongLong();
        discSet.gameId = query.value(1).toString();
        discSet.setKey = query.value(2).toString();
        discSet.discNumber = query.value(3).toInt();
        discSet.discCount = query.value(4).toInt();
        discSet.setVariant = query.value(5).toString();
        discSet.setRole = query.value(6).toString();
        discSet.titleDisc = query.value(7).toString();
        discSet.primaryContentSha1 = query.value(8).toString();
        discSet.trackCount = query.value(9).toInt();
        return discSet;
    }

    void applyDiscSetToMetadata(GameMetadata &metadata, const CompendiumDiscSet &discSet) {
        if (discSet.setKey.isEmpty())
            return;
        metadata.setKey = discSet.setKey;
        metadata.matchedDiscNumber = discSet.discNumber;
        metadata.catalogDiscCount = discSet.discCount;
    }

} // namespace

QList<CompendiumDiscSet> CompendiumProvider::queryDiscSets(
    const QString &whereSql, const QVariantList &bindValues) const {
    QList<CompendiumDiscSet> results;
    QSqlDatabase db = database();
    if (!db.isOpen() || !discSetsTableExists(db))
        return results;

    QSqlQuery query(db);
    const QString sql
        = QStringLiteral("SELECT ds.disc_set_id, ds.game_id, ds.set_key, ds.disc_number, ds.disc_count, "
                         "ds.set_variant, ds.set_role, ds.title_disc, ds.primary_content_sha1, "
                         "(SELECT COUNT(*) FROM game_disc_tracks dt WHERE dt.disc_set_id = ds.disc_set_id) "
                         "FROM game_disc_sets ds WHERE %1 "
                         "ORDER BY ds.set_key, ds.disc_number, ds.set_variant")
              .arg(whereSql);
    query.prepare(sql);
    for (const QVariant &value : bindValues)
        query.addBindValue(value);

    if (!query.exec()) {
        qWarning() << "CompendiumProvider::queryDiscSets failed:" << query.lastError().text();
        return results;
    }

    while (query.next())
        results.append(discSetFromQuery(query));
    return results;
}

bool CompendiumProvider::lookupDiscSetBySourceEntry(const QString &sourceEntryKey, CompendiumDiscSet &discSet) const {
    discSet = { };
    if (sourceEntryKey.isEmpty())
        return false;

    QSqlDatabase db = database();
    if (!db.isOpen() || !discSetsTableExists(db))
        return false;

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT ds.disc_set_id, ds.game_id, ds.set_key, ds.disc_number, ds.disc_count, "
                                 "ds.set_variant, ds.set_role, ds.title_disc, ds.primary_content_sha1, "
                                 "(SELECT COUNT(*) FROM game_disc_tracks dt2 WHERE dt2.disc_set_id = ds.disc_set_id) "
                                 "FROM game_disc_tracks dt "
                                 "JOIN game_disc_sets ds ON ds.disc_set_id = dt.disc_set_id "
                                 "WHERE dt.source_entry_key = ? "
                                 "LIMIT 1"));
    query.addBindValue(sourceEntryKey);
    if (!query.exec()) {
        qWarning() << "CompendiumProvider::lookupDiscSetBySourceEntry failed:" << query.lastError().text();
        return false;
    }
    if (!query.next())
        return false;

    discSet = discSetFromQuery(query);
    return true;
}

void CompendiumProvider::populateDiscContextFromSourceEntry(
    GameMetadata &metadata, const QString &sourceEntryKey) const {
    CompendiumDiscSet discSet;
    if (!lookupDiscSetBySourceEntry(sourceEntryKey, discSet))
        return;
    applyDiscSetToMetadata(metadata, discSet);
}

QList<CompendiumDiscSet> CompendiumProvider::getDiscSetsForGame(const QString &gameId) const {
    if (gameId.isEmpty())
        return { };
    return queryDiscSets(QStringLiteral("ds.game_id = ?"), { gameId });
}

QList<CompendiumDiscSet> CompendiumProvider::getDiscSetsBySetKey(const QString &setKey) const {
    if (setKey.isEmpty())
        return { };
    return queryDiscSets(QStringLiteral("ds.set_key = ?"), { setKey });
}

void CompendiumProvider::applyDiscContextToMatch(const ROMSignals &input, CompendiumMultiSignalMatch &match) const {
    CompendiumDiscSet discSet;
    if (!lookupDiscSetBySourceEntry(match.sourceEntryKey, discSet))
        return;

    match.setKey = discSet.setKey;
    match.catalogDiscNumber = discSet.discNumber;
    match.catalogDiscCount = discSet.discCount;

    int inputDiscNumber = input.discNumber;
    if (inputDiscNumber <= 0 && !input.filename.isEmpty())
        inputDiscNumber = DiscTitleParser::extractDiscNumber(input.filename);

    if (inputDiscNumber > 0 && discSet.discNumber > 0 && inputDiscNumber == discSet.discNumber) {
        match.discNumberMatch = true;
        match.confidenceScore += Constants::Confidence::MultiSignal::DISC_NUMBER_BONUS;
        match.matchSignalCount++;
    }
}

} // namespace Remus
