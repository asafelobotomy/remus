#include "rom_bundler.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>
#include <QTextStream>

#include "constants/constants.h"
#include "constants/files.h"
#include "constants/systems.h"
#include "conversion_planner.h"
#include "cso_converter.h"
#include "logging_categories.h"
#include "rvz_converter.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug()    qCDebug(logCore)
#define qInfo()     qCInfo(logCore)
#define qWarning()  qCWarning(logCore)
#define qCritical() qCCritical(logCore)

namespace Remus {

namespace {

static constexpr const char *MARKER_FILENAME = ".remus.md";
static constexpr const char *ARTWORK_SUBDIR  = "artwork";
static constexpr const char *BOXART_FILENAME = "boxfront.jpg";

bool isDiscManifestPath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("cue") || suffix == QStringLiteral("gdi");
}

bool isChdConvertiblePath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("cue") || suffix == QStringLiteral("iso") || suffix == QStringLiteral("img") || suffix == QStringLiteral("gdi");
}

bool isRvzConvertiblePath(const QString &path)
{
    const QString extension = QStringLiteral(".") + QFileInfo(path).suffix().toLower();
    return Constants::Files::isRvzSourceExtension(extension);
}

QStringList collectArchiveEntries(const QString &rootDir)
{
    QStringList entries;
    QDir root(rootDir);
    QDirIterator it(rootDir, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        entries << root.relativeFilePath(it.filePath()).replace('\\', '/');
    }
    entries.sort();
    return entries;
}

QString dottedSuffix(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix();
    return suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix.toLower();
}

bool clearDirectoryExcept(const QString &dirPath, const QString &keepPath)
{
    QDir root(dirPath);
    const QFileInfoList entries = root.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
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

bool ensurePlaceholderFile(const QString &path, QString *error)
{
    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) *error = QStringLiteral("Failed to create placeholder directory: %1").arg(info.absolutePath());
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = QStringLiteral("Failed to create placeholder file: %1").arg(path);
        return false;
    }
    file.close();
    return true;
}

bool copyFileIntoDirectory(const QString &sourcePath,
                           const QString &destinationDir,
                           QString *error,
                           const QString &targetFileName = QString())
{
    if (sourcePath.isEmpty() || !QFile::exists(sourcePath)) {
        if (error) *error = QStringLiteral("Referenced disc file not found: %1").arg(sourcePath);
        return false;
    }

    const QString fileName = targetFileName.isEmpty() ? QFileInfo(sourcePath).fileName() : targetFileName;
    const QString destinationPath = QDir(destinationDir).filePath(fileName);
    if (QFile::exists(destinationPath)) {
        return true;
    }

    if (!QFile::copy(sourcePath, destinationPath)) {
        if (error) *error = QStringLiteral("Failed to stage disc file: %1").arg(fileName);
        return false;
    }
    return true;
}

QStringList getReferencedDiscFiles(const QString &manifestPath)
{
    const QString suffix = QFileInfo(manifestPath).suffix().toLower();
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QStringList referencedFiles;
    QTextStream input(&manifestFile);
    if (suffix == QStringLiteral("cue")) {
        static const QRegularExpression cueFilePattern(QStringLiteral("^\\s*FILE\\s+\"([^\"]+)\""), QRegularExpression::CaseInsensitiveOption);
        while (!input.atEnd()) {
            const QRegularExpressionMatch match = cueFilePattern.match(input.readLine());
            if (match.hasMatch()) {
                referencedFiles << match.captured(1);
            }
        }
    } else if (suffix == QStringLiteral("gdi")) {
        static const QRegularExpression gdiFilePattern(QStringLiteral("^\\s*\\d+\\s+\\d+\\s+\\d+\\s+\\d+\\s+(.+?)\\s+\\d+\\s*$"));
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

bool stageReferencedDiscFiles(const QString &manifestPath,
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

RomBundler::DiscOutputFormat resolveDiscOutputFormat(const FileRecord &file,
                                                     const Database::MatchResult &match,
                                                     const QString &payloadPath,
                                                     const RomBundler::BundleConfig &config)
{
    const int resolvedSystemId = match.systemId > 0 ? match.systemId : file.systemId;
    if (config.discOutputFormat == RomBundler::DiscOutputFormat::Original) {
        return RomBundler::DiscOutputFormat::Original;
    }
    if (config.discOutputFormat == RomBundler::DiscOutputFormat::Rvz) {
        return ((resolvedSystemId == Constants::Systems::ID_GAMECUBE || resolvedSystemId == Constants::Systems::ID_WII)
                && isRvzConvertiblePath(payloadPath))
            ? RomBundler::DiscOutputFormat::Rvz
            : RomBundler::DiscOutputFormat::Original;
    }
    // CHD requested: delegate to ConversionPlanner for system-aware routing.
    // The planner handles GameCube/Wii → RVZ, PSP → CSO, and disc images → CHD.
    // Tool availability is probed once per file; checks are fast (binary existence).
    ConversionPlanner::Request req;
    req.systemId = resolvedSystemId;
    req.extension = QStringLiteral(".") + QFileInfo(payloadPath).suffix().toLower();
    req.intent = ConversionPlanner::PlanningIntent::AutoProcess;
    req.availableTools.chdmanAvailable     = CHDConverter().isChdmanAvailable();
    req.availableTools.dolphinToolAvailable = RVZConverter().isDolphinToolAvailable();
    req.availableTools.maxcsoAvailable     = CSOConverter().isMaxcsoAvailable();
    const ConversionPlanner::Plan plan = ConversionPlanner::plan(req);
    switch (plan.action) {
    case ConversionPlanner::PlannedAction::ConvertToRvz:
        return isRvzConvertiblePath(payloadPath) ? RomBundler::DiscOutputFormat::Rvz
                                                 : RomBundler::DiscOutputFormat::Original;
    case ConversionPlanner::PlannedAction::ConvertToCso:
        return RomBundler::DiscOutputFormat::Cso;
    case ConversionPlanner::PlannedAction::ConvertToChd:
        return isChdConvertiblePath(payloadPath) ? RomBundler::DiscOutputFormat::Chd
                                                 : RomBundler::DiscOutputFormat::Original;
    default:
        return RomBundler::DiscOutputFormat::Original;
    }
}

bool stageBundlePayloadAtRoot(const QString &payloadPath,
                              const QString &bundleRoot,
                              const QList<FileRecord> &childFiles,
                              QString *stagedPayloadPath,
                              QString *error)
{
    const QString payloadFileName = QFileInfo(payloadPath).fileName();
    if (!copyFileIntoDirectory(payloadPath, bundleRoot, error, payloadFileName)) {
        return false;
    }
    if (stagedPayloadPath) {
        *stagedPayloadPath = QDir(bundleRoot).filePath(payloadFileName);
    }
    if (isDiscManifestPath(payloadPath) && !stageReferencedDiscFiles(payloadPath, bundleRoot, error)) {
        return false;
    }
    for (const FileRecord &child : childFiles) {
        if (child.currentPath.isEmpty() || !QFile::exists(child.currentPath)) {
            continue;
        }
        if (!copyFileIntoDirectory(child.currentPath, bundleRoot, error, QFileInfo(child.filename).fileName())) {
            return false;
        }
    }
    return true;
}

QString findCompanionManifestPath(const QString &sourceRoot, const QList<FileRecord> &childFiles)
{
    for (const FileRecord &child : childFiles) {
        if (!isDiscManifestPath(child.filename)) {
            continue;
        }
        QString candidatePath;
        if (!child.archiveInternalPath.isEmpty()) {
            const QString normalized = ArchiveExtractor::normalizeArchiveMemberPath(child.archiveInternalPath);
            if (!normalized.isEmpty()) {
                candidatePath = QDir(sourceRoot).filePath(normalized);
            }
        }
        if (candidatePath.isEmpty() && !child.filename.isEmpty()) {
            candidatePath = QDir(sourceRoot).filePath(QFileInfo(child.filename).fileName());
        }
        if (!candidatePath.isEmpty() && QFile::exists(candidatePath)) {
            return candidatePath;
        }
    }
    return {};
}

} // namespace

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
                result.error = "chdman not found. Install MAME tools to bundle discs as CHD";
                cleanup();
                return result;
            }

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