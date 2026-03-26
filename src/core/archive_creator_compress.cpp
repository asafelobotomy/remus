#include "archive_creator.h"

#include <QFile>
#include <QFileInfo>

namespace Remus {

CompressionResult ArchiveCreator::compressZip(const QStringList &inputPaths,
                                              const QString &outputArchive)
{
    CompressionResult result;
    result.inputFiles = inputPaths;
    result.outputPath = outputArchive;
    result.originalSize = calculateTotalSize(inputPaths);

    if (QFile::exists(outputArchive)) {
        QFile::remove(outputArchive);
    }

    QStringList args;
    args << QStringLiteral("-j") << outputArchive;

    bool allSameDir = true;
    QString commonDir;
    for (const QString &path : inputPaths) {
        const QFileInfo fileInfo(path);
        if (fileInfo.isDir()) {
            allSameDir = false;
            break;
        }

        const QString dir = fileInfo.absolutePath();
        if (commonDir.isEmpty()) {
            commonDir = dir;
        } else if (dir != commonDir) {
            allSameDir = false;
        }
    }

    if (!allSameDir || inputPaths.size() == 1) {
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

    const ProcessResult proc = runProcess(m_zipPath, args);
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

    const QFileInfo outInfo(outputArchive);
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

    if (QFile::exists(outputArchive)) {
        QFile::remove(outputArchive);
    }

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

    const ProcessResult proc = runProcess(m_sevenZipPath, args);
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

    const QFileInfo outInfo(outputArchive);
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

CompressionResult ArchiveCreator::compressRelativePaths(const QStringList &relativePaths,
                                                        const QString &rootDir,
                                                        const QString &outputArchive,
                                                        ArchiveFormat format)
{
    CompressionResult result;
    result.inputFiles = relativePaths;
    result.outputPath = outputArchive;
    result.originalSize = calculateTotalSize({rootDir});

    if (QFile::exists(outputArchive)) {
        QFile::remove(outputArchive);
    }

    QStringList args;
    ProcessResult proc;

    switch (format) {
    case ArchiveFormat::ZIP:
        args << outputArchive;
        for (const QString &path : relativePaths) {
            if (m_cancelled) {
                result.error = QStringLiteral("Cancelled");
                return result;
            }
            args << path;
        }

        emit compressionProgress(0, QStringLiteral("Compressing directory contents to ZIP..."));
        proc = runProcessInDirectory(m_zipPath, args, rootDir);
        if (proc.exitCode != 0) {
            result.error = QStringLiteral("zip failed (exit %1): %2")
                               .arg(proc.exitCode)
                               .arg(proc.stdErr.trimmed());
            return result;
        }
        emit compressionProgress(100, QStringLiteral("ZIP compression complete"));
        break;

    case ArchiveFormat::SevenZip:
        args << QStringLiteral("a") << outputArchive;
        for (const QString &path : relativePaths) {
            if (m_cancelled) {
                result.error = QStringLiteral("Cancelled");
                return result;
            }
            args << path;
        }

        emit compressionProgress(0, QStringLiteral("Compressing directory contents to 7z..."));
        proc = runProcessInDirectory(m_sevenZipPath, args, rootDir);
        if (proc.exitCode != 0) {
            result.error = QStringLiteral("7z failed (exit %1): %2")
                               .arg(proc.exitCode)
                               .arg(proc.stdErr.trimmed());
            return result;
        }
        emit compressionProgress(100, QStringLiteral("7z compression complete"));
        break;

    default:
        result.error = QStringLiteral("Unsupported compression format");
        return result;
    }

    if (m_cancelled) {
        result.error = QStringLiteral("Cancelled");
        return result;
    }

    const QFileInfo outInfo(outputArchive);
    if (!outInfo.exists()) {
        result.error = QStringLiteral("Output archive not created");
        return result;
    }

    result.success = true;
    result.compressedSize = outInfo.size();
    result.filesCompressed = relativePaths.size();
    return result;
}

} // namespace Remus