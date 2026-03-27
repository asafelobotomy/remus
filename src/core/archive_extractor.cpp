#include "archive_extractor.h"
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QRegularExpression>

namespace Remus {

ArchiveExtractor::ArchiveExtractor(QObject *parent)
    : ExternalToolRunner(parent)
{
    // Find default tool paths
    m_unzipPath = findTool({"unzip"});
    m_sevenZipPath = findTool({"7z", "7za", "7zz"});
    m_unrarPath = findTool({"unrar", "rar"});
}

QMap<ArchiveFormat, bool> ArchiveExtractor::getAvailableTools() const
{
    QMap<ArchiveFormat, bool> available;
    available[ArchiveFormat::ZIP] = isToolAvailable(m_unzipPath) || isToolAvailable(m_sevenZipPath);
    available[ArchiveFormat::SevenZip] = isToolAvailable(m_sevenZipPath);
    available[ArchiveFormat::RAR] = isToolAvailable(m_unrarPath) || isToolAvailable(m_sevenZipPath);
    available[ArchiveFormat::GZip] = isToolAvailable("gunzip") || isToolAvailable(m_sevenZipPath);
    return available;
}

bool ArchiveExtractor::canExtract(ArchiveFormat format) const
{
    return getAvailableTools().value(format, false);
}

bool ArchiveExtractor::canExtract(const QString &path) const
{
    return canExtract(detectFormat(path));
}

void ArchiveExtractor::setUnzipPath(const QString &path)
{
    m_unzipPath = path;
}

void ArchiveExtractor::setSevenZipPath(const QString &path)
{
    m_sevenZipPath = path;
}

void ArchiveExtractor::setUnrarPath(const QString &path)
{
    m_unrarPath = path;
}

ArchiveFormat ArchiveExtractor::detectFormat(const QString &path)
{
    QString ext = QFileInfo(path).suffix().toLower();
    
    if (ext == "zip") return ArchiveFormat::ZIP;
    if (ext == "7z") return ArchiveFormat::SevenZip;
    if (ext == "rar") return ArchiveFormat::RAR;
    if (ext == "gz") return ArchiveFormat::GZip;
    if (ext == "tgz") return ArchiveFormat::TarGz;
    if (ext == "bz2") return ArchiveFormat::TarBz2;
    
    // Check for .tar.gz, .tar.bz2
    QString baseName = QFileInfo(path).completeBaseName();
    if (baseName.endsWith(".tar")) {
        if (ext == "gz") return ArchiveFormat::TarGz;
        if (ext == "bz2") return ArchiveFormat::TarBz2;
    }
    
    return ArchiveFormat::Unknown;
}

ExtractionResult ArchiveExtractor::extract(const QString &archivePath,
                                            const QString &outputDir,
                                            bool createSubfolder)
{
    ExtractionResult result;
    result.archivePath = archivePath;
    
    QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists()) {
        result.success = false;
        result.error = "Archive file not found";
        return result;
    }
    
    QString targetDir = outputDir;
    if (targetDir.isEmpty()) {
        targetDir = archiveInfo.absolutePath();
    }
    
    if (createSubfolder) {
        targetDir = QDir(targetDir).filePath(archiveInfo.completeBaseName());
    }
    
    result.outputDir = targetDir;
    
    // Create output directory if needed
    QDir().mkpath(targetDir);
    
    emit extractionStarted(archivePath, targetDir);
    
    ArchiveFormat format = detectFormat(archivePath);
    
    switch (format) {
        case ArchiveFormat::ZIP:
            result = extractZip(archivePath, targetDir);
            break;
        case ArchiveFormat::SevenZip:
        case ArchiveFormat::GZip:
        case ArchiveFormat::TarGz:
        case ArchiveFormat::TarBz2:
            result = extract7z(archivePath, targetDir);
            break;
        case ArchiveFormat::RAR:
            result = extractRar(archivePath, targetDir);
            break;
        default:
            result.success = false;
            result.error = "Unsupported archive format";
    }
    
    emit extractionCompleted(result);
    return result;
}

ExtractionResult ArchiveExtractor::extractFile(const QString &archivePath,
                                                const QString &fileName,
                                                const QString &outputDir)
{
    ExtractionResult result;
    result.archivePath = archivePath;
    result.outputDir = outputDir;
    
    ArchiveFormat format = detectFormat(archivePath);
    
    QStringList args;
    ProcessResult processResult;
    
    switch (format) {
        case ArchiveFormat::ZIP:
            if (isToolAvailable(m_unzipPath)) {
                args << archivePath << fileName << "-d" << outputDir;
                processResult = runProcess(m_unzipPath, args, 120000);
            }
            break;
            
        case ArchiveFormat::SevenZip:
            if (isToolAvailable(m_sevenZipPath)) {
                args << "e" << archivePath << "-o" + outputDir << fileName << "-y";
                processResult = runProcess(m_sevenZipPath, args, 120000);
            }
            break;
            
        case ArchiveFormat::RAR:
            if (isToolAvailable(m_unrarPath)) {
                args << "e" << archivePath << fileName << outputDir;
                processResult = runProcess(m_unrarPath, args, 120000);
            }
            break;
            
        default:
            result.error = "Unsupported format for single file extraction";
            return result;
    }
    
    result.success = (processResult.exitCode == 0 && processResult.started);
    
    if (result.success) {
        const QString expectedPath = QDir(outputDir).filePath(QFileInfo(fileName).fileName());
        if (QFileInfo::exists(expectedPath)) {
            result.filesExtracted = 1;
            result.extractedFiles.append(expectedPath);
        } else {
            // 7z may exit 0 but extract nothing when the filename doesn't match;
            // mark as failed so the caller can fall back to full extraction.
            result.success = false;
            result.error = QStringLiteral("Extracted file not found at expected path: ") + expectedPath;
        }
    } else {
        result.error = processResult.stdError;
    }
    
    return result;
}

QList<ExtractionResult> ArchiveExtractor::batchExtract(const QStringList &archivePaths,
                                                        const QString &outputDir,
                                                        bool createSubfolders)
{
    QList<ExtractionResult> results;
    m_cancelled = false;
    
    int total = archivePaths.size();
    int completed = 0;
    
    for (const QString &archivePath : archivePaths) {
        if (m_cancelled) {
            break;
        }
        
        ExtractionResult result = extract(archivePath, outputDir, createSubfolders);
        results.append(result);
        
        completed++;
        emit batchProgress(completed, total);
    }
    
    return results;
}

ExtractionResult ArchiveExtractor::extractZip(const QString &archivePath, const QString &outputDir)
{
    ExtractionResult result;
    result.archivePath = archivePath;
    result.outputDir = outputDir;
    
    QStringList args;
    ProcessResult processResult;
    
    // Prefer unzip, fall back to 7z
    if (isToolAvailable(m_unzipPath)) {
        args << "-o" << archivePath << "-d" << outputDir;
        qInfo() << "Extracting with unzip:" << archivePath;
        processResult = runProcessTracked(m_unzipPath, args, 600000);
    } else if (isToolAvailable(m_sevenZipPath)) {
        args << "x" << archivePath << "-o" + outputDir << "-y";
        qInfo() << "Extracting with 7z:" << archivePath;
        processResult = runProcessTracked(m_sevenZipPath, args, 600000);
    } else {
        result.error = "No ZIP extraction tool available (install unzip or 7z)";
        return result;
    }

    result.success = (processResult.exitCode == 0 && processResult.started);
    
    if (result.success) {
        result.extractedFiles = listFiles(outputDir);
        result.filesExtracted = result.extractedFiles.count();
        qInfo() << "Extraction successful:" << result.filesExtracted << "items";
    } else {
        result.error = processResult.stdError;
        qWarning() << "Extraction failed:" << result.error;
    }
    return result;
}

ExtractionResult ArchiveExtractor::extract7z(const QString &archivePath, const QString &outputDir)
{
    ExtractionResult result;
    result.archivePath = archivePath;
    result.outputDir = outputDir;
    
    if (!isToolAvailable(m_sevenZipPath)) {
        result.error = "7z not available (install p7zip)";
        return result;
    }
    
    QStringList args;
    args << "x" << archivePath << "-o" + outputDir << "-y";
    
    qInfo() << "Extracting with 7z:" << archivePath;
    ProcessResult processResult = runProcessTracked(m_sevenZipPath, args, 600000);
    result.success = (processResult.exitCode == 0 && processResult.started);
    
    if (result.success) {
        result.extractedFiles = listFiles(outputDir);
        result.filesExtracted = result.extractedFiles.count();
    } else {
        result.error = processResult.stdError;
    }
    return result;
}

ExtractionResult ArchiveExtractor::extractRar(const QString &archivePath, const QString &outputDir)
{
    ExtractionResult result;
    result.archivePath = archivePath;
    result.outputDir = outputDir;
    
    QStringList args;
    ProcessResult processResult;
    
    if (isToolAvailable(m_unrarPath)) {
        args << "x" << "-y" << archivePath << outputDir + "/";
        qInfo() << "Extracting with unrar:" << archivePath;
        processResult = runProcessTracked(m_unrarPath, args, 600000);
    } else if (isToolAvailable(m_sevenZipPath)) {
        args << "x" << archivePath << "-o" + outputDir << "-y";
        qInfo() << "Extracting with 7z:" << archivePath;
        processResult = runProcessTracked(m_sevenZipPath, args, 600000);
    } else {
        result.error = "No RAR extraction tool available (install unrar or 7z)";
        return result;
    }

    result.success = (processResult.exitCode == 0 && processResult.started);
    
    if (result.success) {
        result.extractedFiles = listFiles(outputDir);
        result.filesExtracted = result.extractedFiles.count();
    } else {
        result.error = processResult.stdError;
    }
    return result;
}

bool ArchiveExtractor::isToolAvailable(const QString &tool) const
{
    if (tool.isEmpty()) return false;
    
    ProcessResult result = const_cast<ArchiveExtractor*>(this)
                               ->runProcess(tool, QStringList() << "--version", 3000);
    return result.exitStatus == QProcess::NormalExit;
}

QString ArchiveExtractor::findTool(const QStringList &candidates) const
{
    for (const QString &tool : candidates) {
        if (isToolAvailable(tool)) {
            return tool;
        }
    }
    return QString();
}

QStringList ArchiveExtractor::listFiles(const QString &dirPath) const
{
    QStringList files;
    QDirIterator it(dirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        files.append(it.filePath());
    }
    return files;
}

} // namespace Remus
