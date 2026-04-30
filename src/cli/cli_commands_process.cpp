#include "cli_commands.h"
#include "cli_helpers.h"

#include <algorithm>

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include "../core/archive_extractor.h"
#include "../core/chd_converter.h"
#include "../core/cso_converter.h"
#include "../core/disc_magic_detector.h"
#include "../core/hasher.h"
#include "../core/rvz_converter.h"
#include "../core/scanner.h"

using namespace Remus;

namespace {

QString scanResultIdentifier(const ScanResult &result)
{
    if (result.isCompressed && !result.archivePath.isEmpty() && !result.archiveInternalPath.isEmpty()) {
        const QString normalized = ArchiveExtractor::normalizeArchiveMemberPath(result.archiveInternalPath);
        if (!normalized.isEmpty()) {
            return result.archivePath + QStringLiteral("::") + normalized;
        }
    }

    return QFileInfo(result.path).absoluteFilePath();
}

QString fileRecordIdentifier(const FileRecord &record)
{
    if (!record.archiveInternalPath.isEmpty()) {
        const QString normalized = ArchiveExtractor::normalizeArchiveMemberPath(record.archiveInternalPath);
        const QString archivePath = !record.originalPath.isEmpty()
            ? QFileInfo(record.originalPath).absoluteFilePath()
            : QFileInfo(record.archivePath.isEmpty() ? record.currentPath : record.archivePath).absoluteFilePath();
        if (!normalized.isEmpty() && !archivePath.isEmpty()) {
            return archivePath + QStringLiteral("::") + normalized;
        }
    }

    const QString path = !record.originalPath.isEmpty() ? record.originalPath : record.currentPath;
    return QFileInfo(path).absoluteFilePath();
}

QList<int> orderedProcessSystemIds(Database &db, const QSet<int> &fileScopeIds)
{
    QMap<QString, int> knownSystems;
    bool hasUnknown = false;

    const QList<FileRecord> files = db.getExistingFiles();
    for (const FileRecord &file : files) {
        if (!fileMatchesProcessScope(file, fileScopeIds) || !file.isPrimary) {
            continue;
        }

        if (file.systemId > 0) {
            knownSystems.insert(db.getSystemDisplayName(file.systemId), file.systemId);
        } else {
            hasUnknown = true;
        }
    }

    QList<int> systemIds = knownSystems.values();
    if (hasUnknown) {
        systemIds.append(0);
    }
    return systemIds;
}

QString processSystemLabel(Database &db, int systemId)
{
    return systemId > 0 ? db.getSystemDisplayName(systemId) : QStringLiteral("Unknown");
}

}

int handleScanCommand(CliContext &ctx)
{
    const bool scanRequested = ctx.parser.isSet("scan") || ctx.processRequested;
    if (!scanRequested) {
        return 0;
    }

    if (ctx.processRequested) {
        ctx.processFileScopeIds.clear();
    }

    const QString scanPath = ctx.parser.isSet("scan") ? ctx.parser.value("scan") : ctx.processSourcePath;
    if (scanPath.isEmpty()) {
        qCritical() << "Scan path not provided";
        return 1;
    }

    if (ctx.processRequested) {
        const bool hasOutput = !ctx.processOutputPath.isEmpty();
        const QString effectiveBundleFormat = resolveCliOptionValue(ctx.parser,
                                                                    QStringLiteral("bundle-format"),
                                                                    ctx.presetBundleFormat);
        const QString effectiveDiscFormat = resolveCliOptionValue(ctx.parser,
                                                                  QStringLiteral("bundle-disc-format"),
                                                                  ctx.presetDiscFormat);
        const QString effectiveFolderNaming = resolveCliOptionValue(ctx.parser,
                                                                    QStringLiteral("folder-naming"),
                                                                    ctx.presetFolderNaming);
        qInfo() << "=== Full Processing Pipeline ===";
        if (!ctx.presetDisplayName.isEmpty()) {
            qInfo() << "Preset:" << ctx.presetDisplayName;
        }
        qInfo() << "Source:" << scanPath;
        if (hasOutput) {
            qInfo() << "Output:" << ctx.processOutputPath;
            qInfo() << "Archive:" << effectiveBundleFormat
                    << "| Disc:" << effectiveDiscFormat
                    << "| Folders:" << effectiveFolderNaming;
        }
        QStringList stages = {QStringLiteral("scan"), QStringLiteral("per-system [hash → match → enrich")};
        if (hasOutput) {
            stages.last().append(QStringLiteral(" → bundle"));
        }
        stages.last().append(QStringLiteral("]"));
        if (hasOutput) {
            stages << QStringLiteral("organize") << QStringLiteral("m3u");
        }
        qInfo().noquote() << "Stages:" << stages.join(QStringLiteral(" → "));
        qInfo() << "";
    }

    qInfo() << "Scanning path:" << scanPath;
    qInfo() << "";

    Scanner scanner;
    scanner.setExtensions(ctx.detector.getAllExtensions());
    QObject::connect(&scanner, &Scanner::fileFound, [](const QString &path) {
        qDebug() << "Found:" << path;
    });
    QObject::connect(&scanner, &Scanner::scanProgress, [](int processed, int) {
        if (processed % 50 == 0) {
            qInfo() << "Processed" << processed << "files...";
        }
    });

    const QList<ScanResult> results = scanner.scan(scanPath);
    qInfo() << "";
    qInfo() << "Scan complete:" << results.size() << "files found";

    const int libraryId = ctx.db.insertLibrary(scanPath);
    int insertedCount = 0;
    int skippedCount = 0;
    QHash<QString, int> insertedIds;
    for (const FileRecord &existing : ctx.db.getAllFiles()) {
        const QString identifier = fileRecordIdentifier(existing);
        if (!identifier.isEmpty()) {
            insertedIds.insert(identifier, existing.id);
        }
    }

    QList<ScanResult> orderedResults = results;
    std::stable_sort(orderedResults.begin(), orderedResults.end(),
                     [](const ScanResult &left, const ScanResult &right) {
                         // Primary files always before companions
                         if (left.isPrimary != right.isPrimary)
                             return left.isPrimary > right.isPrimary;
                         // Among primaries: smallest first (fast failures surface early)
                         return left.fileSize < right.fileSize;
                     });

    for (const ScanResult &result : orderedResults) {
        const QString systemDetectPath = result.isCompressed && !result.archiveInternalPath.isEmpty()
            ? result.archiveInternalPath
            : result.path;
        QString systemName = ctx.detector.detectSystem(result.extension, systemDetectPath);

        if (result.isCompressed && !result.archivePath.isEmpty() &&
            DiscMagicDetector::isDiscImageExtension(result.extension)) {
            // Use REMUS_TMPDIR if set; fall back to system temp.
            const QString tmpBase = qEnvironmentVariable("REMUS_TMPDIR",
                                                         QDir::tempPath());
            QTemporaryDir tempDir(tmpBase + QStringLiteral("/remus-discmagic-XXXXXX"));
            if (tempDir.isValid()) {
                ArchiveExtractor extractor;
                const QString memberPath = result.archiveInternalPath.isEmpty() ? result.filename : result.archiveInternalPath;
                const ExtractionResult extraction = extractor.extractFile(result.archivePath, memberPath, tempDir.path());
                if (extraction.success && !extraction.extractedFiles.isEmpty()) {
                    const DiscHeaderInfo discInfo = DiscMagicDetector::detect(extraction.extractedFiles.first());
                    if (discInfo.detected && !discInfo.systemName.isEmpty()) {
                        systemName = discInfo.systemName;
                        qInfo() << "  Disc magic:" << systemName << "(from" << result.filename << ")";
                    }
                } else {
                    qWarning() << "  Disc magic detection failed for" << result.filename
                               << "(extraction error:" << extraction.error.simplified() << ")"
                               << "— falling back to extension-based system detection:"
                               << systemName;
                }
            }
        }

        FileRecord record;
        record.libraryId = libraryId;
        record.originalPath = result.path;
        record.currentPath = result.path;
        record.filename = result.filename;
        record.extension = result.extension;
        record.fileSize = result.fileSize;
        record.isCompressed = result.isCompressed;
        record.archivePath = result.archivePath;
        record.archiveInternalPath = result.archiveInternalPath;
        record.systemId = systemName.isEmpty() ? 0 : ctx.db.getSystemId(systemName);
        record.isPrimary = result.isPrimary;
        if (!result.parentFilePath.isEmpty()) {
            record.parentFileId = insertedIds.value(result.parentFilePath);
        }
        record.lastModified = result.lastModified;

        const int insertedId = ctx.db.insertFile(record);
        if (insertedId > 0) {
            insertedCount++;
            insertedIds.insert(scanResultIdentifier(result), insertedId);
            if (ctx.processRequested && result.isPrimary) {
                ctx.processFileScopeIds.insert(insertedId);
            }
        } else {
            skippedCount++;
        }
    }

    qInfo() << "";
    qInfo() << "Database updated:";
    qInfo() << "  - Inserted:" << insertedCount << "files";
    qInfo() << "  - Skipped:" << skippedCount << "files";

    if (ctx.processRequested) {
        if (ctx.processFileScopeIds.isEmpty()) {
            qInfo() << "No new primary files found to process";
            ctx.processHandled = true;
            return 0;
        }

        const bool hasOutput = !ctx.processOutputPath.isEmpty();
        const QList<int> systemIds = orderedProcessSystemIds(ctx.db, ctx.processFileScopeIds);
        Hasher hasher;

        // Tool availability pre-flight — warn early if converters are missing
        if (hasOutput) {
            CHDConverter chdCheck;
            RVZConverter rvzCheck;
            CSOConverter csoCheck;
            qInfo() << "Tool availability:";
            qInfo() << (chdCheck.isChdmanAvailable()       ? "  ✓ chdman"       : "  ✗ chdman       (disc images will not be converted to CHD)");
            qInfo() << (rvzCheck.isDolphinToolAvailable()  ? "  ✓ dolphin-tool"  : "  ✗ dolphin-tool  (GameCube/Wii images will not be converted to RVZ)");
            qInfo() << (csoCheck.isMaxcsoAvailable()       ? "  ✓ maxcso"        : "  ✗ maxcso        (PSP images will not be converted to CSO)");
            qInfo() << "";
        }

        // Create a persistent artwork cache for this process run.
        // Artwork downloaded during bundling is stored here and reused across
        // per-system batches, avoiding duplicate provider round-trips.
        QTemporaryDir processArtworkTemp;
        if (hasOutput && processArtworkTemp.isValid()) {
            ctx.processArtworkCacheDir = processArtworkTemp.path();
        }

        for (int systemId : systemIds) {
            qInfo() << "";
            qInfo() << "=== System Batch ===" << processSystemLabel(ctx.db, systemId);

            const QList<FileRecord> filesToHash = ctx.db.getFilesWithoutHashes();
            int totalForSystem = 0;
            for (const FileRecord &file : filesToHash) {
                if (fileMatchesProcessScope(file, ctx.processFileScopeIds) && fileMatchesSystemFilter(file, systemId)) {
                    totalForSystem++;
                }
            }

            qInfo() << "Hashing" << totalForSystem << "file(s)...";
            int hashedCount = 0;
            for (const FileRecord &file : filesToHash) {
                if (!fileMatchesProcessScope(file, ctx.processFileScopeIds) || !fileMatchesSystemFilter(file, systemId)) {
                    continue;
                }

                const HashResult hashResult = hashFileRecord(file, hasher);
                if (hashResult.success) {
                    ctx.db.updateFileHashes(file.id, hashResult.crc32, hashResult.md5, hashResult.sha1);
                    hashedCount++;
                } else {
                    qWarning() << "  Hash failed for" << file.filename << ":" << hashResult.error;
                }
            }
            qInfo() << "Hash calculation complete:" << hashedCount << "files hashed";

            ctx.processSystemIdFilter = systemId;
            if (const int rc = handleMatchCommand(ctx)) {
                ctx.processSystemIdFilter = -1;
                return rc;
            }
            if (const int rc = handleEnrichCommand(ctx)) {
                ctx.processSystemIdFilter = -1;
                return rc;
            }
            if (hasOutput) {
                // Bundle failures are per-file and non-fatal for the pipeline;
                // continue processing remaining system batches regardless.
                handleBundleCommand(ctx);
            }
        }

        ctx.processSystemIdFilter = -1;
        if (hasOutput) {
            if (const int rc = handleOrganizeCommand(ctx)) {
                return rc;
            }
            if (const int rc = handleGenerateM3uCommand(ctx)) {
                return rc;
            }
        }

        ctx.processHandled = true;
        return 0;
    }

    if (ctx.parser.isSet("hash")) {
        qInfo() << "";
        qInfo() << "Calculating hashes...";

        Hasher hasher;
        const QList<FileRecord> filesToHash = ctx.db.getFilesWithoutHashes();
        int hashedCount = 0;
        for (const FileRecord &file : filesToHash) {
            const HashResult hashResult = hashFileRecord(file, hasher);
            if (hashResult.success) {
                ctx.db.updateFileHashes(file.id, hashResult.crc32, hashResult.md5, hashResult.sha1);
                hashedCount++;
                if (hashedCount % 10 == 0) {
                    qInfo() << "  Hashed" << hashedCount << "of" << filesToHash.size() << "files...";
                }
            } else {
                qWarning() << "  Hash failed for" << file.filename << ":" << hashResult.error;
            }
        }
        qInfo() << "Hash calculation complete:" << hashedCount << "files hashed";
    }

    return 0;
}