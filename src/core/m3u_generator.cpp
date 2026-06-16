#include "m3u_generator.h"
#include "constants/files.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QDebug>
#include <algorithm>

namespace Remus {

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

    QList<FileRecord> files;
    for (int fileId : fileIds) {
        const FileRecord file = m_database.getFileById(fileId);
        if (file.id > 0)
            files.append(file);
    }
    return groupByDiscSetKey(files);
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

    int generated = 0;

    for (auto it = multiDiscGames.constBegin(); it != multiDiscGames.constEnd(); ++it) {
        QList<FileRecord> fileInfos;
        for (int fileId : it.value()) {
            const FileRecord file = m_database.getFileById(fileId);
            if (file.id > 0)
                fileInfos.append(file);
        }
        fileInfos = sortByDiscNumber(fileInfos);

        QStringList discPaths;
        for (const FileRecord &file : fileInfos)
            discPaths.append(file.currentPath);

        const QString baseTitle = titleForDiscSet(fileInfos);
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

    qInfo() << "Generated" << generated << "M3U playlists";
    return generated;
}

int M3UGenerator::generateAll(const QSet<int> &fileIds, const QString &outputDir) {
    const QMap<QString, QList<int>> multiDiscGames = detectMultiDiscGames(fileIds);
    if (multiDiscGames.isEmpty()) {
        qInfo() << "No multi-disc games found";
        return 0;
    }

    int generated = 0;

    for (auto it = multiDiscGames.constBegin(); it != multiDiscGames.constEnd(); ++it) {
        QList<FileRecord> fileInfos;
        for (int fileId : it.value()) {
            const FileRecord file = m_database.getFileById(fileId);
            if (file.id > 0)
                fileInfos.append(file);
        }
        fileInfos = sortByDiscNumber(fileInfos);

        QStringList discPaths;
        for (const FileRecord &file : fileInfos)
            discPaths.append(file.currentPath);

        const QString baseTitle = titleForDiscSet(fileInfos);
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

QList<FileRecord> M3UGenerator::sortByDiscNumber(const QList<FileRecord> &files) const {
    QList<FileRecord> sorted = files;
    std::sort(sorted.begin(), sorted.end(), [](const FileRecord &a, const FileRecord &b) {
        const int discA = a.discNumber > 0 ? a.discNumber
            : DiscSetUtils::extractDiscNumber(
                  DiscSetUtils::labelPath(a.currentPath, a.archivePath, a.archiveInternalPath, a.filename));
        const int discB = b.discNumber > 0 ? b.discNumber
            : DiscSetUtils::extractDiscNumber(
                  DiscSetUtils::labelPath(b.currentPath, b.archivePath, b.archiveInternalPath, b.filename));
        if (discA != discB)
            return discA < discB;
        return a.filename < b.filename;
    });
    return sorted;
}

QString M3UGenerator::titleForDiscSet(const QList<FileRecord> &files) const {
    for (const FileRecord &file : files) {
        if (!file.baseTitle.isEmpty())
            return file.baseTitle;
    }
    if (files.isEmpty())
        return QString();
    const FileRecord &first = files.first();
    return DiscSetUtils::extractBaseTitle(
        DiscSetUtils::labelPath(first.currentPath, first.archivePath, first.archiveInternalPath, first.filename));
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
