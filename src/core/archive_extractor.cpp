#include "archive_extractor.h"

#include <QDir>
#include <QFileInfo>

namespace Remus {

ArchiveExtractor::ArchiveExtractor(QObject *parent)
    : QObject(parent)
{
}

QMap<ArchiveFormat, bool> ArchiveExtractor::getAvailableTools() const
{
    QMap<ArchiveFormat, bool> available;
    available[ArchiveFormat::ZIP]    = true;
    available[ArchiveFormat::SevenZip] = true;
    available[ArchiveFormat::RAR]    = true;
    available[ArchiveFormat::GZip]   = true;
    available[ArchiveFormat::TarGz]  = true;
    available[ArchiveFormat::TarBz2] = true;
    return available;
}

bool ArchiveExtractor::canExtract(ArchiveFormat format) const
{
    switch (format) {
    case ArchiveFormat::ZIP:
    case ArchiveFormat::SevenZip:
    case ArchiveFormat::RAR:
    case ArchiveFormat::GZip:
    case ArchiveFormat::TarGz:
    case ArchiveFormat::TarBz2:
        return true;
    default:
        return false;
    }
}

bool ArchiveExtractor::canExtract(const QString &path) const
{
    return canExtract(detectFormat(path));
}

ArchiveFormat ArchiveExtractor::detectFormat(const QString &path)
{
    const QString ext = QFileInfo(path).suffix().toLower();

    if (ext == QLatin1String("zip")) return ArchiveFormat::ZIP;
    if (ext == QLatin1String("7z"))  return ArchiveFormat::SevenZip;
    if (ext == QLatin1String("rar")) return ArchiveFormat::RAR;
    if (ext == QLatin1String("tgz")) return ArchiveFormat::TarGz;
    if (ext == QLatin1String("gz"))  return ArchiveFormat::GZip;
    if (ext == QLatin1String("bz2")) return ArchiveFormat::TarBz2;

    return ArchiveFormat::Unknown;
}

QString ArchiveExtractor::normalizeArchiveMemberPath(const QString &path)
{
    QString normalized = path;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    normalized = QDir::fromNativeSeparators(normalized).trimmed();
    if (normalized.isEmpty())
        return {};

    while (normalized.startsWith(QStringLiteral("./")))
        normalized.remove(0, 2);

    if (normalized.isEmpty() || normalized == QStringLiteral(".") || normalized == QStringLiteral(".."))
        return {};

    if (normalized.startsWith(QLatin1Char('/')) || normalized.startsWith(QLatin1Char('~')))
        return {};

    if (QFileInfo(normalized).isAbsolute())
        return {};

    const QStringList segments = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.isEmpty())
        return {};

    for (const QString &segment : segments) {
        if (segment == QStringLiteral(".") || segment == QStringLiteral(".."))
            return {};
    }

    return segments.join(QLatin1Char('/'));
}

} // namespace Remus
