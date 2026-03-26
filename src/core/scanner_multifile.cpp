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
    QMap<QString, int> cueFiles;
    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::CUE) {
            cueFiles[QFileInfo(results[i].path).completeBaseName()] = i;
        }
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::BIN || results[i].extension == Constants::Files::IMG) {
            const QString baseName = QFileInfo(results[i].path).completeBaseName();
            if (cueFiles.contains(baseName)) {
                results[i].isPrimary = false;
                results[i].parentFilePath = results[cueFiles[baseName]].path;
                qDebug() << "Linked" << results[i].filename << "to" << results[cueFiles[baseName]].filename;
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
    QMap<QString, int> ccdFiles;
    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::CCD) {
            const QString key = QFileInfo(results[i].path).absolutePath() + "/" +
                                QFileInfo(results[i].path).completeBaseName();
            ccdFiles[key] = i;
        }
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::IMG || results[i].extension == Constants::Files::SUB) {
            const QString key = QFileInfo(results[i].path).absolutePath() + "/" +
                                QFileInfo(results[i].path).completeBaseName();
            if (ccdFiles.contains(key)) {
                results[i].isPrimary = false;
                results[i].parentFilePath = results[ccdFiles[key]].path;
                qDebug() << "Linked" << results[i].filename << "to" << results[ccdFiles[key]].filename;
            }
        }
    }
}

void Scanner::linkMdsToMdf(QList<ScanResult> &results)
{
    QMap<QString, int> mdsFiles;
    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::MDS) {
            const QString key = QFileInfo(results[i].path).absolutePath() + "/" +
                                QFileInfo(results[i].path).completeBaseName();
            mdsFiles[key] = i;
        }
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == Constants::Files::MDF) {
            const QString key = QFileInfo(results[i].path).absolutePath() + "/" +
                                QFileInfo(results[i].path).completeBaseName();
            if (mdsFiles.contains(key)) {
                results[i].isPrimary = false;
                results[i].parentFilePath = results[mdsFiles[key]].path;
                qDebug() << "Linked" << results[i].filename << "to" << results[mdsFiles[key]].filename;
            }
        }
    }
}

} // namespace Remus