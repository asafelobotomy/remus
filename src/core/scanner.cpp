#include "scanner.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QMap>
#include <QTextStream>
#include <QDebug>
#include <QSet>
#include "logging_categories.h"
#include "constants/files.h"
#include "constants/settings.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug() qCDebug(logCore)
#define qInfo() qCInfo(logCore)
#define qWarning() qCWarning(logCore)
#define qCritical() qCCritical(logCore)

namespace Remus {

namespace {

const QSet<QString> kMarkdownDocumentNames = {
    "readme.md",
    "changelog.md",
    "contributing.md",
    "copying.md",
    "license.md",
    "todo.md"
};

}

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
    m_excludedDirs.clear();
    m_checkedDirs.clear();

    const QFileInfo rootInfo(libraryPath);
    if (!rootInfo.exists()) {
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

    if (rootInfo.isFile()) {
        const QString absolutePath = rootInfo.absoluteFilePath();
        const QString extension = "." + rootInfo.suffix().toLower();

        if (m_archiveScanning && isArchiveExtension(extension)) {
            processArchive(absolutePath, results);
            m_filesProcessed++;
            emit fileFound(absolutePath);
        } else if (shouldScanFile(rootInfo)) {
            results.append(createScanResult(rootInfo));
            m_filesProcessed++;
            emit fileFound(absolutePath);
        }

        if (m_cancelRequested) {
            m_cancelled = true;
            return results;
        }

        if (m_multiFileDetection) {
            detectMultiFileSets(results);
        }

        emit scanCompleted(results.size());
        return results;
    }

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
            else if (shouldScanFile(fileInfo)) {
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

bool Scanner::shouldScanFile(const QFileInfo &fileInfo) const
{
    const QString extension = "." + fileInfo.suffix().toLower();
    if (!isValidExtension(extension)) {
        return false;
    }

    return !isLikelyMarkdownDocument(fileInfo.absoluteFilePath());
}

bool Scanner::shouldScanArchiveEntry(const QString &internalPath) const
{
    const QString extension = "." + QFileInfo(internalPath).suffix().toLower();
    if (!isValidExtension(extension)) {
        return false;
    }

    return !isLikelyMarkdownDocument(internalPath);
}

bool Scanner::isArchiveExtension(const QString &extension) const
{
    return Constants::Files::isArchiveExtension(extension);
}

bool Scanner::isInExcludedDirectory(const QString &dirPath) const
{
    // Check if this directory or any parent contains .remusdir marker
    QDir dir(dirPath);

    // Quick check if we already know this path is excluded
    QString absPath = dir.absolutePath();
    if (m_excludedDirs.contains(absPath)) {
        return true;
    }
    if (m_checkedDirs.contains(absPath)) {
        return false;
    }
    
    // Walk up the directory tree looking for .remusdir
    QDir checkDir(absPath);
    while (!checkDir.isRoot()) {
        QString markerPath = checkDir.absolutePath() + "/" + Constants::Settings::Files::MARKER_SKIP_SCAN;
        if (QFile::exists(markerPath)) {
            m_excludedDirs.insert(absPath);  // Cache this path as excluded
            return true;
        }
        if (!checkDir.cdUp()) {
            break;
        }
    }

    m_checkedDirs.insert(absPath);  // Cache as not excluded
    return false;
}

bool Scanner::isLikelyMarkdownDocument(const QString &path) const
{
    const QFileInfo fileInfo(path);
    if (fileInfo.suffix().compare("md", Qt::CaseInsensitive) != 0) {
        return false;
    }

    const QString normalizedPath = QDir::fromNativeSeparators(path).toLower();
    if (kMarkdownDocumentNames.contains(fileInfo.fileName().toLower())) {
        return true;
    }

    if (normalizedPath.contains("/docs/") || normalizedPath.contains("/documentation/")) {
        return true;
    }

    if (fileInfo.exists()) {
        return isLikelyTextFile(fileInfo.absoluteFilePath());
    }

    return false;
}

bool Scanner::isLikelyTextFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray sample = file.read(1024);
    if (sample.isEmpty()) {
        return true;
    }

    if (sample.contains('\0')) {
        return false;
    }

    int printableBytes = 0;
    for (char byte : sample) {
        const unsigned char ch = static_cast<unsigned char>(byte);
        if (ch == '\n' || ch == '\r' || ch == '\t' || (ch >= 32 && ch <= 126)) {
            printableBytes++;
        }
    }

    return (static_cast<double>(printableBytes) / sample.size()) >= 0.9;
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
        const QString normalizedInternalPath = ArchiveExtractor::normalizeArchiveMemberPath(internalPath);
        if (normalizedInternalPath.isEmpty()) {
            qWarning() << "Skipping unsafe archive entry:" << internalPath;
            continue;
        }

        const QString extension = "." + QFileInfo(normalizedInternalPath).suffix().toLower();

        // Skip if it's not a ROM file we care about.
        if (!shouldScanArchiveEntry(normalizedInternalPath)) {
            continue;
        }

        ScanResult result;
        result.path = archivePath;  // Archive path is the main file
        result.filename = QFileInfo(normalizedInternalPath).fileName();
        result.extension = extension;
        result.fileSize = archiveInfo.entrySizes.value(normalizedInternalPath, 0);
        result.lastModified = QFileInfo(archivePath).lastModified();
        result.isCompressed = true;
        result.archivePath = archivePath;
        result.archiveInternalPath = normalizedInternalPath;
        
        results.append(result);
        emit fileFound(archivePath + "::" + normalizedInternalPath);
        
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

} // namespace Remus
