#include "rom_bundler.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>

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
    return info.contents.contains(MARKER_FILENAME);
}

RomBundler::BundleResult RomBundler::bundle(const FileRecord            &file,
                                            const Database::MatchResult &match,
                                            const GameMetadata          &metadata,
                                            const QString               &destinationDir,
                                            const BundleConfig          &config)
{
    BundleResult result;

    // ── 1. Guard: already bundled? ────────────────────────────────────────────
    const QString sourcePath = file.isCompressed ? file.archivePath : file.currentPath;

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
    const QString tempBase = QDir::tempPath() + "/remus_bundle_" +
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

    if (file.isCompressed && !file.archivePath.isEmpty()) {
        // Extract the single ROM entry (or whole archive if unknown)
        ExtractionResult ex = m_extractor.extract(file.archivePath, tempBase, false);
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

    // ── 3. Write .remus.md marker ─────────────────────────────────────────────
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

    // ── 4. Include pre-downloaded box art (optional) ─────────────────────────
    QStringList filesToPack = { romInTemp, markerPath };

    if (config.includeBoxArt && !config.artworkPath.isEmpty() &&
        QFile::exists(config.artworkPath)) {
        // Copy into artwork/ subdirectory inside the temp tree
        const QString artDestDir = tempBase + "/" + ARTWORK_SUBDIR;
        QDir().mkpath(artDestDir);
        const QString artDest = artDestDir + "/" + BOXART_FILENAME;
        if (QFile::copy(config.artworkPath, artDest)) {
            filesToPack << artDest;
            qInfo() << "  ✓ Box art included from:" << config.artworkPath;
        } else {
            qWarning() << "  ⚠ Could not copy box art:" << config.artworkPath;
        }
    }

    // ── 5. Determine output archive path ─────────────────────────────────────
    const QString ext = (config.outputFormat == ArchiveFormat::SevenZip) ? ".7z" : ".zip";
    const QString baseName = QFileInfo(file.filename).completeBaseName();

    QDir destDir(destinationDir);
    if (!config.dryRun && !destDir.exists())
        destDir.mkpath(".");

    const QString outputArchive = destinationDir + "/" + baseName + ext;

    // ── 6. Dry-run short-circuit ──────────────────────────────────────────────
    if (config.dryRun) {
        qInfo() << "  [DRY-RUN] Would create bundle:" << outputArchive;
        qInfo() << "  [DRY-RUN] Contents:" << filesToPack;
        result.success = true;
        result.outputPath = outputArchive;
        cleanup();
        return result;
    }

    // ── 7. Pack into output archive ───────────────────────────────────────────
    CompressionResult cr = m_creator.compress(filesToPack, outputArchive, config.outputFormat);
    cleanup();

    if (!cr.success) {
        result.error = "Compression failed: " + cr.error;
        return result;
    }

    // ── 8. Mark processed in database ────────────────────────────────────────
    m_db.markFileProcessed(file.id, "bundled");
    m_db.updateFilePath(file.id, outputArchive);

    result.success    = true;
    result.outputPath = outputArchive;
    qInfo() << "  ✓ Bundle created:" << outputArchive
            << "(" << cr.filesCompressed << "files," << cr.compressedSize << "bytes)";

    emit progressMessage(QString("Bundled: %1").arg(outputArchive));
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
    s << "| Confidence | " << QString::number(match.confidence * 100.0f, 'f', 1) << "% |\n\n";

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
