#include "archive_extractor.h"

#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>

namespace Remus {

ArchiveExtractor::ArchiveExtractor(QObject *parent)
    : ExternalToolRunner(parent)
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

QString ArchiveExtractor::normalizeArchiveMemberPath(const QString &path)
{
    QString normalized = path;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    normalized = QDir::fromNativeSeparators(normalized).trimmed();
    if (normalized.isEmpty()) {
        return {};
    }

    while (normalized.startsWith(QStringLiteral("./"))) {
        normalized.remove(0, 2);
    }

    if (normalized.isEmpty() || normalized == QStringLiteral(".") || normalized == QStringLiteral("..")) {
        return {};
    }

    if (normalized.startsWith(QLatin1Char('/')) || normalized.startsWith(QLatin1Char('~'))) {
        return {};
    }

    const QFileInfo info(normalized);
    if (info.isAbsolute()) {
        return {};
    }

    const QStringList segments = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.isEmpty()) {
        return {};
    }

    for (const QString &segment : segments) {
        if (segment == QStringLiteral(".") || segment == QStringLiteral("..")) {
            return {};
        }
    }

    return segments.join(QLatin1Char('/'));
}

bool ArchiveExtractor::isPathWithinDirectory(const QString &rootPath, const QString &candidatePath)
{
    const QFileInfo rootInfo(rootPath);
    const QFileInfo candidateInfo(candidatePath);
    const QString canonicalRoot = rootInfo.canonicalFilePath().isEmpty()
        ? QDir(rootPath).absolutePath()
        : rootInfo.canonicalFilePath();
    const QString canonicalCandidate = candidateInfo.canonicalFilePath().isEmpty()
        ? QDir(candidatePath).absolutePath()
        : candidateInfo.canonicalFilePath();

    if (canonicalRoot.isEmpty() || canonicalCandidate.isEmpty()) {
        return false;
    }

    return canonicalCandidate == canonicalRoot ||
        canonicalCandidate.startsWith(canonicalRoot + QDir::separator());
}

QString ArchiveExtractor::validateArchiveEntries(const ArchiveInfo &info)
{
    if (!info.unsafeEntries.isEmpty()) {
        return QStringLiteral("Archive contains unsafe path entries: %1")
            .arg(info.unsafeEntries.first());
    }

    return {};
}

QString ArchiveExtractor::validateExtractedFiles(const QString &outputDir, const QStringList &files)
{
    for (const QString &file : files) {
        if (!isPathWithinDirectory(outputDir, file)) {
            return QStringLiteral("Extraction escaped output directory: %1").arg(file);
        }
    }

    return {};
}

bool ArchiveExtractor::isToolAvailable(const QString &tool) const
{
    if (tool.isEmpty()) return false;
    
    ProcessResult result = const_cast<ArchiveExtractor*>(this)
                               ->runProcess(tool, QStringList() << "--version", 3000);
    return result.started && result.exitStatus == QProcess::NormalExit;
}

QString ArchiveExtractor::findTool(const QStringList &candidates) const
{
    for (const QString &tool : candidates) {
        const QString executable = QStandardPaths::findExecutable(tool);
        if (!executable.isEmpty() && isToolAvailable(executable)) {
            return executable;
        }
    }
    return QString();
}

QStringList ArchiveExtractor::listFiles(const QString &dirPath) const
{
    QStringList files;
    QDirIterator it(dirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        files.append(it.filePath());
    }
    return files;
}

} // namespace Remus
