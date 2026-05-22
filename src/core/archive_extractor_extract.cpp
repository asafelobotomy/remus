#include "archive_extractor.h"

#include <archive.h>
#include <archive_entry.h>
#include <memory>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace Remus {

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

bool failureRatioExceeded(int successes, int failures)
{
    return failures >= 3 && failures >= (successes * 3);
}

QString summarizeFailures(const QString &prefix, int failedFiles)
{
    return QStringLiteral("%1 (%2 failed file%3)")
        .arg(prefix)
        .arg(failedFiles)
        .arg(failedFiles == 1 ? QString() : QStringLiteral("s"));
}

// Streams all data blocks from archive reader |ar| into a QFile.
// Returns true on success (EOF reached without error).
bool copyEntryToFile(archive *ar, QFile &out)
{
    const void *buff;
    size_t size;
    la_int64_t offset;
    for (;;) {
        const int r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF) return true;
        if (r < ARCHIVE_OK) return false;
        if (out.write(static_cast<const char *>(buff), static_cast<qint64>(size)) < 0)
            return false;
    }
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

ExtractionResult ArchiveExtractor::extract(const QString &archivePath,
                                           const QString &outputDir,
                                           bool createSubfolder)
{
    ExtractionResult result;
    result.archivePath = archivePath;

    if (!QFileInfo::exists(archivePath)) {
        result.error = QStringLiteral("Archive file not found");
        return result;
    }

    if (detectFormat(archivePath) == ArchiveFormat::Unknown) {
        result.error = QStringLiteral("Unsupported archive format");
        return result;
    }

    QString targetDir = outputDir;
    if (targetDir.isEmpty())
        targetDir = QFileInfo(archivePath).absolutePath();
    if (createSubfolder)
        targetDir = QDir(targetDir).filePath(QFileInfo(archivePath).completeBaseName());

    result.outputDir = targetDir;
    QDir().mkpath(targetDir);

    emit extractionStarted(archivePath, targetDir);
    result = extractToDir(archivePath, targetDir);
    result.archivePath = archivePath;
    result.outputDir = targetDir;

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

    const QString normalized = normalizeArchiveMemberPath(fileName);
    if (normalized.isEmpty()) {
        result.error = QStringLiteral("Unsafe archive member path");
        return result;
    }

    QDir().mkpath(outputDir);
    result = extractToDir(archivePath, outputDir, normalized);
    result.archivePath = archivePath;
    result.outputDir = outputDir;

    if (result.success && result.filesExtracted == 0) {
        result.success = false;
        result.error = QStringLiteral("Archive member not found: %1").arg(normalized);
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
        if (m_cancelled) break;
        results.append(extract(archivePath, outputDir, createSubfolders));
        emit batchProgress(++completed, total);
    }

    return results;
}

QByteArray ArchiveExtractor::readMemberPrefix(const QString &archivePath,
                                               const QString &memberPath,
                                               qint64 maxBytes)
{
    if (maxBytes <= 0) return {};

    const QString normalized = normalizeArchiveMemberPath(memberPath);
    if (normalized.isEmpty()) return {};

    using ArchivePtr = std::unique_ptr<archive, decltype(&archive_read_free)>;
    ArchivePtr a(archive_read_new(), archive_read_free);
    archive_read_support_filter_all(a.get());
    archive_read_support_format_all(a.get());

    const QByteArray pathBytes = archivePath.toUtf8();
    if (archive_read_open_filename(a.get(), pathBytes.constData(), 65536) != ARCHIVE_OK)
        return {};

    archive_entry *entry = nullptr;
    while (archive_read_next_header(a.get(), &entry) == ARCHIVE_OK) {
        const QString entryNorm = normalizeArchiveMemberPath(
            QString::fromUtf8(archive_entry_pathname(entry)));
        if (entryNorm != normalized) {
            archive_read_data_skip(a.get());
            continue;
        }

        QByteArray data;
        data.reserve(static_cast<int>(maxBytes));
        char buf[65536];
        qint64 remaining = maxBytes;
        la_ssize_t n;
        while (remaining > 0) {
            const la_ssize_t toRead = static_cast<la_ssize_t>(
                qMin<qint64>(remaining, static_cast<qint64>(sizeof(buf))));
            n = archive_read_data(a.get(), buf, static_cast<size_t>(toRead));
            if (n <= 0) break;
            data.append(buf, static_cast<int>(n));
            remaining -= n;
        }
        return data;
    }

    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Private extraction core
// ─────────────────────────────────────────────────────────────────────────────

ExtractionResult ArchiveExtractor::extractToDir(const QString &archivePath,
                                                 const QString &outputDir,
                                                 const QString &singleMember)
{
    ExtractionResult result;
    result.archivePath = archivePath;
    result.outputDir = outputDir;

    const ArchiveInfo info = getArchiveInfo(archivePath);
    if (!info.unsafeEntries.isEmpty()) {
        result.error = QStringLiteral("Archive contains unsafe path entries: %1")
            .arg(info.unsafeEntries.first());
        return result;
    }

    using ArchivePtr = std::unique_ptr<archive, decltype(&archive_read_free)>;
    ArchivePtr a(archive_read_new(), archive_read_free);
    archive_read_support_filter_all(a.get());
    archive_read_support_format_all(a.get());

    const QByteArray pathBytes = archivePath.toUtf8();
    if (archive_read_open_filename(a.get(), pathBytes.constData(), 65536) != ARCHIVE_OK) {
        result.error = QString::fromUtf8(archive_error_string(a.get()));
        if (result.error.isEmpty())
            result.error = QStringLiteral("Failed to open archive");
        return result;
    }

    archive_entry *entry = nullptr;
    int readStatus = ARCHIVE_OK;
    while ((readStatus = archive_read_next_header(a.get(), &entry)) == ARCHIVE_OK) {
        const unsigned ae_type = archive_entry_filetype(entry);

        // Skip directories and symlinks — only extract regular files
        if (ae_type == AE_IFDIR || ae_type == AE_IFLNK) {
            archive_read_data_skip(a.get());
            continue;
        }

        const QString rawPath = QString::fromUtf8(archive_entry_pathname(entry));
        const QString normalized = normalizeArchiveMemberPath(rawPath);

        // When extracting a single named member, skip non-matching entries
        if (!singleMember.isEmpty() && normalized != singleMember) {
            archive_read_data_skip(a.get());
            continue;
        }

        const QString destPath = QDir(outputDir).filePath(normalized);
        QDir().mkpath(QFileInfo(destPath).absolutePath());

        QFile outFile(destPath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            qWarning() << "extractToDir: cannot open for write:" << destPath;
            result.failedFiles++;
            archive_read_data_skip(a.get());
            if (failureRatioExceeded(result.filesExtracted, result.failedFiles)) {
                result.error = summarizeFailures(QStringLiteral("Extraction aborted after too many file failures"),
                                                 result.failedFiles);
                return result;
            }
            continue;
        }

        if (!copyEntryToFile(a.get(), outFile)) {
            qWarning() << "extractToDir: data error for entry:" << normalized;
            outFile.close();
            outFile.remove();
            result.failedFiles++;
            if (failureRatioExceeded(result.filesExtracted, result.failedFiles)) {
                result.error = summarizeFailures(QStringLiteral("Extraction aborted after too many file failures"),
                                                 result.failedFiles);
                return result;
            }
            continue;
        }
        outFile.close();

        result.extractedFiles.append(destPath);
        result.bytesExtracted += outFile.size();
        result.filesExtracted++;

        emit extractionProgress(0, normalized);
    }

    if (readStatus != ARCHIVE_EOF) {
        result.failedFiles++;
        result.error = QStringLiteral("Archive read failed: %1")
            .arg(QString::fromUtf8(archive_error_string(a.get())));
        return result;
    }

    result.success = (result.filesExtracted > 0);
    if (result.failedFiles > 0 && result.success) {
        result.error = summarizeFailures(QStringLiteral("Extraction completed with skipped files"),
                                         result.failedFiles);
    } else if (!result.success && result.error.isEmpty()) {
        result.error = summarizeFailures(QStringLiteral("Extraction completed without any successful files"),
                                         result.failedFiles);
    }
    return result;
}

} // namespace Remus
