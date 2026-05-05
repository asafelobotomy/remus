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

namespace {

/// Strip trailing No-Intro / Redump parenthetical tags from a raw game title
/// and return the clean title along with the first region tag found.
/// Mirrors the stripping logic in TemplateEngine::buildVariableMap().
QPair<QString, QString> stripNoIntroTags(const QString &rawTitle)
{
    static const QStringList kRegionTokens = {
        QStringLiteral("USA"), QStringLiteral("Europe"), QStringLiteral("Japan"),
        QStringLiteral("World"), QStringLiteral("Australia"), QStringLiteral("Brazil"),
        QStringLiteral("Canada"), QStringLiteral("China"), QStringLiteral("France"),
        QStringLiteral("Germany"), QStringLiteral("Italy"), QStringLiteral("Korea"),
        QStringLiteral("Netherlands"), QStringLiteral("Russia"), QStringLiteral("Spain"),
        QStringLiteral("Sweden"), QStringLiteral("UK")
    };
    static const QRegularExpression kTagPat(QStringLiteral(R"(\s*\(([^)]+)\)\s*$)"));
    static const QRegularExpression kLangPat(QStringLiteral(R"(^[A-Z][a-z](?:,[A-Z][a-z])*$)"));
    static const QRegularExpression kVerPat(
        QStringLiteral(R"(^(?:Rev\s+\w+|v\d+\.\d+.*)$)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kStatusPat(
        QStringLiteral(R"(^(?:Beta|Proto|Sample|Demo|Kiosk|Debug|Unl|Aftermarket|Virtual Console)(?:\s+\w+)?$)"),
        QRegularExpression::CaseInsensitiveOption);

    QString clean = rawTitle;
    QString region;
    bool stripping = true;
    while (stripping) {
        const QRegularExpressionMatch m = kTagPat.match(clean);
        if (!m.hasMatch()) break;
        const QString tag = m.captured(1).trimmed();
        const QStringList parts = tag.split(QLatin1Char(','));
        bool allRegion = !parts.isEmpty();
        for (const QString &p : parts) {
            if (!kRegionTokens.contains(p.trimmed(), Qt::CaseInsensitive)) { allRegion = false; break; }
        }
        const bool isKnown = allRegion
            || kLangPat.match(tag).hasMatch()
            || kVerPat.match(tag).hasMatch()
            || kStatusPat.match(tag).hasMatch();
        if (isKnown) {
            if (allRegion && region.isEmpty())
                region = tag;
            clean = clean.left(m.capturedStart()).trimmed();
        } else {
            stripping = false;
        }
    }
    return {clean, region};
}

} // anonymous namespace

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

    static const QRegularExpression kUnsafeFilenameChars(QStringLiteral("[<>:\"/\\\\|?*]"));

    // Resolve the raw title from metadata (already stripped of tags by the template
    // engine when called via TemplateEngine, but the bundler receives the raw DAT title).
    const QString rawTitle  = metadata.title.isEmpty() ? match.gameTitle : metadata.title;
    const QPair<QString, QString> stripped = stripNoIntroTags(rawTitle);
    const QString cleanTitle = stripped.first;
    // Region priority: explicit match region > extracted from title tags > metadata region
    const QString region = !match.region.isEmpty()   ? match.region
                         : !stripped.second.isEmpty() ? stripped.second
                         : metadata.region;

    QString baseName;
    if (!config.namingTemplate.isEmpty()) {
        const QString system    = m_db.getSystemDisplayName(match.systemId);
        const QString publisher = metadata.publisher.isEmpty() ? match.publisher : metadata.publisher;
        const int year = match.releaseYear > 0
                             ? match.releaseYear
                             : (metadata.releaseDate.length() >= 4
                                    ? metadata.releaseDate.left(4).toInt()
                                    : 0);
        baseName = config.namingTemplate;
        // {title} always receives the clean title (all No-Intro tags stripped).
        // {region} receives only the region, so adding both never produces duplicates.
        baseName.replace(QStringLiteral("{title}"),     cleanTitle);
        baseName.replace(QStringLiteral("{region}"),    region);
        baseName.replace(QStringLiteral("{year}"),      year > 0 ? QString::number(year) : QString());
        baseName.replace(QStringLiteral("{system}"),    system);
        baseName.replace(QStringLiteral("{publisher}"), publisher);
        baseName.replace(kUnsafeFilenameChars, QStringLiteral("_"));
        // Clean up artefacts from empty substitutions (e.g. "(_)", "[]", trailing spaces)
        baseName.replace(QStringLiteral("(_)"), QString());
        baseName.replace(QStringLiteral("[]"),  QString());
        baseName = baseName.simplified().trimmed();
    }
    if (baseName.isEmpty() && !cleanTitle.isEmpty()) {
        // Fallback (no template): clean title + region
        baseName = cleanTitle;
        if (!region.isEmpty())
            baseName += QStringLiteral(" (") + region + QStringLiteral(")");
        baseName.replace(kUnsafeFilenameChars, QStringLiteral("_"));
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