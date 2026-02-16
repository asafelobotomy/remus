#include "archive_extractor.h"
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>

namespace Remus {

ArchiveExtractor::ArchiveExtractor(QObject *parent)
    : QObject(parent)
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

ArchiveInfo ArchiveExtractor::getArchiveInfo(const QString &path)
{
    ArchiveInfo info;
    info.path = path;
    info.format = detectFormat(path);
    info.compressedSize = QFileInfo(path).size();
    
    QProcess process;
    QStringList args;
    
    switch (info.format) {
        case ArchiveFormat::ZIP:
            if (isToolAvailable(m_unzipPath)) {
                process.start(m_unzipPath, QStringList() << "-l" << path);
            } else if (isToolAvailable(m_sevenZipPath)) {
                process.start(m_sevenZipPath, QStringList() << "l" << path);
            }
            break;
            
        case ArchiveFormat::SevenZip:
            if (isToolAvailable(m_sevenZipPath)) {
                process.start(m_sevenZipPath, QStringList() << "l" << path);
            }
            break;
            
        case ArchiveFormat::RAR:
            if (isToolAvailable(m_unrarPath)) {
                process.start(m_unrarPath, QStringList() << "l" << path);
            } else if (isToolAvailable(m_sevenZipPath)) {
                process.start(m_sevenZipPath, QStringList() << "l" << path);
            }
            break;
            
        default:
            break;
    }
    
    process.waitForFinished(30000);
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    
    // Parse output for file list (format-specific parsing)
    QStringList lines = output.split('\n');
    
    // Skip headers and parse based on format
    if (info.format == ArchiveFormat::ZIP) {
        // unzip -l format:
        // Archive:  filename.zip
        //   Length      Date    Time    Name
        // ---------  ---------- -----   ----
        //     524288  1996-12-24 23:32   Sonic The Hedgehog (USA, Europe).md
        // ---------                     -------
        //     524288                     1 file
        bool inFileList = false;
        for (const QString &line : lines) {
            // Skip header lines and separators
            if (line.contains("Archive:") || line.contains("Name") || 
                line.contains("---------") || line.contains("files") ||
                line.trimmed().isEmpty()) {
                continue;
            }
            
            // Look for lines that start with a number (size)
            if (line[0].isDigit() || line[0] == ' ') {
                // Parse unzip output: Size Date Time Name
                // The filename starts after the time field
                // Format example: "   524288  1996-12-24 23:32   Sonic The Hedgehog (USA, Europe).md"
                QRegularExpression re("\\s+(\\d+)\\s+(\\d{4}-\\d{2}-\\d{2}\\s+\\d{2}:\\d{2})\\s+(.+)");
                QRegularExpressionMatch match = re.match(line);
                
                if (match.hasMatch()) {
                    QString filename = match.captured(3).trimmed();
                    if (!filename.isEmpty() && filename != "1 file") {
                        info.contents.append(filename);
                        info.fileCount++;
                    }
                }
            }
        }
    } else if (info.format == ArchiveFormat::SevenZip ||
               info.format == ArchiveFormat::GZip ||
               info.format == ArchiveFormat::TarGz ||
               info.format == ArchiveFormat::TarBz2) {
        // 7z l format varies, but generally:
        //    Date      Time    Attr         Size   Compressed  Name
        // ------------------- ----- ------------ ------------  ------------------------
        //                     .....    616494480    252257573  Silent Hill (USA).bin
        // OR:
        // 2026-02-05 18:40  .....       812000       400000  file.nes
        bool inFileList = false;
        for (const QString &line : lines) {
            // Skip header lines and separators
            if (line.contains("Date") || line.contains("Time") || 
                line.contains("---------") || line.contains("----------") ||
                line.contains("Type =") || line.contains("Path =") ||
                line.contains("files") || line.contains("folders") ||
                line.trimmed().isEmpty()) {
                continue;
            }
            
            // Look for attribute pattern (..... or D....) which indicates a file line
            // This handles both lines with and without dates
            if (line.contains(QRegularExpression("\\.[\\.\\.]+\\s+"))) {
                // Parse: [Date Time] Attr Size [Compressed] Name
                // The filename is the last field after the size numbers
                // Use regex to capture: optional date, attr, size(s), then name
                QRegularExpression re("(?:\\d{4}-\\d{2}-\\d{2}\\s+\\d{2}:\\d{2})?\\s*([.DA]+)\\s+(\\d+)\\s+(?:\\d+\\s+)?(.+)");
                QRegularExpressionMatch match = re.match(line.trimmed());
                
                if (match.hasMatch()) {
                    QString filename = match.captured(3).trimmed();
                    if (!filename.isEmpty()) {
                        info.contents.append(filename);
                        info.fileCount++;
                    }
                } else {
                    // Fallback: split by whitespace and take the last non-empty field
                    QStringList parts = line.trimmed().split(QRegularExpression("\\s+"));
                    if (parts.size() >= 3) {
                        QString filename = parts.last();
                        if (!filename.isEmpty() && !filename.contains(QRegularExpression("^\\d+$"))) {
                            info.contents.append(filename);
                            info.fileCount++;
                        }
                    }
                }
            }
        }
    } else if (info.format == ArchiveFormat::RAR) {
        // unrar l format:
        // RAR 5.10   Copyright (c) 1993-2020 Alexander Roshal
        // Name             Size   Packed Ratio  Date    Time   Attr CRC   
        // file.nes        812000  400000  49%  02-05-26 18:40  -rw- 12AB34CD
        for (const QString &line : lines) {
            // Skip header and empty lines
            if (line.contains("RAR") || line.contains("Name") ||
                line.contains("-----") || line.trimmed().isEmpty()) {
                continue;
            }
            
            // Extract filename (first field before whitespace)
            QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                // Look for pattern: filename + spaces + size
                QStringList parts = trimmed.split(QRegularExpression("\\s+"));
                if (parts.size() >= 2 && parts[1].toInt() > 0) {
                    QString filename = parts[0];
                    if (!filename.isEmpty()) {
                        info.contents.append(filename);
                        info.fileCount++;
                    }
                }
            }
        }
    }
    
    return info;
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
    
    QProcess process;
    QStringList args;
    
    switch (format) {
        case ArchiveFormat::ZIP:
            if (isToolAvailable(m_unzipPath)) {
                args << archivePath << fileName << "-d" << outputDir;
                process.start(m_unzipPath, args);
            }
            break;
            
        case ArchiveFormat::SevenZip:
            if (isToolAvailable(m_sevenZipPath)) {
                args << "e" << archivePath << "-o" + outputDir << fileName << "-y";
                process.start(m_sevenZipPath, args);
            }
            break;
            
        case ArchiveFormat::RAR:
            if (isToolAvailable(m_unrarPath)) {
                args << "e" << archivePath << fileName << outputDir;
                process.start(m_unrarPath, args);
            }
            break;
            
        default:
            result.error = "Unsupported format for single file extraction";
            return result;
    }
    
    process.waitForFinished(120000);
    result.success = (process.exitCode() == 0);
    
    if (result.success) {
        result.filesExtracted = 1;
        result.extractedFiles.append(QDir(outputDir).filePath(QFileInfo(fileName).fileName()));
    } else {
        result.error = QString::fromUtf8(process.readAllStandardError());
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

void ArchiveExtractor::cancel()
{
    m_cancelled = true;
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->terminate();
        m_process->waitForFinished(3000);
        if (m_process->state() == QProcess::Running) {
            m_process->kill();
        }
    }
}

bool ArchiveExtractor::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

ExtractionResult ArchiveExtractor::extractZip(const QString &archivePath, const QString &outputDir)
{
    ExtractionResult result;
    result.archivePath = archivePath;
    result.outputDir = outputDir;
    
    QProcess process;
    m_process = &process;
    
    QStringList args;
    
    // Prefer unzip, fall back to 7z
    if (isToolAvailable(m_unzipPath)) {
        args << "-o" << archivePath << "-d" << outputDir;
        qInfo() << "Extracting with unzip:" << archivePath;
        process.start(m_unzipPath, args);
    } else if (isToolAvailable(m_sevenZipPath)) {
        args << "x" << archivePath << "-o" + outputDir << "-y";
        qInfo() << "Extracting with 7z:" << archivePath;
        process.start(m_sevenZipPath, args);
    } else {
        result.error = "No ZIP extraction tool available (install unzip or 7z)";
        m_process = nullptr;
        return result;
    }
    
    process.waitForFinished(600000);  // 10 minutes for large archives
    
    result.success = (process.exitCode() == 0);
    
    if (result.success) {
        // Count and list extracted files
        QDir dir(outputDir);
        QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        result.filesExtracted = files.count();
        for (const QString &file : files) {
            result.extractedFiles.append(dir.absoluteFilePath(file));
        }
        qInfo() << "Extraction successful:" << result.filesExtracted << "items";
    } else {
        result.error = QString::fromUtf8(process.readAllStandardError());
        qWarning() << "Extraction failed:" << result.error;
    }
    
    m_process = nullptr;
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
    
    QProcess process;
    m_process = &process;
    
    QStringList args;
    args << "x" << archivePath << "-o" + outputDir << "-y";
    
    qInfo() << "Extracting with 7z:" << archivePath;
    process.start(m_sevenZipPath, args);
    process.waitForFinished(600000);
    
    result.success = (process.exitCode() == 0);
    
    if (result.success) {
        // Count and list extracted files
        QDir dir(outputDir);
        QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        result.filesExtracted = files.count();
        for (const QString &file : files) {
            result.extractedFiles.append(dir.absoluteFilePath(file));
        }
    } else {
        result.error = QString::fromUtf8(process.readAllStandardError());
    }
    
    m_process = nullptr;
    return result;
}

ExtractionResult ArchiveExtractor::extractRar(const QString &archivePath, const QString &outputDir)
{
    ExtractionResult result;
    result.archivePath = archivePath;
    result.outputDir = outputDir;
    
    QProcess process;
    m_process = &process;
    
    QStringList args;
    
    if (isToolAvailable(m_unrarPath)) {
        args << "x" << "-y" << archivePath << outputDir + "/";
        qInfo() << "Extracting with unrar:" << archivePath;
        process.start(m_unrarPath, args);
    } else if (isToolAvailable(m_sevenZipPath)) {
        args << "x" << archivePath << "-o" + outputDir << "-y";
        qInfo() << "Extracting with 7z:" << archivePath;
        process.start(m_sevenZipPath, args);
    } else {
        result.error = "No RAR extraction tool available (install unrar or 7z)";
        m_process = nullptr;
        return result;
    }
    
    process.waitForFinished(600000);
    
    result.success = (process.exitCode() == 0);
    
    if (result.success) {
        // Count and list extracted files
        QDir dir(outputDir);
        QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        result.filesExtracted = files.count();
        for (const QString &file : files) {
            result.extractedFiles.append(dir.absoluteFilePath(file));
        }
    } else {
        result.error = QString::fromUtf8(process.readAllStandardError());
    }
    
    m_process = nullptr;
    return result;
}

bool ArchiveExtractor::isToolAvailable(const QString &tool) const
{
    if (tool.isEmpty()) return false;
    
    QProcess process;
    process.start(tool, QStringList() << "--version");
    process.waitForFinished(3000);
    
    // Some tools return 0, some return 1 for --version
    return process.exitStatus() == QProcess::NormalExit;
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

} // namespace Remus
