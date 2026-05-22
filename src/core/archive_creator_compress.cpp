#include "archive_creator.h"

#include <archive.h>
#include <archive_entry.h>
#include <memory>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace Remus {

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
        if (!fi.isFile())
            continue;

        QFile inputFile(input.sourcePath);
        if (!inputFile.open(QIODevice::ReadOnly))
            continue;

        EntryPtr entry(archive_entry_new(), archive_entry_free);
        const QByteArray archivePathBytes = QDir::fromNativeSeparators(input.archivePath).toUtf8();
        archive_entry_set_pathname(entry.get(), archivePathBytes.constData());
        archive_entry_set_size(entry.get(), fi.size());
        archive_entry_set_filetype(entry.get(), AE_IFREG);
        archive_entry_set_perm(entry.get(), 0644);
        archive_entry_set_mtime(entry.get(), fi.lastModified().toSecsSinceEpoch(), 0);
        archive_write_header(a.get(), entry.get());

        char buf[65536];
        qint64 bytesRead;
        while ((bytesRead = inputFile.read(buf, sizeof(buf))) > 0)
            archive_write_data(a.get(), buf, static_cast<size_t>(bytesRead));

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

    result.success = true;
    result.compressedSize = outInfo.size();
    return result;
}

} // namespace Remus
