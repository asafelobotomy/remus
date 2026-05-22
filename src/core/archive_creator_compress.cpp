#include "archive_creator.h"

#include <archive.h>
#include <archive_entry.h>
#include <memory>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace Remus {

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

} // namespace

CompressionResult ArchiveCreator::compressFiles(const QList<ArchiveInputEntry> &entries,
                                                 const QString &outputArchive)
{
    CompressionResult result;
    result.outputPath = outputArchive;

    if (QFile::exists(outputArchive))
        QFile::remove(outputArchive);

    using ArchivePtr = std::unique_ptr<archive, decltype(&archive_write_free)>;
    ArchivePtr a(archive_write_new(), archive_write_free);
    archive_write_set_format_zip(a.get());

    const QByteArray outBytes = outputArchive.toUtf8();
    if (archive_write_open_filename(a.get(), outBytes.constData()) != ARCHIVE_OK) {
        result.error = QStringLiteral("Failed to create archive: %1")
            .arg(QString::fromUtf8(archive_error_string(a.get())));
        return result;
    }

    using EntryPtr = std::unique_ptr<archive_entry, decltype(&archive_entry_free)>;

    for (const ArchiveInputEntry &input : entries) {
        if (m_cancelled) {
            result.error = QStringLiteral("Cancelled");
            return result;
        }

        const QFileInfo fi(input.sourcePath);
        if (!fi.isFile()) {
            result.failedFiles++;
            if (failureRatioExceeded(result.filesCompressed, result.failedFiles)) {
                result.error = summarizeFailures(QStringLiteral("Compression aborted after too many file failures"),
                                                 result.failedFiles);
                return result;
            }
            continue;
        }

        QFile inputFile(input.sourcePath);
        if (!inputFile.open(QIODevice::ReadOnly)) {
            result.failedFiles++;
            if (failureRatioExceeded(result.filesCompressed, result.failedFiles)) {
                result.error = summarizeFailures(QStringLiteral("Compression aborted after too many file failures"),
                                                 result.failedFiles);
                return result;
            }
            continue;
        }

        EntryPtr entry(archive_entry_new(), archive_entry_free);
        const QByteArray archivePathBytes = QDir::fromNativeSeparators(input.archivePath).toUtf8();
        archive_entry_set_pathname(entry.get(), archivePathBytes.constData());
        archive_entry_set_size(entry.get(), fi.size());
        archive_entry_set_filetype(entry.get(), AE_IFREG);
        archive_entry_set_perm(entry.get(), 0644);
        archive_entry_set_mtime(entry.get(), fi.lastModified().toSecsSinceEpoch(), 0);
        if (archive_write_header(a.get(), entry.get()) != ARCHIVE_OK) {
            result.failedFiles++;
            if (failureRatioExceeded(result.filesCompressed, result.failedFiles)) {
                result.error = summarizeFailures(QStringLiteral("Compression aborted after too many file failures"),
                                                 result.failedFiles);
                return result;
            }
            continue;
        }

        char buf[65536];
        qint64 bytesRead;
        bool entryOk = true;
        while ((bytesRead = inputFile.read(buf, sizeof(buf))) > 0) {
            const la_ssize_t written = archive_write_data(a.get(), buf, static_cast<size_t>(bytesRead));
            if (written < 0 || written != bytesRead) {
                entryOk = false;
                break;
            }
        }

        if (bytesRead < 0)
            entryOk = false;

        if (!entryOk) {
            result.failedFiles++;
            if (failureRatioExceeded(result.filesCompressed, result.failedFiles)) {
                result.error = summarizeFailures(QStringLiteral("Compression aborted after too many file failures"),
                                                 result.failedFiles);
                return result;
            }
            continue;
        }

        result.filesCompressed++;
    }

    archive_write_close(a.get());

    if (m_cancelled) {
        result.error = QStringLiteral("Cancelled");
        return result;
    }

    const QFileInfo outInfo(outputArchive);
    if (!outInfo.exists()) {
        result.error = QStringLiteral("Output archive not created");
        return result;
    }

    result.success = (result.filesCompressed > 0);
    if (result.failedFiles > 0 && result.success) {
        result.error = summarizeFailures(QStringLiteral("Compression completed with skipped files"),
                                         result.failedFiles);
    } else if (!result.success && result.error.isEmpty()) {
        result.error = summarizeFailures(QStringLiteral("Compression completed without any successful files"),
                                         result.failedFiles);
    }
    result.compressedSize = outInfo.size();
    return result;
}

} // namespace Remus
