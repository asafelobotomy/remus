#include "rom_bundler.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include "constants/constants.h"

namespace Remus {

namespace {

static constexpr const char *MARKER_FILENAME = ".remus.md";
static constexpr const char *ARTWORK_SUBDIR  = "artwork";
static constexpr const char *BOXART_FILENAME = "boxfront.jpg";

QStringList collectArchiveEntries(const QString &rootDir)
{
    QStringList entries;
    QDir root(rootDir);
    QDirIterator it(rootDir,
                    QDir::Files | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        entries << root.relativeFilePath(it.filePath()).replace('\\', '/');
    }
    entries.sort();
    return entries;
}

} // namespace

RomBundler::BundleResult RomBundler::bundleStaged(
    const FileRecord              &patchedFile,
    const Database::MatchResult   &baseMatch,
    const GameMetadata            &metadata,
    const QString                 &destinationDir,
    const BundleConfig            &config)
{
    BundleResult result;

    const QString sourcePath = patchedFile.currentPath;
    if (sourcePath.isEmpty() || !QFile::exists(sourcePath)) {
        result.error = "Patched ROM not found: " + sourcePath;
        return result;
    }

    QDir destDir(destinationDir);
    if (!config.dryRun && !destDir.exists() && !destDir.mkpath(".")) {
        result.error = "Cannot create destination directory: " + destinationDir;
        return result;
    }

    const QString tempRoot = config.dryRun ? QDir::tempPath() : destDir.absolutePath();
    const QString tempBase = tempRoot + "/.remus_bundle_"
                           + QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir tempDir(tempBase);
    if (!tempDir.mkpath(".")) {
        result.error = "Cannot create temp directory: " + tempBase;
        return result;
    }

    auto cleanup = [&]() { tempDir.removeRecursively(); };

    const QString destRom = tempBase + "/" + QFileInfo(patchedFile.filename).fileName();
    if (!QFile::copy(sourcePath, destRom)) {
        result.error = "Failed to copy patched ROM to temp dir";
        cleanup();
        return result;
    }

    const QString markerPath = tempBase + "/" + MARKER_FILENAME;
    {
        QFile markerFile(markerPath);
        if (!markerFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            result.error = "Cannot write marker file";
            cleanup();
            return result;
        }
        QTextStream out(&markerFile);
        out << generateMarkerContent(patchedFile, baseMatch, metadata);
    }

    if (config.includeBoxArt && !config.artworkPath.isEmpty()
        && QFile::exists(config.artworkPath)) {
        const QString artDestDir = tempBase + "/" + ARTWORK_SUBDIR;
        QDir().mkpath(artDestDir);
        const QString artDest = artDestDir + "/" + BOXART_FILENAME;
        if (QFile::copy(config.artworkPath, artDest)) {
            result.archiveEntries << QString::fromLatin1(ARTWORK_SUBDIR) + "/" + BOXART_FILENAME;
        }
    }

    result.archiveEntries = collectArchiveEntries(tempBase);

    const QString ext = (config.outputFormat == ArchiveFormat::SevenZip) ? Constants::Files::SEVEN_Z : Constants::Files::ZIP;
    const QString baseName = QFileInfo(patchedFile.filename).completeBaseName();
    const QString outputArchive = destDir.absoluteFilePath(baseName + ext);

    if (config.dryRun) {
        qInfo() << "  [DRY-RUN] Would create bundle:" << outputArchive;
        result.success = true;
        result.outputPath = outputArchive;
        cleanup();
        return result;
    }

    CompressionResult cr = m_creator.compressDirectoryContents(
        tempBase, outputArchive, config.outputFormat);
    cleanup();

    if (!cr.success) {
        result.error = "Compression failed: " + cr.error;
        return result;
    }

    m_db.markFileProcessed(patchedFile.id, Constants::Engines::ProcessingStatus::BUNDLED);
    m_db.updateFilePath(patchedFile.id, outputArchive);

    result.success = true;
    result.outputPath = outputArchive;
    qInfo() << "  ✓ Bundle created (staged):" << outputArchive;

    emit progressMessage(QString("Bundled (staged): %1").arg(outputArchive));
    return result;
}

QString RomBundler::generateMarkerContent(const FileRecord            &file,
                                          const Database::MatchResult &match,
                                          const GameMetadata          &metadata) const
{
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QString out;
    QTextStream s(&out);

    s << "---\n";
    s << "remus_processed: true\n";
    s << "processed_at: " << now << "\n";
    s << "file_id: " << file.id << "\n";
    s << "match_method: " << match.matchMethod << "\n";
    s << "confidence: " << QString::number(match.confidence, 'f', 4) << "\n";
    s << "crc32: " << file.crc32 << "\n";
    s << "md5: " << file.md5 << "\n";
    s << "sha1: " << file.sha1 << "\n";
    s << "---\n\n";

    s << "# " << (metadata.title.isEmpty() ? match.gameTitle : metadata.title) << "\n\n";
    s << "Processed by Remus on " << now << ".\n\n";

    s << "## Identification\n\n";
    s << "| Field | Value |\n";
    s << "|---|---|\n";
    s << "| CRC32 | `" << file.crc32 << "` |\n";
    s << "| MD5   | `" << file.md5 << "` |\n";
    s << "| SHA1  | `" << file.sha1 << "` |\n";
    s << "| Match method | " << match.matchMethod << " |\n";
    s << "| Confidence | " << QString::number(match.confidence, 'f', 1) << "% |\n\n";

    s << "## Metadata\n\n";
    s << "| Field | Value |\n";
    s << "|---|---|\n";
    const QString title = metadata.title.isEmpty() ? match.gameTitle : metadata.title;
    s << "| Title | " << title << " |\n";
    if (!metadata.system.isEmpty())
        s << "| System | " << metadata.system << " |\n";
    if (!match.region.isEmpty())
        s << "| Region | " << match.region << " |\n";
    if (!match.publisher.isEmpty())
        s << "| Publisher | " << match.publisher << " |\n";
    if (!match.developer.isEmpty())
        s << "| Developer | " << match.developer << " |\n";
    if (match.releaseYear > 0)
        s << "| Release year | " << match.releaseYear << " |\n";
    if (!metadata.description.isEmpty())
        s << "\n### Description\n\n" << metadata.description << "\n";

    return out;
}

} // namespace Remus