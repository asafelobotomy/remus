#include "archive_creator.h"

#include "constants/files.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

namespace Remus {

ArchiveCreator::ArchiveCreator(QObject *parent)
    : QObject(parent)
{
}

QMap<ArchiveFormat, bool> ArchiveCreator::getAvailableTools() const
{
    QMap<ArchiveFormat, bool> tools;
    tools[ArchiveFormat::ZIP] = true;
    tools[ArchiveFormat::SevenZip] = false;
    return tools;
}

bool ArchiveCreator::canCompress(ArchiveFormat format) const
{
    return format == ArchiveFormat::ZIP;
}

CompressionResult ArchiveCreator::compress(const QStringList &inputPaths,
                                           const QString &outputArchive,
                                           ArchiveFormat format)
{
    CompressionResult result;
    result.inputFiles = inputPaths;
    result.outputPath = outputArchive;

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
        result.error = QStringLiteral("Unsupported compression format");
        emit errorOccurred(result.error);
        return result;
    }

    m_cancelled = false;
    m_running = true;
    result.originalSize = calculateTotalSize(inputPaths);
    emit compressionStarted(outputArchive);

    QList<ArchiveInputEntry> entries;
    QSet<QString> archivePaths;
    int failedInputs = 0;
    for (const QString &path : inputPaths) {
        const QFileInfo fi(path);
        if (!fi.isFile()) {
            failedInputs++;
            continue;
        }

        const QString archivePath = fi.fileName();
        if (archivePaths.contains(archivePath)) {
            failedInputs++;
            continue;
        }

        archivePaths.insert(archivePath);
        entries.append({fi.absoluteFilePath(), archivePath});
    }

    if (entries.isEmpty()) {
        result.error = QStringLiteral("No input files found");
        m_running = false;
        emit errorOccurred(result.error);
        emit compressionCompleted(result);
        return result;
    }

    result = compressFiles(entries, outputArchive);
    result.failedFiles += failedInputs;
    if (result.failedFiles >= 3 && result.failedFiles >= (result.filesCompressed * 3)) {
        result.success = false;
        result.error = QStringLiteral("Compression aborted after too many file failures (%1 failed files)")
            .arg(result.failedFiles);
    } else if (result.failedFiles > 0 && result.success) {
        result.error = QStringLiteral("Compression completed with skipped files (%1 failed files)")
            .arg(result.failedFiles);
    }

    m_running = false;
    if (!result.error.isEmpty())
        emit errorOccurred(result.error);
    emit compressionCompleted(result);
    return result;
}

CompressionResult ArchiveCreator::compressDirectoryContents(const QString &rootDir,
                                                             const QString &outputArchive,
                                                             ArchiveFormat format)
{
    CompressionResult result;
    result.inputFiles = {rootDir};
    result.outputPath = outputArchive;

    if (!QFileInfo(rootDir).isDir()) {
        result.error = QStringLiteral("Directory not found: %1").arg(rootDir);
        emit errorOccurred(result.error);
        return result;
    }
    if (outputArchive.isEmpty()) {
        result.error = QStringLiteral("No output path specified");
        emit errorOccurred(result.error);
        return result;
    }
    if (!canCompress(format)) {
        result.error = QStringLiteral("Unsupported compression format");
        emit errorOccurred(result.error);
        return result;
    }

    const QStringList relativePaths = collectRelativeFilePaths(rootDir);
    if (relativePaths.isEmpty()) {
        result.error = QStringLiteral("No files found under directory: %1").arg(rootDir);
        emit errorOccurred(result.error);
        return result;
    }

    m_cancelled = false;
    m_running = true;
    result.originalSize = calculateTotalSize({rootDir});
    emit compressionStarted(outputArchive);

    QList<ArchiveInputEntry> entries;
    entries.reserve(relativePaths.size());
    for (const QString &relativePath : relativePaths) {
        entries.append({QDir(rootDir).filePath(relativePath), relativePath});
    }

    result = compressFiles(entries, outputArchive);

    m_running = false;
    if (!result.error.isEmpty())
        emit errorOccurred(result.error);
    emit compressionCompleted(result);
    return result;
}

QList<CompressionResult> ArchiveCreator::batchCompress(const QStringList &dirs,
                                                        const QString &outputDir,
                                                        ArchiveFormat format)
{
    QList<CompressionResult> results;
    m_cancelled = false;

    if (!canCompress(format)) {
        emit errorOccurred(QStringLiteral("Unsupported compression format"));
        return results;
    }

    QDir().mkpath(outputDir);
    const QString ext = Constants::Files::ZIP;
    const int total = dirs.size();

    for (int i = 0; i < total; ++i) {
        if (m_cancelled) break;
        const QFileInfo dirInfo(dirs[i]);
        const QString outputPath = outputDir + QStringLiteral("/") + dirInfo.fileName() + ext;
        emit batchProgress(i + 1, total, dirInfo.fileName());
        if (dirInfo.isDir())
            results.append(compressDirectoryContents(dirs[i], outputPath, format));
        else
            results.append(compress({dirs[i]}, outputPath, format));
    }

    return results;
}

void ArchiveCreator::cancel()
{
    m_cancelled = true;
}

QStringList ArchiveCreator::collectRelativeFilePaths(const QString &rootDir) const
{
    QStringList files;
    QDir root(rootDir);
    QDirIterator it(rootDir, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        files << root.relativeFilePath(it.filePath());
    }
    files.sort();
    return files;
}

qint64 ArchiveCreator::calculateTotalSize(const QStringList &paths) const
{
    qint64 total = 0;
    for (const QString &path : paths) {
        const QFileInfo fi(path);
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
