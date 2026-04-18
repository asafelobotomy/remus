#include "cli_helpers.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include "../core/archive_extractor.h"
#include "../core/constants/files.h"
#include "../core/space_calculator.h"
#include "../core/system_resolver.h"
#include "../metadata/filename_normalizer.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

/**
 * @brief Select the best hash for matching based on system's preferred algorithm.
 *
 * Disc-based systems (PlayStation, Saturn, etc.) prefer MD5/SHA1.
 * Cartridge-based systems (NES, SNES, GBA, etc.) prefer CRC32.
 */
QString selectBestHash(const FileRecord &file)
{
    return selectBestMatchHash(file);
}

static bool isArchivePath(const QString &path)
{
    const QString lower = path.toLower();
    for (const QString &extension : Files::ARCHIVE_EXTENSIONS) {
        if (lower.endsWith(extension)) {
            return true;
        }
    }
    return false;
}

static QStringList referencedDiscFiles(const QString &manifestPath)
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

static QString resolveHashSourcePath(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix != QStringLiteral("cue") && suffix != QStringLiteral("gdi")) {
        return path;
    }

    const QFileInfo manifestInfo(path);
    const QDir manifestDir = manifestInfo.dir();
    const QStringList referencedFiles = referencedDiscFiles(path);

    QString bestPath;
    qint64 bestSize = -1;
    for (const QString &relativePath : referencedFiles) {
        const QString candidatePath = manifestDir.filePath(relativePath);
        const QFileInfo candidateInfo(candidatePath);
        if (!candidateInfo.exists() || !candidateInfo.isFile()) {
            continue;
        }

        if (candidateInfo.size() > bestSize) {
            bestPath = candidateInfo.absoluteFilePath();
            bestSize = candidateInfo.size();
        }
    }

    return bestPath.isEmpty() ? path : bestPath;
}

static HashResult hashResolvedPath(const QString &path, Hasher &hasher)
{
    const QString extension = QStringLiteral(".") + QFileInfo(path).suffix().toLower();
    const int headerSize = Hasher::detectHeaderSize(path, extension);
    return hasher.calculateHashes(path, headerSize > 0, headerSize);
}

HashResult hashFileRecord(const FileRecord &file, Hasher &hasher)
{
    const QString archivePath = file.archivePath.isEmpty() ? file.currentPath : file.archivePath;
    const bool treatAsArchive = file.isCompressed || isArchivePath(archivePath);

    if (!treatAsArchive) {
        return hashResolvedPath(resolveHashSourcePath(file.currentPath), hasher);
    }

    HashResult result;
    if (!QFileInfo::exists(archivePath)) {
        result.error = "Archive file not found";
        return result;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        result.error = "Failed to create temporary directory";
        return result;
    }

    ArchiveExtractor extractor;
    const QString internalPath = file.archiveInternalPath.isEmpty() ? file.filename : file.archiveInternalPath;
    const bool isDiscManifest = file.extension.compare(QStringLiteral(".cue"), Qt::CaseInsensitive) == 0 ||
        file.extension.compare(QStringLiteral(".gdi"), Qt::CaseInsensitive) == 0;

    ExtractionResult extraction;
    if (!isDiscManifest) {
        extraction = extractor.extractFile(archivePath, internalPath, tempDir.path());
    }
    if (!extraction.success || extraction.extractedFiles.isEmpty()) {
        extraction = extractor.extract(archivePath, tempDir.path(), false);
        if (!extraction.success || extraction.extractedFiles.isEmpty()) {
            result.error = extraction.error.isEmpty()
                ? QString("Failed to extract %1 from archive").arg(internalPath)
                : extraction.error;
            return result;
        }

        if (isDiscManifest) {
            QString extractedManifestPath;
            if (!file.archiveInternalPath.isEmpty()) {
                const QString normalized = ArchiveExtractor::normalizeArchiveMemberPath(file.archiveInternalPath);
                if (!normalized.isEmpty()) {
                    extractedManifestPath = QDir(tempDir.path()).filePath(normalized);
                }
            }
            if (extractedManifestPath.isEmpty() || !QFileInfo::exists(extractedManifestPath)) {
                for (const QString &path : extraction.extractedFiles) {
                    if (QFileInfo(path).suffix().compare(QFileInfo(file.filename).suffix(), Qt::CaseInsensitive) == 0) {
                        extractedManifestPath = path;
                        break;
                    }
                }
            }
            if (!extractedManifestPath.isEmpty() && QFileInfo::exists(extractedManifestPath)) {
                return hashResolvedPath(resolveHashSourcePath(extractedManifestPath), hasher);
            }
        }

        QString picked;
        for (const QString &path : extraction.extractedFiles) {
            if (path.endsWith(file.extension, Qt::CaseInsensitive)) { picked = path; break; }
        }
        if (picked.isEmpty()) picked = extraction.extractedFiles.first();
        return hashResolvedPath(resolveHashSourcePath(picked), hasher);
    }

    const QString extractedPath = extraction.extractedFiles.first();
    return hashResolvedPath(resolveHashSourcePath(extractedPath), hasher);
}

QString findDataSubdir(const QString &subdir)
;

QString resolveCliOptionValue(const QCommandLineParser &parser,
                              const QString &optionName,
                              const QString &presetValue)
{
    if (parser.isSet(optionName)) {
        return parser.value(optionName).trimmed();
    }

    if (!presetValue.trimmed().isEmpty()) {
        return presetValue.trimmed();
    }

    return parser.value(optionName).trimmed();
}

QList<FileRecord> getHashedFiles(Database &db)
{
    return getHashedFiles(db, {});
}

QList<FileRecord> getHashedFiles(Database &db, const QSet<int> &fileScopeIds)
{
    const QList<FileRecord> files = db.getExistingFiles();
    QList<FileRecord> filtered;
    for (const FileRecord &f : files) {
        if (!fileMatchesProcessScope(f, fileScopeIds)) {
            continue;
        }
        if (f.hashCalculated && (!f.crc32.isEmpty() || !f.md5.isEmpty() || !f.sha1.isEmpty()))
            filtered.append(f);
    }
    return filtered;
}

bool fileMatchesProcessScope(const FileRecord &file, const QSet<int> &fileScopeIds)
{
    return fileScopeIds.isEmpty() || fileScopeIds.contains(file.id);
}

int resolveMatchedSystemId(const FileRecord &file,
                           const Database::MatchResult *match)
{
    if (match && match->systemId > 0) {
        return match->systemId;
    }

    return file.systemId;
}

bool fileMatchesSystemFilter(const FileRecord &file,
                             int systemId,
                             const Database::MatchResult *match)
{
    if (systemId < 0) {
        return true;
    }

    return resolveMatchedSystemId(file, match) == systemId;
}

QString getMatchingDisplayName(const FileRecord &file)
{
    return Remus::deriveMatchingDisplayName(file);
}

QString getMatchingSystemName(const FileRecord &file)
{
    if (file.systemId <= 0) {
        return QString();
    }

    const QString systemName = SystemResolver::internalName(file.systemId);
    return systemName == QStringLiteral("Unknown") ? QString() : systemName;
}

QString getProviderLookupSystemName(const FileRecord &file,
                                    const Database::MatchResult *match)
{
    const int systemId = resolveMatchedSystemId(file, match);
    if (systemId <= 0) {
        return QString();
    }

    const QString systemName = SystemResolver::internalName(systemId);
    return systemName == QStringLiteral("Unknown") ? QString() : systemName;
}

int persistMetadata(Database &db, const FileRecord &file, const GameMetadata &metadata)
{
    int systemId = db.getSystemId(metadata.system);
    if (systemId == 0) systemId = file.systemId;

    const QString region = metadata.region.isEmpty()
        ? Metadata::FilenameNormalizer::extractRegion(file.filename)
        : metadata.region;
    const QString genres = metadata.genres.join(", ");
    const QString players = metadata.players > 0 ? QString::number(metadata.players) : QString();
    int gameId = db.insertGame(metadata.title, systemId, region, metadata.publisher,
                               metadata.developer, metadata.releaseDate, metadata.description,
                               genres, players, metadata.rating);
    if (gameId == 0) return 0;

    const int confidence = metadata.matchScore > 0 ? static_cast<int>(metadata.matchScore * 100) : 0;
    const QString method = metadata.matchMethod.isEmpty() ? QStringLiteral("auto") : metadata.matchMethod;
    db.insertMatch(file.id, gameId, confidence, method);
    return gameId;
}

void printFileInfo(const FileRecord &file)
{
    qInfo() << "File ID:" << file.id;
    qInfo() << "Library ID:" << file.libraryId;
    if (file.isCompressed) {
        qInfo() << "Container Path:" << file.currentPath;
        qInfo() << "Archive Path:" << (file.archivePath.isEmpty() ? file.currentPath : file.archivePath);
        qInfo() << "Archive Entry:" << (file.archiveInternalPath.isEmpty() ? file.filename : file.archiveInternalPath);
        qInfo() << "Container Filename:" << QFileInfo(file.currentPath).fileName();
        qInfo() << "Entry Filename:" << file.filename;
        qInfo() << "Entry Extension:" << file.extension;
    } else {
        qInfo() << "Path:" << file.currentPath;
        qInfo() << "Filename:" << file.filename;
        qInfo() << "Extension:" << file.extension;
    }
    qInfo() << "Original Path:" << file.originalPath;
    qInfo() << "Size:" << file.fileSize;
    qInfo() << "System ID:" << file.systemId;
    qInfo() << "Hash calculated:" << file.hashCalculated;
    if (file.hashCalculated) {
        qInfo() << "CRC32:" << file.crc32;
        qInfo() << "MD5:" << file.md5;
        qInfo() << "SHA1:" << file.sha1;
    }
    qInfo() << "Primary:" << file.isPrimary;
    qInfo() << "Parent ID:" << file.parentFileId;
    qInfo() << "File Type:" << file.fileType;
    qInfo() << "Patched:" << file.isPatched;
    qInfo() << "Patch Name:" << file.patchName;
    qInfo() << "Processed:" << file.isProcessed << "Status:" << file.processingStatus;
}

QString buildOutputPath(const QString &inputPath, const QString &outputDir, const QString &targetExt)
{
    QFileInfo info(inputPath);
    const QString filename = info.completeBaseName() + "." + targetExt;
    if (outputDir.isEmpty()) {
        return QDir(info.absolutePath()).filePath(filename);
    }
    QDir().mkpath(outputDir);
    return QDir(outputDir).filePath(filename);
}

bool printConversionResult(const ConversionResult &result, const QString &formatName)
{
    if (result.success) {
        qInfo() << "✓ Conversion successful!";
        qInfo().noquote() << "  Original size:" << SpaceCalculator::formatBytes(result.inputSize);
        qInfo().noquote() << "  " + formatName + " size:" << SpaceCalculator::formatBytes(result.outputSize);
        qInfo().noquote() << "  Saved:" << SpaceCalculator::formatBytes(result.inputSize - result.outputSize);
        qInfo() << "  Compression:"
                << QString::number((1.0 - result.compressionRatio) * 100, 'f', 1) << "%";
        return true;
    }
    qCritical() << "✗ Conversion failed:" << result.error;
    return false;
}
