#include "rom_bundler.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>

#include "constants/constants.h"
#include "logging_categories.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug()    qCDebug(logCore)
#define qInfo()     qCInfo(logCore)
#define qWarning()  qCWarning(logCore)
#define qCritical() qCCritical(logCore)

namespace Remus {

// ─── constants ────────────────────────────────────────────────────────────────

static constexpr const char *MARKER_FILENAME = ".remus.md";
static constexpr const char *ARTWORK_SUBDIR  = "artwork";
static constexpr const char *BOXART_FILENAME = "boxfront.jpg";

static bool isChdConvertiblePath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("cue") ||
           suffix == QStringLiteral("iso") ||
           suffix == QStringLiteral("img") ||
           suffix == QStringLiteral("gdi");
}

static bool clearDirectoryExcept(const QString &dirPath, const QString &keepPath)
{
    QDir root(dirPath);
    const QFileInfoList entries = root.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

    for (const QFileInfo &entry : entries) {
        if (entry.absoluteFilePath() == keepPath) {
            continue;
        }

        if (entry.isDir()) {
            if (!QDir(entry.absoluteFilePath()).removeRecursively()) {
                return false;
            }
        } else if (!QFile::remove(entry.absoluteFilePath())) {
            return false;
        }
    }

    return true;
}

static bool copyFileIntoDirectory(const QString &sourcePath,
                                  const QString &destinationDir,
                                  QString *error,
                                  const QString &targetFileName = QString())
{
    if (sourcePath.isEmpty() || !QFile::exists(sourcePath)) {
        if (error) {
            *error = QStringLiteral("Referenced disc file not found: %1").arg(sourcePath);
        }
        return false;
    }

    const QString fileName = targetFileName.isEmpty()
        ? QFileInfo(sourcePath).fileName()
        : targetFileName;
    const QString destinationPath = QDir(destinationDir).filePath(fileName);
    if (QFile::exists(destinationPath)) {
        return true;
    }

    if (!QFile::copy(sourcePath, destinationPath)) {
        if (error) {
            *error = QStringLiteral("Failed to stage disc file: %1").arg(fileName);
        }
        return false;
    }

    return true;
}

static QStringList getReferencedDiscFiles(const QString &manifestPath)
{
    const QString suffix = QFileInfo(manifestPath).suffix().toLower();
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QStringList referencedFiles;
    QTextStream input(&manifestFile);

    if (suffix == QStringLiteral("cue")) {
        static const QRegularExpression cueFilePattern(
            QStringLiteral("^\\s*FILE\\s+\"([^\"]+)\""),
            QRegularExpression::CaseInsensitiveOption);

        while (!input.atEnd()) {
            const QString line = input.readLine();
            const QRegularExpressionMatch match = cueFilePattern.match(line);
            if (match.hasMatch()) {
                referencedFiles << match.captured(1);
            }
        }
    } else if (suffix == QStringLiteral("gdi")) {
        static const QRegularExpression gdiFilePattern(
            QStringLiteral("^\\s*\\d+\\s+\\d+\\s+\\d+\\s+\\d+\\s+(.+?)\\s+\\d+\\s*$"));

        while (!input.atEnd()) {
            const QString line = input.readLine().trimmed();
            if (line.isEmpty()) {
                continue;
            }

            const QRegularExpressionMatch match = gdiFilePattern.match(line);
            if (!match.hasMatch()) {
                continue;
            }

            QString fileName = match.captured(1).trimmed();
            if (fileName.startsWith('"') && fileName.endsWith('"') && fileName.size() >= 2) {
                fileName = fileName.mid(1, fileName.size() - 2);
            }
            referencedFiles << fileName;
        }
    }

    referencedFiles.removeDuplicates();
    return referencedFiles;
}

static bool stageReferencedDiscFiles(const QString &manifestPath,
                                     const QString &destinationDir,
                                     QString *error)
{
    const QFileInfo manifestInfo(manifestPath);
    const QDir manifestDir = manifestInfo.dir();
    const QStringList referencedFiles = getReferencedDiscFiles(manifestPath);

    for (const QString &relativePath : referencedFiles) {
        const QString sourcePath = manifestDir.filePath(relativePath);
        if (!copyFileIntoDirectory(sourcePath, destinationDir, error, QFileInfo(relativePath).fileName())) {
            return false;
        }
    }

    return true;
}

// ─── construction ─────────────────────────────────────────────────────────────

RomBundler::RomBundler(Database &db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

// ─── public API ───────────────────────────────────────────────────────────────

bool RomBundler::isAlreadyBundled(const QString &archivePath)
{
    ArchiveInfo info = m_extractor.getArchiveInfo(archivePath);
    for (QString entry : info.contents) {
        entry = entry.trimmed().replace('\\', '/');
        while (entry.startsWith("./")) {
            entry.remove(0, 2);
        }
        if (entry == QLatin1String(MARKER_FILENAME)) {
            return true;
        }
    }
    return false;
}

RomBundler::BundleResult RomBundler::bundle(const FileRecord            &file,
                                            const Database::MatchResult &match,
                                            const GameMetadata          &metadata,
                                            const QString               &destinationDir,
                                            const BundleConfig          &config)
{
    BundleResult result;

    // ── 1. Guard: already bundled? ────────────────────────────────────────────
    const QString sourcePath = file.currentPath.isEmpty() ? file.archivePath : file.currentPath;

    if (file.isCompressed && !sourcePath.isEmpty() && QFile::exists(sourcePath)) {
        if (isAlreadyBundled(sourcePath)) {
            qInfo() << "  ↷ Already bundled (marker present):" << sourcePath;
            result.skippedAlreadyBundled = true;
            result.success = true;
            result.outputPath = sourcePath;
            return result;
        }
    }

    // ── 2. Resolve ROM file path (extract from archive if needed) ─────────────
    QDir destDir(destinationDir);
    if (!config.dryRun && !destDir.exists() && !destDir.mkpath(".")) {
        result.error = "Cannot create destination directory: " + destinationDir;
        return result;
    }

    const QString tempRoot = config.dryRun ? QDir::tempPath() : destDir.absolutePath();
    const QString tempBase = tempRoot + "/.remus_bundle_" +
                             QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir tempDir(tempBase);
    if (!tempDir.mkpath(".")) {
        result.error = "Cannot create temp directory: " + tempBase;
        return result;
    }

    // Cleanup helper — runs on all exit paths after this point
    auto cleanup = [&]() {
        tempDir.removeRecursively();
    };

    QString romInTemp;

    if (file.isCompressed && !sourcePath.isEmpty()) {
        // Extract the current archive container (or whole archive if unknown)
        ExtractionResult ex = m_extractor.extract(sourcePath, tempBase, false);
        if (!ex.success) {
            result.error = "Archive extraction failed: " + ex.error;
            cleanup();
            return result;
        }
        // Prefer the specific internal path if recorded
        if (!file.archiveInternalPath.isEmpty()) {
            romInTemp = tempBase + "/" + file.archiveInternalPath;
        } else if (!ex.extractedFiles.isEmpty()) {
            romInTemp = ex.extractedFiles.first();
        }
    } else {
        // Loose file — copy into temp dir so we own it
        const QString destRom = tempBase + "/" + QFileInfo(file.filename).fileName();
        if (!QFile::copy(file.currentPath, destRom)) {
            result.error = "Failed to copy ROM to temp dir";
            cleanup();
            return result;
        }
        romInTemp = destRom;
    }

    if (romInTemp.isEmpty() || !QFile::exists(romInTemp)) {
        result.error = "ROM not found in temp dir after extraction";
        cleanup();
        return result;
    }

    QString bundlePayloadPath = romInTemp;

    // ── 3. Optionally convert supported disc media to CHD ───────────────────
    if (config.convertDiscsToChd && isChdConvertiblePath(romInTemp)) {
        const QString ext = QFileInfo(romInTemp).suffix().toLower();
        const QString chdPath = tempBase + "/" + QFileInfo(romInTemp).completeBaseName() + ".chd";

        if (config.dryRun) {
            qInfo() << "  [DRY-RUN] Would convert disc media to CHD:" << chdPath;
            bundlePayloadPath = chdPath;
        } else {
            if (!file.isCompressed) {
                if (!stageReferencedDiscFiles(file.currentPath, tempBase, &result.error)) {
                    cleanup();
                    return result;
                }

                const QList<FileRecord> children = m_db.getFilesByParent(file.id);
                for (const FileRecord &child : children) {
                    if (child.currentPath.isEmpty()) {
                        continue;
                    }
                    if (!copyFileIntoDirectory(child.currentPath,
                                               tempBase,
                                               &result.error,
                                               QFileInfo(child.filename).fileName())) {
                        cleanup();
                        return result;
                    }
                }
            }

            CHDConverter converter;
            if (!converter.isChdmanAvailable()) {
                result.error = "chdman not found. Install MAME tools to bundle discs as CHD";
                cleanup();
                return result;
            }

            CHDConversionResult conversion;
            if (ext == QStringLiteral("cue")) {
                conversion = converter.convertCueToCHD(romInTemp, chdPath);
            } else if (ext == QStringLiteral("iso") || ext == QStringLiteral("img")) {
                conversion = converter.convertIsoToCHD(romInTemp, chdPath);
            } else if (ext == QStringLiteral("gdi")) {
                conversion = converter.convertGdiToCHD(romInTemp, chdPath);
            }

            if (!conversion.success || !QFile::exists(chdPath)) {
                result.error = conversion.error.isEmpty()
                    ? QStringLiteral("Disc-to-CHD conversion failed")
                    : conversion.error;
                cleanup();
                return result;
            }

            if (!clearDirectoryExcept(tempBase, chdPath)) {
                result.error = "Failed to clean temp directory after CHD conversion";
                cleanup();
                return result;
            }

            bundlePayloadPath = chdPath;
            qInfo() << "  ✓ Disc media converted to CHD:" << chdPath;
        }
    }

    // ── 4. Write .remus.md marker ─────────────────────────────────────────────
    const QString markerPath = tempBase + "/" + MARKER_FILENAME;
    {
        QFile markerFile(markerPath);
        if (!markerFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            result.error = "Cannot write marker file";
            cleanup();
            return result;
        }
        QTextStream out(&markerFile);
        out << generateMarkerContent(file, match, metadata);
    }

    // ── 5. Include pre-downloaded box art (optional) ─────────────────────────
    QStringList archiveEntries = {
        QDir(tempBase).relativeFilePath(bundlePayloadPath),
        QString::fromLatin1(MARKER_FILENAME)
    };

    if (config.includeBoxArt && !config.artworkPath.isEmpty() &&
        QFile::exists(config.artworkPath)) {
        // Copy into artwork/ subdirectory inside the temp tree
        const QString artDestDir = tempBase + "/" + ARTWORK_SUBDIR;
        QDir().mkpath(artDestDir);
        const QString artDest = artDestDir + "/" + BOXART_FILENAME;
        if (QFile::copy(config.artworkPath, artDest)) {
            archiveEntries << QString::fromLatin1(ARTWORK_SUBDIR) + "/" + BOXART_FILENAME;
            qInfo() << "  ✓ Box art included from:" << config.artworkPath;
        } else {
            qWarning() << "  ⚠ Could not copy box art:" << config.artworkPath;
        }
    }

    // ── 6. Determine output archive path ─────────────────────────────────────
    const QString ext = (config.outputFormat == ArchiveFormat::SevenZip) ? Constants::Files::SEVEN_Z : Constants::Files::ZIP;
    const QString baseName = QFileInfo(file.filename).completeBaseName();

    const QString outputArchive = destDir.absoluteFilePath(baseName + ext);

    // ── 7. Dry-run short-circuit ──────────────────────────────────────────────
    if (config.dryRun) {
        qInfo() << "  [DRY-RUN] Would create bundle:" << outputArchive;
        qInfo() << "  [DRY-RUN] Archive entries:" << archiveEntries;
        result.success = true;
        result.outputPath = outputArchive;
        cleanup();
        return result;
    }

    // ── 8. Pack into output archive ───────────────────────────────────────────
    CompressionResult cr = m_creator.compressDirectoryContents(tempBase, outputArchive,
                                                               config.outputFormat);
    cleanup();

    if (!cr.success) {
        result.error = "Compression failed: " + cr.error;
        return result;
    }

    // ── 9. Mark processed in database ────────────────────────────────────────
    m_db.markFileProcessed(file.id, Constants::Engines::ProcessingStatus::BUNDLED);
    m_db.updateFilePath(file.id, outputArchive);

    result.success    = true;
    result.outputPath = outputArchive;
    qInfo() << "  ✓ Bundle created:" << outputArchive
            << "(" << cr.filesCompressed << "files," << cr.compressedSize << "bytes)";

    emit progressMessage(QString("Bundled: %1").arg(outputArchive));
    return result;
}

// ─── bundleStaged ─────────────────────────────────────────────────────────────

RomBundler::BundleResult RomBundler::bundleStaged(
    const FileRecord              &patchedFile,
    const Database::MatchResult   &baseMatch,
    const GameMetadata            &metadata,
    const QString                 &destinationDir,
    const BundleConfig            &config)
{
    BundleResult result;

    // ── 1. Resolve ROM file path ──────────────────────────────────────────────
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

    // Copy the patched ROM into the temp directory
    const QString destRom = tempBase + "/" + QFileInfo(patchedFile.filename).fileName();
    if (!QFile::copy(sourcePath, destRom)) {
        result.error = "Failed to copy patched ROM to temp dir";
        cleanup();
        return result;
    }

    // ── 2. Write .remus.md marker ─────────────────────────────────────────────
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

    // ── 3. Include box art (optional) ─────────────────────────────────────────
    QStringList archiveEntries = {
        QDir(tempBase).relativeFilePath(destRom),
        QString::fromLatin1(MARKER_FILENAME)
    };

    if (config.includeBoxArt && !config.artworkPath.isEmpty()
        && QFile::exists(config.artworkPath)) {
        const QString artDestDir = tempBase + "/" + ARTWORK_SUBDIR;
        QDir().mkpath(artDestDir);
        const QString artDest = artDestDir + "/" + BOXART_FILENAME;
        if (QFile::copy(config.artworkPath, artDest)) {
            archiveEntries << QString::fromLatin1(ARTWORK_SUBDIR) + "/" + BOXART_FILENAME;
        }
    }

    // ── 4. Determine output archive path ──────────────────────────────────────
    const QString ext = (config.outputFormat == ArchiveFormat::SevenZip) ? Constants::Files::SEVEN_Z : Constants::Files::ZIP;
    const QString baseName = QFileInfo(patchedFile.filename).completeBaseName();
    const QString outputArchive = destDir.absoluteFilePath(baseName + ext);

    // ── 5. Dry-run short-circuit ──────────────────────────────────────────────
    if (config.dryRun) {
        qInfo() << "  [DRY-RUN] Would create bundle:" << outputArchive;
        result.success = true;
        result.outputPath = outputArchive;
        cleanup();
        return result;
    }

    // ── 6. Pack into output archive ───────────────────────────────────────────
    CompressionResult cr = m_creator.compressDirectoryContents(
        tempBase, outputArchive, config.outputFormat);
    cleanup();

    if (!cr.success) {
        result.error = "Compression failed: " + cr.error;
        return result;
    }

    // ── 7. Mark the patched file as processed (NOT the base) ──────────────────
    m_db.markFileProcessed(patchedFile.id, Constants::Engines::ProcessingStatus::BUNDLED);
    m_db.updateFilePath(patchedFile.id, outputArchive);

    result.success    = true;
    result.outputPath = outputArchive;
    qInfo() << "  ✓ Bundle created (staged):" << outputArchive;

    emit progressMessage(QString("Bundled (staged): %1").arg(outputArchive));
    return result;
}

// ─── private helpers ──────────────────────────────────────────────────────────

QString RomBundler::generateMarkerContent(const FileRecord            &file,
                                          const Database::MatchResult &match,
                                          const GameMetadata          &metadata) const
{
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QString out;
    QTextStream s(&out);

    // YAML front-matter — machine-parseable by future tool versions
    s << "---\n";
    s << "remus_processed: true\n";
    s << "processed_at: "    << now                                    << "\n";
    s << "file_id: "         << file.id                                << "\n";
    s << "match_method: "    << match.matchMethod                       << "\n";
    s << "confidence: "      << QString::number(match.confidence, 'f', 4) << "\n";
    s << "crc32: "           << file.crc32                             << "\n";
    s << "md5: "             << file.md5                               << "\n";
    s << "sha1: "            << file.sha1                              << "\n";
    s << "---\n\n";

    // Human-readable section
    s << "# " << (metadata.title.isEmpty() ? match.gameTitle : metadata.title) << "\n\n";
    s << "Processed by Remus on " << now << ".\n\n";

    s << "## Identification\n\n";
    s << "| Field | Value |\n";
    s << "|---|---|\n";
    s << "| CRC32 | `" << file.crc32 << "` |\n";
    s << "| MD5   | `" << file.md5   << "` |\n";
    s << "| SHA1  | `" << file.sha1  << "` |\n";
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
