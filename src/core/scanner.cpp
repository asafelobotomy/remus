#include "scanner.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QMap>
#include <QTextStream>
#include <QDebug>
#include "logging_categories.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug() qCDebug(logCore)
#define qInfo() qCInfo(logCore)
#define qWarning() qCWarning(logCore)
#define qCritical() qCCritical(logCore)

namespace Remus {

Scanner::Scanner(QObject *parent)
    : QObject(parent)
{
}

void Scanner::setExtensions(const QStringList &extensions)
{
    m_extensions = extensions;
}

QList<ScanResult> Scanner::scan(const QString &libraryPath)
{
    QList<ScanResult> results;
    m_filesProcessed = 0;
    m_cancelRequested = false;
    m_cancelled = false;

    QDir dir(libraryPath);
    if (!dir.exists()) {
        emit scanError(QString("Directory does not exist: %1").arg(libraryPath));
        return results;
    }

    // Log available archive tools
    if (m_archiveScanning) {
        auto tools = m_archiveExtractor.getAvailableTools();
        qInfo() << "Archive scanning enabled. Available tools:";
        qInfo() << "  ZIP:" << (tools[ArchiveFormat::ZIP] ? "yes" : "NO (install unzip or 7z)");
        qInfo() << "  7z:" << (tools[ArchiveFormat::SevenZip] ? "yes" : "NO (install p7zip/7z)");
        qInfo() << "  RAR:" << (tools[ArchiveFormat::RAR] ? "yes" : "NO (install unrar or 7z)");
    }

    emit scanStarted(libraryPath);
    scanDirectory(libraryPath, results);

    if (m_cancelRequested) {
        m_cancelled = true;
        return results;
    }

    // Post-processing: detect multi-file sets
    if (m_multiFileDetection) {
        detectMultiFileSets(results);
    }

    emit scanCompleted(results.size());
    return results;
}

void Scanner::scanDirectory(const QString &dirPath, QList<ScanResult> &results)
{
    QDirIterator it(dirPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        if (m_cancelRequested) {
            return;
        }

        QString path = it.next();
        QFileInfo fileInfo(path);
        
        // Skip files in directories marked with .remusdir
        if (isInExcludedDirectory(fileInfo.absolutePath())) {
            continue;
        }

        if (fileInfo.isFile()) {
            QString extension = "." + fileInfo.suffix().toLower();
            
            // Check if it's an archive and archive scanning is enabled
            if (m_archiveScanning && isArchiveExtension(extension)) {
                processArchive(path, results);
                m_filesProcessed++;
                emit fileFound(path);
            }
            // Check if it's a regular ROM file
            else if (isValidExtension(extension)) {
                ScanResult result = createScanResult(fileInfo);
                results.append(result);
                
                m_filesProcessed++;
                emit fileFound(path);
                
                if (m_filesProcessed % 100 == 0) {
                    emit scanProgress(m_filesProcessed, -1);
                }
            }
        }
    }
}

bool Scanner::isValidExtension(const QString &extension) const
{
    if (m_extensions.isEmpty()) {
        return true;  // No filter, accept all
    }
    return m_extensions.contains(extension, Qt::CaseInsensitive);
}

bool Scanner::isArchiveExtension(const QString &extension) const
{
    static const QStringList archiveExtensions = {
        ".zip", ".7z", ".rar", ".tar", ".tar.gz", ".tgz", ".tar.bz2", ".tbz2"
    };
    return archiveExtensions.contains(extension, Qt::CaseInsensitive);
}

bool Scanner::isInExcludedDirectory(const QString &dirPath) const
{
    // Check if this directory or any parent contains .remusdir marker
    QDir dir(dirPath);
    
    // Cache to avoid repeated filesystem checks
    static QSet<QString> excludedDirs;
    static QSet<QString> checkedDirs;
    
    // Quick check if we already know this path is excluded
    QString absPath = dir.absolutePath();
    if (excludedDirs.contains(absPath)) {
        return true;
    }
    if (checkedDirs.contains(absPath)) {
        return false;
    }
    
    // Walk up the directory tree looking for .remusdir
    QDir checkDir(absPath);
    while (!checkDir.isRoot()) {
        QString markerPath = checkDir.absolutePath() + "/.remusdir";
        if (QFile::exists(markerPath)) {
            excludedDirs.insert(absPath);  // Cache this path as excluded
            return true;
        }
        if (!checkDir.cdUp()) {
            break;
        }
    }
    
    checkedDirs.insert(absPath);  // Cache as not excluded
    return false;
}

void Scanner::processArchive(const QString &archivePath, QList<ScanResult> &results)
{
    ArchiveInfo archiveInfo = m_archiveExtractor.getArchiveInfo(archivePath);
    
    if (archiveInfo.format == ArchiveFormat::Unknown) {
        qWarning() << "Unknown archive format:" << archivePath;
        return;
    }
    
    // Check if we can extract this format
    if (!m_archiveExtractor.canExtract(archiveInfo.format)) {
        qWarning() << "Cannot extract archive (missing tool):" << archivePath 
                   << "- Format:" << static_cast<int>(archiveInfo.format);
        return;
    }
    
    // Warn if archive appears empty (tool may have failed)
    if (archiveInfo.contents.isEmpty()) {
        qWarning() << "Archive appears empty or tool failed:" << archivePath;
        return;
    }

    // Process each file in the archive
    for (const QString &internalPath : archiveInfo.contents) {
        QString extension = "." + QFileInfo(internalPath).suffix().toLower();
        
        // Skip if it's not a ROM file we care about
        if (!isValidExtension(extension)) {
            continue;
        }

        ScanResult result;
        result.path = archivePath;  // Archive path is the main file
        result.filename = QFileInfo(internalPath).fileName();
        result.extension = extension;
        result.fileSize = 0;  // We don't know individual file size in archive
        result.lastModified = QFileInfo(archivePath).lastModified();
        result.isCompressed = true;
        result.archivePath = archivePath;
        result.archiveInternalPath = internalPath;
        
        results.append(result);
        emit fileFound(archivePath + "::" + internalPath);
        
        if (m_filesProcessed % 100 == 0) {
            emit scanProgress(m_filesProcessed, -1);
        }
    }
}

ScanResult Scanner::createScanResult(const QFileInfo &fileInfo)
{
    ScanResult result;
    result.path = fileInfo.absoluteFilePath();
    result.filename = fileInfo.fileName();
    result.extension = "." + fileInfo.suffix().toLower();
    result.fileSize = fileInfo.size();
    result.lastModified = fileInfo.lastModified();
    return result;
}

void Scanner::detectMultiFileSets(QList<ScanResult> &results)
{
    linkBinToCue(results);
    linkGdiToTracks(results);
    linkCcdToImage(results);
    linkMdsToMdf(results);
}

void Scanner::linkBinToCue(QList<ScanResult> &results)
{
    // Build map of .cue files
    QMap<QString, int> cueFiles;  // baseName -> index in results
    
    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == ".cue") {
            QString baseName = QFileInfo(results[i].path).completeBaseName();
            cueFiles[baseName] = i;
        }
    }

    // Link .bin files to their .cue parents
    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == ".bin" || results[i].extension == ".img") {
            QString baseName = QFileInfo(results[i].path).completeBaseName();
            
            // Check if matching .cue exists
            if (cueFiles.contains(baseName)) {
                results[i].isPrimary = false;
                results[i].parentFilePath = results[cueFiles[baseName]].path;
                qDebug() << "Linked" << results[i].filename << "to" << results[cueFiles[baseName]].filename;
            }
        }
    }
}

static QStringList parseGdiTrackFiles(const QString &gdiPath)
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
            int start = line.indexOf('"') + 1;
            int end = line.indexOf('"', start);
            if (start > 0 && end > start) {
                filename = line.mid(start, end - start);
            }
        }

        if (filename.isEmpty()) {
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
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

void Scanner::linkGdiToTracks(QList<ScanResult> &results)
{
    QHash<QString, int> pathIndex;
    for (int i = 0; i < results.size(); ++i) {
        pathIndex[QFileInfo(results[i].path).absoluteFilePath()] = i;
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension != ".gdi") {
            continue;
        }

        QString gdiPath = results[i].path;
        QString baseDir = QFileInfo(gdiPath).absolutePath();
        QStringList trackFiles = parseGdiTrackFiles(gdiPath);

        for (const QString &trackFile : trackFiles) {
            QString trackPath = QDir(baseDir).filePath(trackFile);
            QString normalized = QFileInfo(trackPath).absoluteFilePath();

            if (pathIndex.contains(normalized)) {
                int index = pathIndex[normalized];
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
        if (results[i].extension == ".ccd") {
            QString key = QFileInfo(results[i].path).absolutePath() + "/" +
                          QFileInfo(results[i].path).completeBaseName();
            ccdFiles[key] = i;
        }
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == ".img" || results[i].extension == ".sub") {
            QString key = QFileInfo(results[i].path).absolutePath() + "/" +
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
        if (results[i].extension == ".mds") {
            QString key = QFileInfo(results[i].path).absolutePath() + "/" +
                          QFileInfo(results[i].path).completeBaseName();
            mdsFiles[key] = i;
        }
    }

    for (int i = 0; i < results.size(); ++i) {
        if (results[i].extension == ".mdf") {
            QString key = QFileInfo(results[i].path).absolutePath() + "/" +
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
