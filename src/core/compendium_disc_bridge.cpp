#include "compendium_disc_bridge.h"

#include "disc_title_parser.h"
#include "compendium_sql_pragmas.h"

#include <QDateTime>
#include <algorithm>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace Remus {

namespace {

    QString normalizeCrc32(const QString &value) {
        return value.trimmed().toUpper().remove(QLatin1Char(' '));
    }

    QString normalizeHexLower(const QString &value) {
        return value.trimmed().toLower();
    }

    bool openReadOnlyCompendium(const QString &compendiumDbPath, QSqlDatabase &db, QString &connectionName) {
        if (compendiumDbPath.isEmpty() || !QFileInfo::exists(compendiumDbPath))
            return false;

        connectionName = QStringLiteral("compendium_bridge_%1").arg(QDateTime::currentMSecsSinceEpoch());
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(compendiumDbPath);
        if (!db.open()) {
            QSqlDatabase::removeDatabase(connectionName);
            connectionName.clear();
            return false;
        }
        CompendiumSql::applyReadOnlyPragmas(db);
        return true;
    }

    bool lookupDiscContextInDb(QSqlDatabase &compendiumDb, const QString &systemInternalName, const QString &crc32,
        const QString &md5, const QString &sha1, CompendiumFileDiscContext &out) {
        out = { };
        if (!compendiumDiscSetsAvailable(compendiumDb))
            return false;

        const QString normCrc = normalizeCrc32(crc32);
        const QString normMd5 = normalizeHexLower(md5);
        const QString normSha1 = normalizeHexLower(sha1);
        if (normCrc.isEmpty() && normMd5.isEmpty() && normSha1.isEmpty())
            return false;

        QSqlQuery query(compendiumDb);
        query.prepare(QStringLiteral(R"(
            SELECT ds.set_key, ds.disc_number, ds.disc_count, ds.game_id, ds.set_variant, ds.title_disc
            FROM game_signatures gs
            JOIN game_disc_tracks dt ON dt.source_entry_key = gs.source_entry_key
            JOIN game_disc_sets ds ON ds.disc_set_id = dt.disc_set_id
            JOIN games g ON g.game_id = gs.game_id
            JOIN systems s ON s.system_id = g.system_id
            WHERE (? = '' OR s.internal_name = ?)
              AND (
                    (? != '' AND gs.hash_type = 'crc32' AND gs.hash_value = ?)
                 OR (? != '' AND gs.hash_type = 'md5' AND gs.hash_value = ?)
                 OR (? != '' AND gs.hash_type = 'sha1' AND gs.hash_value = ?)
              )
            LIMIT 1
        )"));
        query.addBindValue(systemInternalName);
        query.addBindValue(systemInternalName);
        query.addBindValue(normCrc);
        query.addBindValue(normCrc);
        query.addBindValue(normMd5);
        query.addBindValue(normMd5);
        query.addBindValue(normSha1);
        query.addBindValue(normSha1);
        if (!query.exec() || !query.next())
            return false;

        out.found = true;
        out.setKey = query.value(0).toString();
        out.discNumber = query.value(1).toInt();
        out.discCount = query.value(2).toInt();
        out.compendiumGameId = query.value(3).toString();
        out.setVariant = query.value(4).toString();
        out.titleDisc = query.value(5).toString();
        return true;
    }

    CompendiumFileDiscContext lookupLibraryFileDiscContext(
        QSqlDatabase &compendiumDb, QSqlDatabase &libraryDb, int fileId) {
        CompendiumFileDiscContext out;
        QSqlQuery query(libraryDb);
        query.prepare(QStringLiteral(R"(
            SELECT s.name, f.crc32, f.md5, f.sha1, f.disc_number
            FROM files f
            LEFT JOIN systems s ON s.id = f.system_id
            WHERE f.id = ?
        )"));
        query.addBindValue(fileId);
        if (!query.exec() || !query.next())
            return out;

        const QString systemName = query.value(0).toString();
        const int fileDiscNumber = query.value(4).toInt();
        if (!lookupDiscContextInDb(compendiumDb, systemName, query.value(1).toString(), query.value(2).toString(),
                query.value(3).toString(), out)) {
            return out;
        }

        if (fileDiscNumber > 0 && out.discNumber > 0 && fileDiscNumber != out.discNumber) {
            // Caller may surface this as a warning; context is still valid.
        }
        return out;
    }

    QSet<QString> libraryHashSet(QSqlDatabase &libraryDb, const QList<int> &ownedFileIds) {
        QSet<QString> hashes;
        if (ownedFileIds.isEmpty())
            return hashes;

        QStringList placeholders;
        placeholders.reserve(ownedFileIds.size());
        for (int i = 0; i < ownedFileIds.size(); ++i)
            placeholders.append(QStringLiteral("?"));

        QSqlQuery query(libraryDb);
        query.prepare(QStringLiteral("SELECT crc32, md5, sha1 FROM files WHERE id IN (%1) AND hash_calculated = 1")
                .arg(placeholders.join(QLatin1Char(','))));
        for (int fileId : ownedFileIds)
            query.addBindValue(fileId);

        if (!query.exec())
            return hashes;

        while (query.next()) {
            const QString crc = normalizeCrc32(query.value(0).toString());
            const QString md5 = normalizeHexLower(query.value(1).toString());
            const QString sha1 = normalizeHexLower(query.value(2).toString());
            if (!crc.isEmpty())
                hashes.insert(QStringLiteral("crc32:") + crc);
            if (!md5.isEmpty())
                hashes.insert(QStringLiteral("md5:") + md5);
            if (!sha1.isEmpty())
                hashes.insert(QStringLiteral("sha1:") + sha1);
        }
        return hashes;
    }

    DiscTrackCompleteness trackGapForDiscSet(
        QSqlDatabase &compendiumDb, qint64 discSetId, int discNumber, const QSet<QString> &ownedHashes) {
        DiscTrackCompleteness gap;
        gap.discNumber = discNumber;

        QSqlQuery query(compendiumDb);
        query.prepare(QStringLiteral(R"(
            SELECT dt.rom_name, gs.hash_type, gs.hash_value
            FROM game_disc_tracks dt
            LEFT JOIN game_signatures gs ON gs.signature_id = dt.signature_id
            WHERE dt.disc_set_id = ?
            ORDER BY dt.track_index
        )"));
        query.addBindValue(discSetId);
        if (!query.exec())
            return gap;

        while (query.next()) {
            ++gap.expectedTracks;
            const QString romName = query.value(0).toString();
            const QString hashType = query.value(1).toString();
            const QString hashValue = query.value(2).toString();
            if (hashType.isEmpty() || hashValue.isEmpty()) {
                gap.missingRomNames.append(romName);
                continue;
            }

            const QString key = hashType + QLatin1Char(':')
                + (hashType == QLatin1String("crc32") ? normalizeCrc32(hashValue) : normalizeHexLower(hashValue));
            if (ownedHashes.contains(key))
                ++gap.ownedTracks;
            else
                gap.missingRomNames.append(romName);
        }
        return gap;
    }

    DiscSetCompletenessReport buildReportForSetKey(
        QSqlDatabase &compendiumDb, QSqlDatabase &libraryDb, const QString &setKey, const QList<int> &ownedFileIds) {
        DiscSetCompletenessReport report;
        report.setKey = setKey;
        if (setKey.isEmpty() || !compendiumDiscSetsAvailable(compendiumDb))
            return report;

        QSqlQuery expectedQuery(compendiumDb);
        expectedQuery.prepare(QStringLiteral("SELECT disc_set_id, disc_number, disc_count, game_id, title_disc "
                                             "FROM game_disc_sets WHERE set_key = ? ORDER BY disc_number"));
        expectedQuery.addBindValue(setKey);
        if (!expectedQuery.exec())
            return report;

        QHash<int, qint64> discSetIdByNumber;
        while (expectedQuery.next()) {
            const qint64 discSetId = expectedQuery.value(0).toLongLong();
            const int discNumber = expectedQuery.value(1).toInt();
            report.discCount = qMax(report.discCount, expectedQuery.value(2).toInt());
            if (report.compendiumGameId.isEmpty())
                report.compendiumGameId = expectedQuery.value(3).toString();
            if (report.titleDisc.isEmpty())
                report.titleDisc = expectedQuery.value(4).toString();
            discSetIdByNumber.insert(discNumber, discSetId);
        }

        if (report.discCount <= 0)
            report.discCount = discSetIdByNumber.size();

        QSet<int> ownedDiscs;
        for (int fileId : ownedFileIds) {
            const CompendiumFileDiscContext ctx = lookupLibraryFileDiscContext(compendiumDb, libraryDb, fileId);
            if (!ctx.found || ctx.setKey != setKey)
                continue;

            ownedDiscs.insert(ctx.discNumber);

            QSqlQuery fileDiscQuery(libraryDb);
            fileDiscQuery.prepare(QStringLiteral("SELECT disc_number FROM files WHERE id = ?"));
            fileDiscQuery.addBindValue(fileId);
            if (fileDiscQuery.exec() && fileDiscQuery.next()) {
                const int libraryDiscNumber = fileDiscQuery.value(0).toInt();
                if (libraryDiscNumber > 0 && ctx.discNumber > 0 && libraryDiscNumber != ctx.discNumber) {
                    report.warnings.append(QStringLiteral("File %1: library disc %2, catalog disc %3")
                            .arg(fileId)
                            .arg(libraryDiscNumber)
                            .arg(ctx.discNumber));
                }
            }
        }

        report.ownedDiscNumbers = ownedDiscs.values();
        std::sort(report.ownedDiscNumbers.begin(), report.ownedDiscNumbers.end());

        for (int disc = 1; disc <= report.discCount; ++disc) {
            if (!ownedDiscs.contains(disc))
                report.missingDiscNumbers.append(disc);
        }

        const QSet<QString> ownedHashes = libraryHashSet(libraryDb, ownedFileIds);
        for (auto it = discSetIdByNumber.cbegin(), end = discSetIdByNumber.cend(); it != end; ++it) {
            if (!ownedDiscs.contains(it.key()))
                continue;
            const DiscTrackCompleteness gap = trackGapForDiscSet(compendiumDb, it.value(), it.key(), ownedHashes);
            if (gap.expectedTracks > 0 && gap.ownedTracks < gap.expectedTracks)
                report.trackGaps.append(gap);
        }

        return report;
    }

} // namespace

QString resolveBundledCompendiumDbPath() {
    if (qgetenv("REMUS_TEST_NO_BUNDLED_COMPENDIUM") == "1") {
        return QString();
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    const QStringList candidates = {
        cwd + QStringLiteral("/data/compendium/remus_compendium.db"),
        appDir + QStringLiteral("/data/compendium/remus_compendium.db"),
        appDir + QStringLiteral("/../data/compendium/remus_compendium.db"),
        appDir + QStringLiteral("/../../data/compendium/remus_compendium.db"),
        appDir + QStringLiteral("/../../../data/compendium/remus_compendium.db"),
        cwd + QStringLiteral("/../data/compendium/remus_compendium.db"),
        cwd + QStringLiteral("/../../data/compendium/remus_compendium.db"),
    };

    for (const QString &candidate : candidates) {
        const QString cleaned = QDir::cleanPath(candidate);
        if (QFileInfo::exists(cleaned))
            return cleaned;
    }
    return { };
}

bool lookupCompendiumDiscContextFromDb(QSqlDatabase &compendiumDb, const QString &systemInternalName,
    const QString &crc32, const QString &md5, const QString &sha1, CompendiumFileDiscContext &out) {
    return lookupDiscContextInDb(compendiumDb, systemInternalName, crc32, md5, sha1, out);
}

bool lookupCompendiumDiscContext(const QString &compendiumDbPath, const QString &systemInternalName,
    const QString &crc32, const QString &md5, const QString &sha1, CompendiumFileDiscContext &out) {
    QString connectionName;
    QSqlDatabase db;
    if (!openReadOnlyCompendium(compendiumDbPath, db, connectionName))
        return false;

    const bool found = lookupDiscContextInDb(db, systemInternalName, crc32, md5, sha1, out);
    db.close();
    QSqlDatabase::removeDatabase(connectionName);
    return found;
}

bool compendiumDiscSetsAvailable(QSqlDatabase &compendiumDb) {
    QSqlQuery query(compendiumDb);
    if (!query.exec(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'game_disc_sets' "
                                   "LIMIT 1")))
        return false;
    return query.next();
}

bool lookupCatalogDiscSetSummary(QSqlDatabase &compendiumDb, const QString &setKey, CatalogDiscSetSummary &out) {
    out = { };
    if (setKey.isEmpty() || !compendiumDiscSetsAvailable(compendiumDb))
        return false;

    QSqlQuery query(compendiumDb);
    query.prepare(QStringLiteral("SELECT disc_number, disc_count, title_disc "
                                 "FROM game_disc_sets WHERE set_key = ? ORDER BY disc_number"));
    query.addBindValue(setKey);
    if (!query.exec())
        return false;

    int maxDiscCount = 0;
    QSet<int> discNumbers;
    QString titleDisc;
    while (query.next()) {
        const int discNumber = query.value(0).toInt();
        maxDiscCount = qMax(maxDiscCount, query.value(1).toInt());
        if (discNumber > 0)
            discNumbers.insert(discNumber);
        if (titleDisc.isEmpty())
            titleDisc = query.value(2).toString();
    }
    if (discNumbers.isEmpty())
        return false;

    out.found = true;
    out.titleDisc = titleDisc;
    out.catalogDiscCount = maxDiscCount > 0 ? maxDiscCount : discNumbers.size();
    const DiscTitleInfo parsed = DiscTitleParser::parseTitle(titleDisc);
    out.baseTitle = parsed.baseTitle.isEmpty() ? DiscTitleParser::extractBaseTitle(titleDisc) : parsed.baseTitle;
    return true;
}

QList<DiscSetCompletenessReport> computeDiscSetCompleteness(QSqlDatabase &compendiumDb, QSqlDatabase &libraryDb,
    const QString &compendiumGameId, const QList<int> &ownedFileIds) {
    QList<DiscSetCompletenessReport> reports;
    if (compendiumGameId.isEmpty() || !compendiumDiscSetsAvailable(compendiumDb))
        return reports;

    QSqlQuery query(compendiumDb);
    query.prepare(QStringLiteral("SELECT DISTINCT set_key FROM game_disc_sets WHERE game_id = ? ORDER BY set_key"));
    query.addBindValue(compendiumGameId);
    if (!query.exec())
        return reports;

    while (query.next()) {
        const DiscSetCompletenessReport report
            = buildReportForSetKey(compendiumDb, libraryDb, query.value(0).toString(), ownedFileIds);
        if (!report.setKey.isEmpty())
            reports.append(report);
    }
    return reports;
}

DiscSetCompletenessReport computeDiscSetCompletenessBySetKey(
    QSqlDatabase &compendiumDb, QSqlDatabase &libraryDb, const QString &setKey, const QList<int> &ownedFileIds) {
    return buildReportForSetKey(compendiumDb, libraryDb, setKey, ownedFileIds);
}

} // namespace Remus
