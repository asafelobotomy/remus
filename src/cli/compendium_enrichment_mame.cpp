#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "../core/constants/system_ids.h"

#include <QDate>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

using namespace Remus;
using namespace CompendiumEnrichmentSql;

namespace {

// Parse the [Category] section of catver.ini into a romname→genre map.
// Stops reading when a section other than [Category] is encountered.
QHash<QString, QString> parseCatverIni(const QString &path, QString &error)
{
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
            if (inCategory)   // We just left [Category] — no need to read further
                break;
            inCategory = (line == QStringLiteral("[Category]"));
            continue;
        }
        if (!inCategory)
            continue;
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key   = line.left(eq);
        const QString value = line.mid(eq + 1).trimmed();
        if (!value.isEmpty() && !index.contains(key))
            index.insert(key, value);
    }
    return index;
}

} // namespace

namespace CompendiumEnrichment {

bool enrichFromMameCatver(QSqlDatabase &database,
                          const QString &catverPath,
                          int &gamesEnriched,
                          int &factsInserted,
                          QString &error)
{
    gamesEnriched = 0;
    factsInserted = 0;

    QHash<QString, QString> catver = parseCatverIni(catverPath, error);
    if (catver.isEmpty()) {
        if (!error.isEmpty())
            return false;
        qInfo() << "[MAME-catver] No entries parsed — pass skipped";
        return true;
    }

    qInfo().noquote() << QStringLiteral("[MAME-catver] Parsed %1 entries from catver.ini")
                             .arg(catver.size());

    const QString snapshotId = QStringLiteral("mame-catver-")
                             + QDate::currentDate().toString(QStringLiteral("yyyy-MM"));
    if (!upsertEnrichmentSource(
            database,
            SourceSpec{
                QStringLiteral("mame-catver"),
                QStringLiteral("MAME catver.ini"),
                QStringLiteral("static-file"),
                QStringLiteral("https://github.com/AntoPISA/MAME_SupportFiles"),
                /*attributionRequired=*/false,
                /*priority=*/50,
                QString(),
            },
            SnapshotSpec{
                snapshotId,
                QStringLiteral("MAME catver.ini genre enrichment"),
            },
            error))
        return false;

    QSqlQuery updateQ(database);
    updateQ.prepare(QStringLiteral(
        "UPDATE games SET genre = COALESCE(genre, ?) WHERE game_id = ?"));

    QSqlQuery factQ(database);
    factQ.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO game_facts "
        "(game_id, field_name, field_value, value_type, source_id, snapshot_id, "
        "source_priority, confidence) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    QSqlQuery gamesQ(database);
    gamesQ.prepare(QStringLiteral(
        "SELECT game_id, canonical_title FROM games "
        "WHERE system_id = ? AND (genre IS NULL OR genre = '')"));
    gamesQ.addBindValue(Constants::Systems::ID_ARCADE);
    if (!gamesQ.exec()) {
        error = QStringLiteral("Query MAME games: %1").arg(gamesQ.lastError().text());
        return false;
    }

    while (gamesQ.next()) {
        const QString gameId  = gamesQ.value(0).toString();
        const QString romName = gamesQ.value(1).toString();
        const auto it = catver.constFind(romName);
        if (it == catver.cend())
            continue;

        const QString genre = it.value();
        updateQ.bindValue(0, genre);
        updateQ.bindValue(1, gameId);
        if (!execPrepared(updateQ, error, QStringLiteral("Update MAME genre")))
            return false;
        if (updateQ.numRowsAffected() > 0)
            ++gamesEnriched;

        factQ.bindValue(0, gameId);
        factQ.bindValue(1, QStringLiteral("genre"));
        factQ.bindValue(2, genre);
        factQ.bindValue(3, QStringLiteral("text"));
        factQ.bindValue(4, QStringLiteral("mame-catver"));
        factQ.bindValue(5, snapshotId);
        factQ.bindValue(6, 50);
        factQ.bindValue(7, 0.90);
        if (!execPrepared(factQ, error, QStringLiteral("Insert MAME genre fact")))
            return false;
        if (factQ.numRowsAffected() > 0)
            ++factsInserted;
    }

    qInfo().noquote() << QStringLiteral("[MAME-catver] +%1 games enriched, +%2 facts")
                             .arg(gamesEnriched).arg(factsInserted);
    return true;
}

} // namespace CompendiumEnrichment
