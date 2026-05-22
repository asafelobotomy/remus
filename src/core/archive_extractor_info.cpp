#include "archive_extractor.h"

#include <archive.h>
#include <archive_entry.h>
#include <memory>
#include <QFileInfo>

namespace Remus {

ArchiveInfo ArchiveExtractor::getArchiveInfo(const QString &path)
{
    ArchiveInfo info;
    info.path = path;
    info.format = detectFormat(path);
    info.compressedSize = QFileInfo(path).size();

    using ArchivePtr = std::unique_ptr<archive, decltype(&archive_read_free)>;
    ArchivePtr a(archive_read_new(), archive_read_free);
    archive_read_support_filter_all(a.get());
    archive_read_support_format_all(a.get());

    const QByteArray pathBytes = path.toUtf8();
    if (archive_read_open_filename(a.get(), pathBytes.constData(), 65536) != ARCHIVE_OK)
        return info;

    archive_entry *entry = nullptr;
    while (archive_read_next_header(a.get(), &entry) == ARCHIVE_OK) {
        if (archive_entry_filetype(entry) == AE_IFDIR) {
            archive_read_data_skip(a.get());
            continue;
        }

        const QString rawPath = QString::fromUtf8(archive_entry_pathname(entry));
        const QString normalized = normalizeArchiveMemberPath(rawPath);

        if (normalized.isEmpty()) {
            if (!rawPath.isEmpty())
                info.unsafeEntries.append(rawPath);
        } else {
            const qint64 size = qMax<qint64>(0, archive_entry_size(entry));
            info.contents.append(normalized);
            info.entrySizes.insert(normalized, size);
            info.uncompressedSize += size;
            info.fileCount++;
        }

        archive_read_data_skip(a.get());
    }

    return info;
}

} // namespace Remus
