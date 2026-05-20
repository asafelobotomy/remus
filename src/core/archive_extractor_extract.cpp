#include "archive_extractor.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace Remus {

namespace {

QString relativeExtractedPath(const QString &outputDir, const QString &path)
{
    return QDir(outputDir).relativeFilePath(path).replace('\\', '/');
}

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
    const ArchiveInfo preflightInfo = getArchiveInfo(archivePath);
    if (const QString validationError = validateArchiveEntries(preflightInfo); !validationError.isEmpty()) {
        result.success = false;
        result.error = validationError;
        return result;
    }

    QDir().mkpath(targetDir);
    emit extractionStarted(archivePath, targetDir);

    switch (detectFormat(archivePath)) {
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
        break;
    }

    if (result.success) {
        if (const QString validationError = validateExtractedFiles(result.outputDir, result.extractedFiles);
            !validationError.isEmpty()) {
            result.success = false;
            result.error = validationError;
        }
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

    const QString normalizedFileName = normalizeArchiveMemberPath(fileName);
    if (normalizedFileName.isEmpty()) {
        result.error = "Unsafe archive member path";
        return result;
    }

    QStringList args;
    ProcessResult processResult;

    switch (detectFormat(archivePath)) {
    case ArchiveFormat::ZIP:
        if (isToolAvailable(m_unzipPath)) {
            args << archivePath << normalizedFileName << "-d" << outputDir;
            processResult = runProcess(m_unzipPath, args, 120000);
        }
        break;
    case ArchiveFormat::SevenZip:
        if (isToolAvailable(m_sevenZipPath)) {
            args << "e" << archivePath << "-o" + outputDir << normalizedFileName << "-y";
            processResult = runProcess(m_sevenZipPath, args, 120000);
        }
        break;
    case ArchiveFormat::RAR:
        if (isToolAvailable(m_unrarPath)) {
            args << "e" << archivePath << normalizedFileName << outputDir;
            processResult = runProcess(m_unrarPath, args, 120000);
        }
        break;
    default:
        result.error = "Unsupported format for single file extraction";
        return result;
    }

    result.success = (processResult.exitCode == 0 && processResult.started);
    if (result.success) {
        const QString exactPath = QDir(outputDir).filePath(normalizedFileName);
        const QString basenamePath = QDir(outputDir).filePath(QFileInfo(normalizedFileName).fileName());
        QString resolvedPath;

        if (QFileInfo::exists(exactPath)) {
            resolvedPath = exactPath;
        } else if (QFileInfo::exists(basenamePath)) {
            resolvedPath = basenamePath;
        } else {
            const QStringList extractedPaths = listFiles(outputDir);
            for (const QString &path : extractedPaths) {
                if (relativeExtractedPath(outputDir, path) == normalizedFileName) {
                    resolvedPath = path;
                    break;
                }
            }
        }

        if (!resolvedPath.isEmpty()) {
            result.filesExtracted = 1;
            result.extractedFiles.append(resolvedPath);
            if (!isPathWithinDirectory(outputDir, resolvedPath)) {
                result.success = false;
                result.error = QStringLiteral("Extraction escaped output directory: ") + resolvedPath;
            }
        } else {
            result.success = false;
            result.error = QStringLiteral("Extracted file not found for archive member: ") + normalizedFileName;
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

    const int total = archivePaths.size();
    int completed = 0;
    for (const QString &archivePath : archivePaths) {
        if (m_cancelled) {
            break;
        }

        results.append(extract(archivePath, outputDir, createSubfolders));
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
        if (const QString validationError = validateExtractedFiles(outputDir, result.extractedFiles);
            !validationError.isEmpty()) {
            result.success = false;
            result.error = validationError;
        }
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

    const QStringList args = {QStringLiteral("x"), archivePath, QStringLiteral("-o") + outputDir, QStringLiteral("-y")};
    qInfo() << "Extracting with 7z:" << archivePath;
    const ProcessResult processResult = runProcessTracked(m_sevenZipPath, args, 600000);
    result.success = (processResult.exitCode == 0 && processResult.started);

    if (result.success) {
        result.extractedFiles = listFiles(outputDir);
        result.filesExtracted = result.extractedFiles.count();
        if (const QString validationError = validateExtractedFiles(outputDir, result.extractedFiles);
            !validationError.isEmpty()) {
            result.success = false;
            result.error = validationError;
        }
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
        if (const QString validationError = validateExtractedFiles(outputDir, result.extractedFiles);
            !validationError.isEmpty()) {
            result.success = false;
            result.error = validationError;
        }
    } else {
        result.error = processResult.stdError;
    }

    return result;
}

QByteArray ArchiveExtractor::readMemberPrefix(const QString &archivePath,
                                               const QString &memberPath,
                                               qint64 maxBytes)
{
    if (maxBytes <= 0) return {};

    const QString normalizedMember = normalizeArchiveMemberPath(memberPath);
    if (normalizedMember.isEmpty()) return {};

    QString program;
    QStringList args;

    switch (detectFormat(archivePath)) {
    case ArchiveFormat::ZIP:
        if (!m_unzipPath.isEmpty()) {
            program = m_unzipPath;
            args = {"-p", archivePath, normalizedMember};
        } else if (!m_sevenZipPath.isEmpty()) {
            program = m_sevenZipPath;
            args = {"e", archivePath, normalizedMember, "-so", "-y"};
        }
        break;
    case ArchiveFormat::SevenZip:
    case ArchiveFormat::GZip:
    case ArchiveFormat::TarGz:
    case ArchiveFormat::TarBz2:
        if (!m_sevenZipPath.isEmpty()) {
            program = m_sevenZipPath;
            args = {"e", archivePath, normalizedMember, "-so", "-y"};
        }
        break;
    case ArchiveFormat::RAR:
        if (!m_unrarPath.isEmpty()) {
            program = m_unrarPath;
            args = {"p", archivePath, normalizedMember};
        } else if (!m_sevenZipPath.isEmpty()) {
            program = m_sevenZipPath;
            args = {"e", archivePath, normalizedMember, "-so", "-y"};
        }
        break;
    default:
        return {};
    }

    if (program.isEmpty()) return {};

    QProcess proc;
    // Keep stdout and stderr separate; we only read stdout.
    proc.setReadChannel(QProcess::StandardOutput);
    proc.start(program, args);
    if (!proc.waitForStarted(5000)) return {};

    QByteArray data;
    data.reserve(static_cast<int>(maxBytes));

    while (data.size() < maxBytes) {
        if (proc.bytesAvailable() > 0) {
            data.append(proc.read(maxBytes - data.size()));
            continue;
        }
        if (proc.state() == QProcess::NotRunning) {
            // Process finished cleanly — drain any remaining bytes.
            const QByteArray tail = proc.readAll();
            if (!tail.isEmpty())
                data.append(tail.left(static_cast<int>(maxBytes - data.size())));
            break;
        }
        if (!proc.waitForReadyRead(5000)) break;
    }

    if (proc.state() != QProcess::NotRunning) {
        proc.kill();
        proc.waitForFinished(3000);
    }

    return data;
}

} // namespace Remus