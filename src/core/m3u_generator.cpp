#include "m3u_generator.h"
#include "compendium_disc_bridge.h"
#include "constants/files.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QDebug>
#include <QDateTime>
#include <QSqlQuery>
#include <algorithm>

namespace Remus {

namespace {

    QSqlDatabase openCatalogDatabase(Database &database, QString &connectionName) {
        const QString path = database.compendiumDbPath();
        if (path.isEmpty() || !QFileInfo::exists(path))
            return { };

        connectionName = QStringLiteral("m3u_catalog_%1").arg(QDateTime::currentMSecsSinceEpoch());
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(path);
        if (!db.open()) {
            QSqlDatabase::removeDatabase(connectionName);
            connectionName.clear();
            return { };
        }
        return db;
    }

    void closeCatalogDatabase(const QString &connectionName) {
        if (connectionName.isEmpty())
            return;
        if (QSqlDatabase::contains(connectionName)) {
            QSqlDatabase db = QSqlDatabase::database(connectionName);
            if (db.isOpen())
                db.close();
            QSqlDatabase::removeDatabase(connectionName);
        }
    }

} // namespace

M3UGenerator::M3UGenerator(Database &db, QObject *parent)
    : QObject(parent)
    , m_database(db) { }

QMap<QString, QList<int>> M3UGenerator::detectMultiDiscGames(const QString &systemName) {
    QList<FileRecord> files;
    if (systemName.isEmpty())
        files = m_database.getAllFiles();
    else
        files = m_database.getFilesBySystem(systemName);

    return groupByDiscSetKey(files);
}

QMap<QString, QList<int>> M3UGenerator::detectMultiDiscGames(const QSet<int> &fileIds) {
    if (fileIds.isEmpty())
        return detectMultiDiscGames(QString());

    return groupByDiscSetKey(m_database.getFilesByIds(fileIds));
}

bool M3UGenerator::generateM3U(const QString &gameTitle, const QStringList &discPaths, const QString &outputPath) {
    if (discPaths.isEmpty()) {
        qWarning() << "No disc paths provided for M3U generation";
        emit errorOccurred("No disc paths provided");
        return false;
    }

    QStringList relativePaths;
    QFileInfo m3uInfo(outputPath);
    QDir m3uDir = m3uInfo.absoluteDir();

    for (const QString &discPath : discPaths) {
        QFileInfo discInfo(discPath);
        relativePaths.append(m3uDir.relativeFilePath(discInfo.absoluteFilePath()));
    }

    const bool success = writeM3UFile(outputPath, relativePaths);

    if (success) {
        qInfo() << "✓ Generated M3U playlist:" << outputPath << "(" << discPaths.size() << "discs)";
        emit playlistGenerated(outputPath, discPaths.size());
    } else {
        qWarning() << "✗ Failed to generate M3U:" << outputPath;
        emit errorOccurred("Failed to write M3U file");
    }

    return success;
}

int M3UGenerator::generateAll(const QString &systemName, const QString &outputDir) {
    const QMap<QString, QList<int>> multiDiscGames = detectMultiDiscGames(systemName);
    if (multiDiscGames.isEmpty()) {
        qInfo() << "No multi-disc games found";
        return 0;
    }

    QString catalogConn;
    QSqlDatabase catalogDb = openCatalogDatabase(m_database, catalogConn);
    QSqlDatabase *catalogPtr = catalogDb.isOpen() ? &catalogDb : nullptr;

    int generated = 0;

    for (auto it = multiDiscGames.constBegin(); it != multiDiscGames.constEnd(); ++it) {
        QList<FileRecord> fileInfos;
        for (int fileId : it.value()) {
            const FileRecord file = m_database.getFileById(fileId);
            if (file.id > 0)
                fileInfos.append(file);
        }
        fileInfos = sortByDiscNumber(fileInfos, catalogPtr);

        QStringList discPaths;
        for (const FileRecord &file : fileInfos)
            discPaths.append(file.currentPath);

        const QString baseTitle = titleForDiscSet(fileInfos, catalogPtr);
        QString m3uPath;
        if (!outputDir.isEmpty()) {
            m3uPath = QDir(outputDir).filePath(baseTitle + Constants::Files::M3U);
        } else if (!discPaths.isEmpty()) {
            QFileInfo firstDisc(discPaths.first());
            m3uPath = firstDisc.absoluteDir().filePath(baseTitle + Constants::Files::M3U);
        }

        if (generateM3U(baseTitle, discPaths, m3uPath))
            ++generated;
    }

    closeCatalogDatabase(catalogConn);
    qInfo() << "Generated" << generated << "M3U playlists";
    return generated;
}

int M3UGenerator::generateAll(const QSet<int> &fileIds, const QString &outputDir) {
    const QMap<QString, QList<int>> multiDiscGames = detectMultiDiscGames(fileIds);
    if (multiDiscGames.isEmpty()) {
        qInfo() << "No multi-disc games found";
        return 0;
    }

    QString catalogConn;
    QSqlDatabase catalogDb = openCatalogDatabase(m_database, catalogConn);
    QSqlDatabase *catalogPtr = catalogDb.isOpen() ? &catalogDb : nullptr;

    int generated = 0;

    for (auto it = multiDiscGames.constBegin(); it != multiDiscGames.constEnd(); ++it) {
        QList<FileRecord> fileInfos;
        for (int fileId : it.value()) {
            const FileRecord file = m_database.getFileById(fileId);
            if (file.id > 0)
                fileInfos.append(file);
        }
        fileInfos = sortByDiscNumber(fileInfos, catalogPtr);

        QStringList discPaths;
        for (const FileRecord &file : fileInfos)
            discPaths.append(file.currentPath);

        const QString baseTitle = titleForDiscSet(fileInfos, catalogPtr);
        QString m3uPath;
        if (!outputDir.isEmpty()) {
            m3uPath = QDir(outputDir).filePath(baseTitle + Constants::Files::M3U);
        } else if (!discPaths.isEmpty()) {
            QFileInfo firstDisc(discPaths.first());
            m3uPath = firstDisc.absoluteDir().filePath(baseTitle + Constants::Files::M3U);
        }

        if (generateM3U(baseTitle, discPaths, m3uPath))
            ++generated;
    }

    closeCatalogDatabase(catalogConn);
    qInfo() << "Generated" << generated << "M3U playlists";
    return generated;
}

QMap<QString, QList<int>> M3UGenerator::groupByDiscSetKey(const QList<FileRecord> &files) const {
    QHash<QString, int> keyCounts;
    for (const FileRecord &file : files) {
        if (!file.isPrimary || file.discSetKey.isEmpty())
            continue;
        ++keyCounts[file.discSetKey];
    }

    QMap<QString, QList<int>> groups;
    for (const FileRecord &file : files) {
        if (!file.isPrimary || file.discSetKey.isEmpty())
            continue;
        if (keyCounts.value(file.discSetKey, 0) < 2)
            continue;
        groups[file.discSetKey].append(file.id);
    }

    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it)
        qInfo() << "Multi-disc set detected:" << it.key() << "(" << it.value().size() << "discs)";

    return groups;
}

int M3UGenerator::catalogDiscNumberForFile(const FileRecord &file, QSqlDatabase *compendiumDb) const {
    if (file.discNumber > 0)
        return file.discNumber;

    if (compendiumDb != nullptr && compendiumDb->isOpen()) {
        QSqlQuery sysQuery(m_database.database());
        sysQuery.prepare(QStringLiteral("SELECT name FROM systems WHERE id = ? LIMIT 1"));
        sysQuery.addBindValue(file.systemId);
        QString systemInternalName;
        if (sysQuery.exec() && sysQuery.next())
            systemInternalName = sysQuery.value(0).toString();

        CompendiumFileDiscContext catalog;
        if (lookupCompendiumDiscContextFromDb(
                *compendiumDb, systemInternalName, file.crc32, file.md5, file.sha1, catalog)
            && catalog.found && catalog.discNumber > 0) {
            return catalog.discNumber;
        }
    }

    return DiscSetUtils::extractDiscNumber(
        DiscSetUtils::labelPath(file.currentPath, file.archivePath, file.archiveInternalPath, file.filename));
}

QList<FileRecord> M3UGenerator::sortByDiscNumber(const QList<FileRecord> &files, QSqlDatabase *compendiumDb) const {
    QList<FileRecord> sorted = files;
    std::sort(sorted.begin(), sorted.end(), [this, compendiumDb](const FileRecord &a, const FileRecord &b) {
        const int discA = catalogDiscNumberForFile(a, compendiumDb);
        const int discB = catalogDiscNumberForFile(b, compendiumDb);
        if (discA != discB)
            return discA < discB;
        return a.filename < b.filename;
    });
    return sorted;
}

QString M3UGenerator::titleForDiscSet(const QList<FileRecord> &files, QSqlDatabase *compendiumDb) const {
    if (files.isEmpty())
        return QString();

    const FileRecord &first = files.first();
    if (compendiumDb != nullptr && compendiumDb->isOpen() && !first.discSetKey.isEmpty()) {
        CatalogDiscSetSummary summary;
        if (lookupCatalogDiscSetSummary(*compendiumDb, first.discSetKey, summary) && !summary.baseTitle.isEmpty())
            return summary.baseTitle;
    }

    const QString fromLabel = DiscSetUtils::extractBaseTitle(
        DiscSetUtils::labelPath(first.currentPath, first.archivePath, first.archiveInternalPath, first.filename));
    if (!fromLabel.isEmpty())
        return fromLabel;
    for (const FileRecord &file : files) {
        if (!file.baseTitle.isEmpty())
            return file.baseTitle;
    }
    return QString();
}

bool M3UGenerator::writeM3UFile(const QString &path, const QStringList &discPaths) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << path;
        return false;
    }

    QTextStream out(&file);
    for (const QString &discPath : discPaths)
        out << discPath << "\n";

    file.close();
    return true;
}

} // namespace Remus
