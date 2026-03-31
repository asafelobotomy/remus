#include "scanner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
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
            binFiles[QFileInfo(results[i].path).completeBaseName()] = i;
        }
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::CUE) {
            const QString baseName = QFileInfo(results[i].path).completeBaseName();
            if (binFiles.contains(baseName)) {
                results[i].isPrimary = false;
                results[i].parentFilePath = results[binFiles[baseName]].path;
                qDebug() << "Linked" << results[i].filename << "to" << results[binFiles[baseName]].filename;
            }
        }
    }
}

void Scanner::linkGdiToTracks(QList<ScanResult> &results)
{
    QHash<QString, int> pathIndex;
    for (int i = 0; i < results.size(); ++i) {
        pathIndex[QFileInfo(results[i].path).absoluteFilePath()] = i;
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension != Constants::Files::GDI) {
            continue;
        }

        const QString gdiPath = results[i].path;
        const QString baseDir = QFileInfo(gdiPath).absolutePath();
        const QStringList trackFiles = parseGdiTrackFiles(gdiPath);

        for (const QString &trackFile : trackFiles) {
            const QString normalized = QFileInfo(QDir(baseDir).filePath(trackFile)).absoluteFilePath();
            if (pathIndex.contains(normalized)) {
                const int index = pathIndex[normalized];
                results[index].isPrimary = false;
                results[index].parentFilePath = gdiPath;
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
            const QString key = QFileInfo(results[i].path).absolutePath() + "/" +
                                QFileInfo(results[i].path).completeBaseName();
            imgFiles[key] = i;
        }
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::CCD || results[i].extension == Constants::Files::SUB) {
            const QString key = QFileInfo(results[i].path).absolutePath() + "/" +
                                QFileInfo(results[i].path).completeBaseName();
            if (imgFiles.contains(key)) {
                results[i].isPrimary = false;
                results[i].parentFilePath = results[imgFiles[key]].path;
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
            const QString key = QFileInfo(results[i].path).absolutePath() + "/" +
                                QFileInfo(results[i].path).completeBaseName();
            mdfFiles[key] = i;
        }
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::MDS) {
            const QString key = QFileInfo(results[i].path).absolutePath() + "/" +
                                QFileInfo(results[i].path).completeBaseName();
            if (mdfFiles.contains(key)) {
                results[i].isPrimary = false;
                results[i].parentFilePath = results[mdfFiles[key]].path;
                qDebug() << "Linked" << results[i].filename << "to" << results[mdfFiles[key]].filename;
            }
        }
    }
}

} // namespace Remus