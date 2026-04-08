#include "scanner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QTemporaryDir>
#include <QTextStream>

#include "constants/files.h"
#include "logging_categories.h"

#undef qDebug
#define qDebug() qCDebug(logCore)

namespace Remus {

namespace {

QStringList parseGdiTrackFiles(const QString &gdiPath)
{
    QStringList trackFiles;
    QFile file(gdiPath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return trackFiles;
    }

    QTextStream in(&file);
    bool firstLine = true;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }

        if (firstLine) {
            firstLine = false;
            continue;
        }

        QString filename;

        if (line.contains('"')) {
            const int start = line.indexOf('"') + 1;
            const int end = line.indexOf('"', start);
            if (start > 0 && end > start) {
                filename = line.mid(start, end - start);
            }
        }

        if (filename.isEmpty()) {
            const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            for (const QString &part : parts) {
                if (part.contains('.') && !part.at(0).isDigit()) {
                    filename = part;
                    break;
                }
            }

            if (filename.isEmpty() && parts.size() >= 5) {
                filename = parts[4];
            }
        }

        if (!filename.isEmpty()) {
            trackFiles.append(filename);
        }
    }

    return trackFiles;
}

QString normalizedMemberPath(const ScanResult &result)
{
    if (!result.isCompressed || result.archiveInternalPath.isEmpty()) {
        return QString();
    }

    return ArchiveExtractor::normalizeArchiveMemberPath(result.archiveInternalPath);
}

QString scanResultIdentifier(const ScanResult &result)
{
    const QString normalizedPath = normalizedMemberPath(result);
    if (!normalizedPath.isEmpty() && !result.archivePath.isEmpty()) {
        return result.archivePath + "::" + normalizedPath;
    }

    return QFileInfo(result.path).absoluteFilePath();
}

QString scanResultDirectoryKey(const ScanResult &result)
{
    const QString normalizedPath = normalizedMemberPath(result);
    if (!normalizedPath.isEmpty() && !result.archivePath.isEmpty()) {
        const QString internalDir = QFileInfo(normalizedPath).path();
        return result.archivePath + "::" + (internalDir == "." ? QString() : internalDir);
    }

    return QFileInfo(result.path).absolutePath();
}

QString scanResultBaseName(const ScanResult &result)
{
    const QString normalizedPath = normalizedMemberPath(result);
    return QFileInfo(normalizedPath.isEmpty() ? result.path : normalizedPath).completeBaseName();
}

QString multiFileKey(const ScanResult &result)
{
    return scanResultDirectoryKey(result) + "/" + scanResultBaseName(result);
}

QString siblingIdentifier(const ScanResult &result, const QString &fileName)
{
    const QString normalizedPath = normalizedMemberPath(result);
    if (!normalizedPath.isEmpty() && !result.archivePath.isEmpty()) {
        const QString baseDir = QFileInfo(normalizedPath).path();
        const QString siblingPath = ArchiveExtractor::normalizeArchiveMemberPath(
            QDir(baseDir).filePath(fileName));
        if (siblingPath.isEmpty()) {
            return QString();
        }
        return result.archivePath + "::" + siblingPath;
    }

    return QFileInfo(QDir(QFileInfo(result.path).absolutePath()).filePath(fileName)).absoluteFilePath();
}

QStringList gdiTrackFilesForResult(const ScanResult &result)
{
    if (!result.isCompressed || result.archivePath.isEmpty() || result.archiveInternalPath.isEmpty()) {
        return parseGdiTrackFiles(result.path);
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return {};
    }

    ArchiveExtractor extractor;
    ExtractionResult extraction = extractor.extractFile(
        result.archivePath,
        result.archiveInternalPath,
        tempDir.path());
    if (!extraction.success || extraction.extractedFiles.isEmpty()) {
        return {};
    }

    return parseGdiTrackFiles(extraction.extractedFiles.first());
}

} // namespace

void Scanner::detectMultiFileSets(QList<ScanResult> &results)
{
    linkBinToCue(results);
    linkGdiToTracks(results);
    linkCcdToImage(results);
    linkMdsToMdf(results);
}

void Scanner::linkBinToCue(QList<ScanResult> &results)
{
    // For disc images, the data track (.bin/.img) is the file that DATs hash.
    // Mark the sheet file (.cue) as non-primary so the data track gets hashed
    // and matched against CRC/MD5/SHA1 values in the database.
    QMap<QString, int> binFiles;
    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::BIN || results[i].extension == Constants::Files::IMG) {
            binFiles[multiFileKey(results[i])] = i;
        }
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::CUE) {
            const QString key = multiFileKey(results[i]);
            if (binFiles.contains(key)) {
                results[i].isPrimary = false;
                results[i].parentFilePath = scanResultIdentifier(results[binFiles[key]]);
                qDebug() << "Linked" << results[i].filename << "to" << results[binFiles[key]].filename;
            }
        }
    }
}

void Scanner::linkGdiToTracks(QList<ScanResult> &results)
{
    QHash<QString, int> pathIndex;
    for (int i = 0; i < results.size(); ++i) {
        pathIndex[scanResultIdentifier(results[i])] = i;
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension != Constants::Files::GDI) {
            continue;
        }

        const QStringList trackFiles = gdiTrackFilesForResult(results[i]);

        for (const QString &trackFile : trackFiles) {
            const QString identifier = siblingIdentifier(results[i], trackFile);
            if (pathIndex.contains(identifier)) {
                const int index = pathIndex[identifier];
                results[index].isPrimary = false;
                results[index].parentFilePath = scanResultIdentifier(results[i]);
                qDebug() << "Linked" << results[index].filename << "to" << results[i].filename;
            }
        }
    }
}

void Scanner::linkCcdToImage(QList<ScanResult> &results)
{
    // Data files (.img) should be primary; sheet files (.ccd, .sub) are non-primary.
    QMap<QString, int> imgFiles;
    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::IMG) {
            imgFiles[multiFileKey(results[i])] = i;
        }
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::CCD || results[i].extension == Constants::Files::SUB) {
            const QString key = multiFileKey(results[i]);
            if (imgFiles.contains(key)) {
                results[i].isPrimary = false;
                results[i].parentFilePath = scanResultIdentifier(results[imgFiles[key]]);
                qDebug() << "Linked" << results[i].filename << "to" << results[imgFiles[key]].filename;
            }
        }
    }
}

void Scanner::linkMdsToMdf(QList<ScanResult> &results)
{
    // Data files (.mdf) should be primary; sheet files (.mds) are non-primary.
    QMap<QString, int> mdfFiles;
    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::MDF) {
            mdfFiles[multiFileKey(results[i])] = i;
        }
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::MDS) {
            const QString key = multiFileKey(results[i]);
            if (mdfFiles.contains(key)) {
                results[i].isPrimary = false;
                results[i].parentFilePath = scanResultIdentifier(results[mdfFiles[key]]);
                qDebug() << "Linked" << results[i].filename << "to" << results[mdfFiles[key]].filename;
            }
        }
    }
}

} // namespace Remus