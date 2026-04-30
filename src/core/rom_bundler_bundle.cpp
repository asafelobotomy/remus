#include "rom_bundler.h"
#include "rom_bundler_bundle_helpers.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QTextStream>

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
using namespace BundleHelpers;


RomBundler::BundleResult RomBundler::bundle(const FileRecord &file,
                                            const Database::MatchResult &match,
                                            const GameMetadata &metadata,
                                            const QString &destinationDir,
                                            const BundleConfig &config)
{
    BundleResult result;
    const QString sourcePath = file.currentPath.isEmpty() ? file.archivePath : file.currentPath;
    const QString originalInputPath = file.originalPath.isEmpty() ? QString() : QFileInfo(file.originalPath).absoluteFilePath();
    const QString currentInputPath = sourcePath.isEmpty() ? QString() : QFileInfo(sourcePath).absoluteFilePath();
    const bool sourcePathStillPointsAtOriginalInput = !originalInputPath.isEmpty() && currentInputPath == originalInputPath;

    if (file.isCompressed && sourcePathStillPointsAtOriginalInput && !sourcePath.isEmpty() && QFile::exists(sourcePath) && isAlreadyBundled(sourcePath)) {
        qInfo() << "  ↷ Already bundled (marker present):" << sourcePath;
        result.skippedAlreadyBundled = true;
        result.success = true;
        result.outputPath = sourcePath;
        return result;
    }

    QDir destDir(destinationDir);
    if (!config.dryRun && !destDir.exists() && !destDir.mkpath(".")) {
        result.error = "Cannot create destination directory: " + destinationDir;
        return result;
    }

    const QString tempRoot = config.dryRun ? QDir::tempPath() : destDir.absolutePath();
    const QString tempBase = tempRoot + "/.remus_bundle_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    const QString sourceRoot = tempBase + "/source";
    QDir tempDir(tempBase);
    if (!tempDir.mkpath(".") || !QDir().mkpath(sourceRoot)) {
        result.error = "Cannot create temp directory: " + tempBase;
        return result;
    }

    auto cleanup = [&]() { tempDir.removeRecursively(); };

    QString romInTemp;
    if (file.isCompressed && !sourcePath.isEmpty()) {
        const ExtractionResult extraction = m_extractor.extract(sourcePath, sourceRoot, false);
        if (!extraction.success) {
            result.error = "Archive extraction failed: " + extraction.error;
            cleanup();
            return result;
        }
        if (!file.archiveInternalPath.isEmpty()) {
            const QString normalizedInternalPath = ArchiveExtractor::normalizeArchiveMemberPath(file.archiveInternalPath);
            if (normalizedInternalPath.isEmpty()) {
                result.error = "Archive member path is unsafe";
                cleanup();
                return result;
            }
            romInTemp = QDir(sourceRoot).filePath(normalizedInternalPath);
        } else if (!extraction.extractedFiles.isEmpty()) {
            romInTemp = extraction.extractedFiles.first();
        }
    } else {
        const QString destRom = sourceRoot + "/" + QFileInfo(file.filename).fileName();
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

    QList<FileRecord> childFiles = m_db.getFilesByParent(file.id);
    if (!file.isCompressed) {
        if (isDiscManifestPath(romInTemp) && !stageReferencedDiscFiles(file.currentPath, sourceRoot, &result.error)) {
            cleanup();
            return result;
        }
        for (const FileRecord &child : childFiles) {
            if (child.currentPath.isEmpty()) {
                continue;
            }
            if (!copyFileIntoDirectory(child.currentPath, sourceRoot, &result.error, QFileInfo(child.filename).fileName())) {
                cleanup();
                return result;
            }
        }
    }

    QString bundlePayloadPath = romInTemp;
    QString discSourcePath = romInTemp;
    const QString companionManifestPath = findCompanionManifestPath(sourceRoot, childFiles);
    if (!companionManifestPath.isEmpty()) {
        discSourcePath = companionManifestPath;
    }

    const DiscOutputFormat discOutputFormat = resolveDiscOutputFormat(file, match, discSourcePath, config);
    if (discOutputFormat != DiscOutputFormat::Original) {
        const QString ext = QFileInfo(discSourcePath).suffix().toLower();
        const QString targetExtension = discOutputFormat == DiscOutputFormat::Rvz ? QStringLiteral("rvz")
                                       : discOutputFormat == DiscOutputFormat::Cso ? QStringLiteral("cso")
                                       :                                              QStringLiteral("chd");
        const QString convertedPath = sourceRoot + "/" + QFileInfo(discSourcePath).completeBaseName() + "." + targetExtension;

        if (config.dryRun) {
            if (!ensurePlaceholderFile(convertedPath, &result.error)) {
                cleanup();
                return result;
            }
            qInfo() << "  [DRY-RUN] Would convert disc media to"
                    << (discOutputFormat == DiscOutputFormat::Rvz  ? "RVZ:"
                      : discOutputFormat == DiscOutputFormat::Cso  ? "CSO:"
                      :                                              "CHD:")
                    << convertedPath;
            bundlePayloadPath = convertedPath;
        } else if (discOutputFormat == DiscOutputFormat::Chd) {
            CHDConverter converter;
            if (!converter.isChdmanAvailable()) {
                qWarning() << "  ⚠ chdman not found; bundling disc image as-is:" << QFileInfo(discSourcePath).fileName();
                // Soft failure: original file stays as bundlePayloadPath
            } else {
                ConversionResult conversion;
                if (ext == QStringLiteral("cue")) {
                    conversion = converter.convertCueToCHD(discSourcePath, convertedPath);
                } else if (ext == QStringLiteral("iso") || ext == QStringLiteral("img")) {
                    conversion = converter.convertIsoToCHD(discSourcePath, convertedPath);
                } else if (ext == QStringLiteral("gdi")) {
                    conversion = converter.convertGdiToCHD(discSourcePath, convertedPath);
                }

                if (!conversion.success || !QFile::exists(convertedPath)) {
                    result.error = conversion.error.isEmpty() ? QStringLiteral("Disc-to-CHD conversion failed") : conversion.error;
                    cleanup();
                    return result;
                }
                qInfo() << "  ✓ Disc media converted to CHD:" << convertedPath;
                bundlePayloadPath = convertedPath;
            }
        } else if (discOutputFormat == DiscOutputFormat::Cso) {
            CSOConverter converter;
            if (!converter.isMaxcsoAvailable()) {
                qWarning() << "  ⚠ maxcso not found; bundling PSP image as-is:" << QFileInfo(discSourcePath).fileName();
                // Soft failure: original file stays as bundlePayloadPath
            } else {
                const ConversionResult conversion = converter.convertIsoToCSO(discSourcePath, convertedPath);
                if (!conversion.success || !QFile::exists(convertedPath)) {
                    result.error = conversion.error.isEmpty() ? QStringLiteral("Disc-to-CSO conversion failed") : conversion.error;
                    cleanup();
                    return result;
                }
                qInfo() << "  ✓ Disc media converted to CSO:" << convertedPath;
                bundlePayloadPath = convertedPath;
            }
        } else {
            RVZConverter converter;
            if (!converter.isDolphinToolAvailable()) {
                if (config.discOutputFormat == DiscOutputFormat::Rvz) {
                    result.error = "dolphin-tool not found. Install Dolphin tools to bundle GameCube/Wii discs as RVZ";
                    cleanup();
                    return result;
                }
                qWarning() << "  ⚠ dolphin-tool not found; bundling original disc media for GameCube/Wii:" << QFileInfo(discSourcePath).fileName();
            } else {
                const ConversionResult conversion = converter.convertIsoToRVZ(discSourcePath, convertedPath);
                if (!conversion.success || !QFile::exists(convertedPath)) {
                    result.error = conversion.error.isEmpty() ? QStringLiteral("Disc-to-RVZ conversion failed") : conversion.error;
                    cleanup();
                    return result;
                }
                qInfo() << "  ✓ Disc media converted to RVZ:" << convertedPath;
                bundlePayloadPath = convertedPath;
            }
        }

        if (bundlePayloadPath == convertedPath) {
            if (!clearDirectoryExcept(sourceRoot, convertedPath)) {
                result.error = "Failed to clean temp directory after disc conversion";
                cleanup();
                return result;
            }
            childFiles.clear();
        }
    }

    QString stagedPayloadPath;
    if (!stageBundlePayloadAtRoot(bundlePayloadPath, tempBase, childFiles, &stagedPayloadPath, &result.error)) {
        cleanup();
        return result;
    }
    if (QDir(sourceRoot).exists() && !QDir(sourceRoot).removeRecursively()) {
        result.error = "Failed to remove temporary source directory";
        cleanup();
        return result;
    }

    bundlePayloadPath = stagedPayloadPath;
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

    if (config.includeBoxArt && !config.artworkPath.isEmpty() && QFile::exists(config.artworkPath)) {
        const QString artDestDir = tempBase + "/" + ARTWORK_SUBDIR;
        QDir().mkpath(artDestDir);
        const QString artDest = artDestDir + "/" + BOXART_FILENAME;
        if (QFile::copy(config.artworkPath, artDest)) {
            qInfo() << "  ✓ Box art included from:" << config.artworkPath;
        } else {
            qWarning() << "  ⚠ Could not copy box art:" << config.artworkPath;
        }
    }

    result.archiveEntries = collectArchiveEntries(tempBase);
    const QString ext = (config.outputFormat == ArchiveFormat::SevenZip) ? Constants::Files::SEVEN_Z : Constants::Files::ZIP;

    QString baseName;
    if (!metadata.title.isEmpty()) {
        baseName = metadata.title;
        if (!metadata.region.isEmpty() && !baseName.contains(QStringLiteral("(")) && !baseName.contains(metadata.region)) {
            baseName += QStringLiteral(" (") + metadata.region + QStringLiteral(")");
        }
        static const QRegularExpression unsafeChars(QStringLiteral("[<>:\"/\\\\|?*]"));
        baseName.replace(unsafeChars, QStringLiteral("_"));
        baseName = baseName.trimmed();
    }
    if (baseName.isEmpty()) {
        baseName = QFileInfo(file.filename).completeBaseName();
    }

    const QString outputArchive = destDir.absoluteFilePath(baseName + ext);
    if (config.dryRun) {
        qInfo() << "  [DRY-RUN] Would create bundle:" << outputArchive;
        qInfo() << "  [DRY-RUN] Archive entries:" << result.archiveEntries;
        result.success = true;
        result.outputPath = outputArchive;
        cleanup();
        return result;
    }

    const QString bundledInternalPath = QDir(tempBase).relativeFilePath(bundlePayloadPath).replace('\\', '/');
    const QFileInfo bundledPayloadInfo(bundlePayloadPath);
    const QString bundledFilename = bundledPayloadInfo.fileName();
    const QString bundledExtension = dottedSuffix(bundlePayloadPath);
    const qint64 bundledFileSize = bundledPayloadInfo.size();
    const bool preserveExistingHashes =
        bundledFilename.compare(QFileInfo(romInTemp).fileName(), Qt::CaseInsensitive) == 0;
    const CompressionResult compression = m_creator.compressDirectoryContents(tempBase, outputArchive, config.outputFormat);
    cleanup();
    if (!compression.success) {
        result.error = "Compression failed: " + compression.error;
        return result;
    }

    m_db.markFileProcessed(file.id, Constants::Engines::ProcessingStatus::BUNDLED);
    FileRecord bundledRecord = file.id > 0 ? m_db.getFileById(file.id) : file;
    if (bundledRecord.id <= 0) {
        bundledRecord = file;
    }
    bundledRecord.currentPath = outputArchive;
    bundledRecord.isCompressed = true;
    bundledRecord.archivePath = outputArchive;
    bundledRecord.archiveInternalPath = bundledInternalPath;
    bundledRecord.filename = bundledFilename;
    bundledRecord.extension = bundledExtension;
    bundledRecord.fileSize = bundledFileSize;
    if (!preserveExistingHashes) {
        bundledRecord.crc32.clear();
        bundledRecord.md5.clear();
        bundledRecord.sha1.clear();
        bundledRecord.hashCalculated = false;
    }
    m_db.updateFileStorageState(bundledRecord);

    result.success = true;
    result.outputPath = outputArchive;
    qInfo() << "  ✓ Bundle created:" << outputArchive << "(" << compression.filesCompressed << "files," << compression.compressedSize << "bytes)";
    emit progressMessage(QString("Bundled: %1").arg(outputArchive));
    return result;
}

} // namespace Remus