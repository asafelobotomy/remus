#include "archive_creator.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace Remus {

// ════════════════════════════════════════════════════════════
// Construction
// ════════════════════════════════════════════════════════════

ArchiveCreator::ArchiveCreator(QObject *parent)
    : QObject(parent)
{
    // Auto-detect tool paths
    m_zipPath = findTool({QStringLiteral("zip")});
    m_sevenZipPath = findTool({QStringLiteral("7z"), QStringLiteral("7za"), QStringLiteral("7zz")});
}

// ════════════════════════════════════════════════════════════
// Tool Detection
// ════════════════════════════════════════════════════════════

QMap<ArchiveFormat, bool> ArchiveCreator::getAvailableTools() const
{
    QMap<ArchiveFormat, bool> tools;
    tools[ArchiveFormat::ZIP] = isToolAvailable(m_zipPath);
    tools[ArchiveFormat::SevenZip] = isToolAvailable(m_sevenZipPath);
    return tools;
}

bool ArchiveCreator::canCompress(ArchiveFormat format) const
{
    switch (format) {
    case ArchiveFormat::ZIP:
        return isToolAvailable(m_zipPath);
    case ArchiveFormat::SevenZip:
        return isToolAvailable(m_sevenZipPath);
    default:
        return false;
    }
}

void ArchiveCreator::setZipPath(const QString &path)
{
    m_zipPath = path;
}

void ArchiveCreator::setSevenZipPath(const QString &path)
{
    m_sevenZipPath = path;
}

// ════════════════════════════════════════════════════════════
// Compression
// ════════════════════════════════════════════════════════════

CompressionResult ArchiveCreator::compress(const QStringList &inputPaths,
                                           const QString &outputArchive,
                                           ArchiveFormat format)
{
    CompressionResult result;
    result.inputFiles = inputPaths;

    if (inputPaths.isEmpty()) {
        result.error = QStringLiteral("No input files specified");
        emit errorOccurred(result.error);
        return result;
    }

    if (outputArchive.isEmpty()) {
        result.error = QStringLiteral("No output path specified");
        emit errorOccurred(result.error);
        return result;
    }

    if (!canCompress(format)) {
        QString fmtName = (format == ArchiveFormat::SevenZip) ? QStringLiteral("7z") : QStringLiteral("zip");
        result.error = QStringLiteral("No tool available for %1 compression").arg(fmtName);
        emit errorOccurred(result.error);
        return result;
    }

    m_cancelled = false;
    m_running = true;

    result.originalSize = calculateTotalSize(inputPaths);
    result.outputPath = outputArchive;

    emit compressionStarted(outputArchive);

    switch (format) {
    case ArchiveFormat::ZIP:
        result = compressZip(inputPaths, outputArchive);
        break;
    case ArchiveFormat::SevenZip:
        result = compress7z(inputPaths, outputArchive);
        break;
    default:
        result.error = QStringLiteral("Unsupported compression format");
        break;
    }

    m_running = false;

    if (!result.error.isEmpty()) {
        emit errorOccurred(result.error);
    }

    emit compressionCompleted(result);
    return result;
}

QList<CompressionResult> ArchiveCreator::batchCompress(const QStringList &dirs,
                                                        const QString &outputDir,
                                                        ArchiveFormat format)
{
    QList<CompressionResult> results;
    m_cancelled = false;

    // Ensure output dir exists
    QDir().mkpath(outputDir);

    QString ext = (format == ArchiveFormat::SevenZip) ? QStringLiteral(".7z") : QStringLiteral(".zip");

    int total = dirs.size();
    for (int i = 0; i < total; ++i) {
        if (m_cancelled) break;

        QFileInfo dirInfo(dirs[i]);
        QString archiveName = dirInfo.fileName() + ext;
        QString outputPath = outputDir + QStringLiteral("/") + archiveName;

        emit batchProgress(i + 1, total, dirInfo.fileName());

        // Gather files in directory
        QStringList inputFiles;
        if (dirInfo.isDir()) {
            QDirIterator it(dirs[i], QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                inputFiles << it.filePath();
            }
        } else {
            inputFiles << dirs[i];
        }

        CompressionResult result = compress(inputFiles, outputPath, format);
        results.append(result);
    }

    return results;
}

void ArchiveCreator::cancel()
{
    m_cancelled = true;
    if (m_currentProcess) {
        m_currentProcess->terminate();
        if (!m_currentProcess->waitForFinished(3000)) {
            m_currentProcess->kill();
        }
    }
}

// ════════════════════════════════════════════════════════════
// Format-Specific Compression
// ════════════════════════════════════════════════════════════

CompressionResult ArchiveCreator::compressZip(const QStringList &inputPaths,
                                               const QString &outputArchive)
{
    CompressionResult result;
    result.inputFiles = inputPaths;
    result.outputPath = outputArchive;
    result.originalSize = calculateTotalSize(inputPaths);

    // Remove existing archive if present (zip -j appends by default)
    if (QFile::exists(outputArchive)) {
        QFile::remove(outputArchive);
    }

    // Build args: zip -j output.zip file1 file2 ...
    // Use -j to store just filenames (not full paths)
    QStringList args;
    args << QStringLiteral("-j") << outputArchive;

    // If all inputs are in the same directory, use -r for relative paths
    bool allSameDir = true;
    QString commonDir;
    for (const QString &path : inputPaths) {
        QFileInfo fi(path);
        if (fi.isDir()) {
            allSameDir = false;
            break;
        }
        QString dir = fi.absolutePath();
        if (commonDir.isEmpty())
            commonDir = dir;
        else if (dir != commonDir)
            allSameDir = false;
    }

    if (!allSameDir || inputPaths.size() == 1) {
        // Use junk paths mode for mixed directories
        args.clear();
        args << QStringLiteral("-j") << outputArchive;
        for (const QString &path : inputPaths) {
            if (m_cancelled) {
                result.error = QStringLiteral("Cancelled");
                return result;
            }
            args << path;
        }
    } else {
        for (const QString &path : inputPaths) {
            args << path;
        }
    }

    emit compressionProgress(0, QStringLiteral("Compressing to ZIP..."));

    ProcessResult proc = runProcess(m_zipPath, args);

    if (m_cancelled) {
        result.error = QStringLiteral("Cancelled");
        return result;
    }

    if (proc.exitCode != 0) {
        result.error = QStringLiteral("zip failed (exit %1): %2")
                           .arg(proc.exitCode)
                           .arg(proc.stdErr.trimmed());
        return result;
    }

    QFileInfo outInfo(outputArchive);
    if (outInfo.exists()) {
        result.success = true;
        result.compressedSize = outInfo.size();
        result.filesCompressed = inputPaths.size();
    } else {
        result.error = QStringLiteral("Output archive not created");
    }

    emit compressionProgress(100, QStringLiteral("ZIP compression complete"));
    return result;
}

CompressionResult ArchiveCreator::compress7z(const QStringList &inputPaths,
                                              const QString &outputArchive)
{
    CompressionResult result;
    result.inputFiles = inputPaths;
    result.outputPath = outputArchive;
    result.originalSize = calculateTotalSize(inputPaths);

    // Remove existing archive if present
    if (QFile::exists(outputArchive)) {
        QFile::remove(outputArchive);
    }

    // Build args: 7z a output.7z file1 file2 ...
    QStringList args;
    args << QStringLiteral("a") << outputArchive;

    for (const QString &path : inputPaths) {
        if (m_cancelled) {
            result.error = QStringLiteral("Cancelled");
            return result;
        }
        args << path;
    }

    emit compressionProgress(0, QStringLiteral("Compressing to 7z..."));

    ProcessResult proc = runProcess(m_sevenZipPath, args);

    if (m_cancelled) {
        result.error = QStringLiteral("Cancelled");
        return result;
    }

    if (proc.exitCode != 0) {
        result.error = QStringLiteral("7z failed (exit %1): %2")
                           .arg(proc.exitCode)
                           .arg(proc.stdErr.trimmed());
        return result;
    }

    QFileInfo outInfo(outputArchive);
    if (outInfo.exists()) {
        result.success = true;
        result.compressedSize = outInfo.size();
        result.filesCompressed = inputPaths.size();
    } else {
        result.error = QStringLiteral("Output archive not created");
    }

    emit compressionProgress(100, QStringLiteral("7z compression complete"));
    return result;
}

// ════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════

QString ArchiveCreator::findTool(const QStringList &candidates) const
{
    for (const QString &name : candidates) {
        QString path = QStandardPaths::findExecutable(name);
        if (!path.isEmpty())
            return path;
    }
    return {};
}

bool ArchiveCreator::isToolAvailable(const QString &path) const
{
    return !path.isEmpty() && QFileInfo::exists(path);
}

ArchiveCreator::ProcessResult ArchiveCreator::runProcess(const QString &program,
                                                          const QStringList &args,
                                                          int timeoutMs)
{
    ProcessResult result;
    QProcess process;
    m_currentProcess = &process;

    process.start(program, args);
    if (!process.waitForStarted(5000)) {
        result.exitCode = -1;
        result.stdErr = QStringLiteral("Failed to start: ") + program;
        m_currentProcess = nullptr;
        return result;
    }

    if (!process.waitForFinished(timeoutMs)) {
        result.timedOut = true;
        result.stdErr = QStringLiteral("Process timed out");
        process.kill();
        m_currentProcess = nullptr;
        return result;
    }

    result.exitCode = process.exitCode();
    result.stdOut = QString::fromLocal8Bit(process.readAllStandardOutput());
    result.stdErr = QString::fromLocal8Bit(process.readAllStandardError());

    m_currentProcess = nullptr;
    return result;
}

qint64 ArchiveCreator::calculateTotalSize(const QStringList &paths) const
{
    qint64 total = 0;
    for (const QString &path : paths) {
        QFileInfo fi(path);
        if (fi.isDir()) {
            QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                total += it.fileInfo().size();
            }
        } else {
            total += fi.size();
        }
    }
    return total;
}

} // namespace Remus
