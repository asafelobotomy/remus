#include "m3u_generator.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

namespace Remus {

M3UGenerator::M3UGenerator(Database &db, QObject *parent)
    : QObject(parent)
    , m_database(db)
{
}

QMap<QString, QList<int>> M3UGenerator::detectMultiDiscGames(const QString &systemName)
{
    QMap<QString, QList<int>> multiDiscGames;

    // Get all files, optionally filtered by system
    QList<FileRecord> files;
    
    if (systemName.isEmpty()) {
        files = m_database.getAllFiles();
    } else {
        files = m_database.getFilesBySystem(systemName);
    }

    // Group by base title
    QMap<QString, QList<FileRecord>> grouped = groupByBaseTitle(files);

    // Filter to only multi-disc games (2+ discs)
    for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it) {
        if (it.value().size() >= 2) {
            QList<int> fileIds;
            for (const FileRecord &file : it.value()) {
                fileIds.append(file.id);
            }
            multiDiscGames[it.key()] = fileIds;
            qInfo() << "Multi-disc game detected:" << it.key() 
                    << "(" << it.value().size() << "discs)";
        }
    }

    return multiDiscGames;
}

bool M3UGenerator::generateM3U(const QString &gameTitle,
                              const QStringList &discPaths,
                              const QString &outputPath)
{
    if (discPaths.isEmpty()) {
        qWarning() << "No disc paths provided for M3U generation";
        emit errorOccurred("No disc paths provided");
        return false;
    }

    // Convert to relative paths if they're in the same directory as M3U
    QStringList relativePaths;
    QFileInfo m3uInfo(outputPath);
    QDir m3uDir = m3uInfo.absoluteDir();

    for (const QString &discPath : discPaths) {
        QFileInfo discInfo(discPath);
        QString relativePath = m3uDir.relativeFilePath(discInfo.absoluteFilePath());
        relativePaths.append(relativePath);
    }

    bool success = writeM3UFile(outputPath, relativePaths);

    if (success) {
        qInfo() << "✓ Generated M3U playlist:" << outputPath 
                << "(" << discPaths.size() << "discs)";
        emit playlistGenerated(outputPath, discPaths.size());
    } else {
        qWarning() << "✗ Failed to generate M3U:" << outputPath;
        emit errorOccurred("Failed to write M3U file");
    }

    return success;
}

int M3UGenerator::generateAll(const QString &systemName, const QString &outputDir)
{
    QMap<QString, QList<int>> multiDiscGames = detectMultiDiscGames(systemName);

    if (multiDiscGames.isEmpty()) {
        qInfo() << "No multi-disc games found";
        return 0;
    }

    int generated = 0;

    for (auto it = multiDiscGames.constBegin(); it != multiDiscGames.constEnd(); ++it) {
        QString baseTitle = it.key();
        QList<int> fileIds = it.value();

        // Get file paths
        QStringList discPaths;
        for (int fileId : fileIds) {
            FileRecord file = m_database.getFileById(fileId);
            if (file.id > 0) {
                discPaths.append(file.currentPath);
            }
        }

        // Sort by disc number
        QList<FileRecord> fileInfos;
        for (int fileId : fileIds) {
            FileRecord file = m_database.getFileById(fileId);
            if (file.id > 0) {
                fileInfos.append(file);
            }
        }
        fileInfos = sortByDiscNumber(fileInfos);

        // Rebuild path list in sorted order
        discPaths.clear();
        for (const FileRecord &file : fileInfos) {
            discPaths.append(file.currentPath);
        }

        // Determine output path
        QString m3uPath;
        if (!outputDir.isEmpty()) {
            m3uPath = QDir(outputDir).filePath(baseTitle + ".m3u");
        } else {
            // Use same directory as first disc
            if (!discPaths.isEmpty()) {
                QFileInfo firstDisc(discPaths.first());
                m3uPath = firstDisc.absoluteDir().filePath(baseTitle + ".m3u");
            }
        }

        if (generateM3U(baseTitle, discPaths, m3uPath)) {
            generated++;
        }
    }

    qInfo() << "Generated" << generated << "M3U playlists";
    return generated;
}

bool M3UGenerator::isMultiDisc(const QString &filename)
{
    // Match patterns like "Disc 1", "Disc 01", "(Disc 1)", "CD1", etc.
    QRegularExpression re("\\b(Disc|CD|Disk)\\s*\\d+", QRegularExpression::CaseInsensitiveOption);
    return re.match(filename).hasMatch();
}

QString M3UGenerator::extractBaseTitle(const QString &filename)
{
    // Remove disc number patterns
    QString baseTitle = filename;

    // Remove extension
    QFileInfo info(baseTitle);
    baseTitle = info.completeBaseName();

    // Remove disc patterns
    QRegularExpression discPattern("\\s*\\(?\\s*(Disc|CD|Disk)\\s*\\d+.*?\\)?\\s*", 
                                  QRegularExpression::CaseInsensitiveOption);
    baseTitle.remove(discPattern);

    // Clean up extra spaces and parentheses
    baseTitle = baseTitle.trimmed();
    baseTitle.replace(QRegularExpression("\\s{2,}"), " ");
    baseTitle.replace(QRegularExpression("\\(\\s*\\)"), "");

    return baseTitle;
}

int M3UGenerator::extractDiscNumber(const QString &filename)
{
    QRegularExpression re("\\b(Disc|CD|Disk)\\s*(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = re.match(filename);

    if (match.hasMatch()) {
        return match.captured(2).toInt();
    }

    return 0;
}

QMap<QString, QList<FileRecord>> M3UGenerator::groupByBaseTitle(const QList<FileRecord> &files)
{
    QMap<QString, QList<FileRecord>> groups;

    for (const FileRecord &file : files) {
        if (isMultiDisc(file.currentPath)) {
            QString baseTitle = extractBaseTitle(file.currentPath);
            groups[baseTitle].append(file);
        }
    }

    return groups;
}

QList<FileRecord> M3UGenerator::sortByDiscNumber(const QList<FileRecord> &files)
{
    QList<FileRecord> sorted = files;

    // Sort by disc number
    std::sort(sorted.begin(), sorted.end(), [](const FileRecord &a, const FileRecord &b) {
        int discA = extractDiscNumber(a.currentPath);
        int discB = extractDiscNumber(b.currentPath);
        return discA < discB;
    });

    return sorted;
}

bool M3UGenerator::writeM3UFile(const QString &path, const QStringList &discPaths)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << path;
        return false;
    }

    QTextStream out(&file);

    // M3U format is simple: one file per line
    for (const QString &discPath : discPaths) {
        out << discPath << "\n";
    }

    file.close();
    return true;
}

} // namespace Remus
