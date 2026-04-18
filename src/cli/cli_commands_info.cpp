#include "cli_commands.h"
#include "cli_helpers.h"
#include <algorithm>
#include <QDir>
#include <QMap>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QTemporaryDir>
#include "../core/scanner.h"
#include "../core/hasher.h"
#include "../core/header_detector.h"
#include "../core/disc_magic_detector.h"
#include "../core/archive_extractor.h"
#include "../core/constants/constants.h"
#include "terminal_image.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

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
        if (!fileMatchesProcessScope(file, fileScopeIds)) {
            continue;
        }

        if (!file.isPrimary) {
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
    return systemId > 0 ? db.getSystemDisplayName(systemId)
                        : QStringLiteral("Unknown");
}

}

int handleStatsCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("stats")) return 0;

    QList<FileRecord> files = ctx.db.getExistingFiles();
    QMap<QString, int> counts = ctx.db.getFileCountBySystem();
    int hashed = 0;
    for (const FileRecord &f : files) {
        if (f.hashCalculated) hashed++;
    }
    const int libraryCount = ctx.db.getLibraryCount();

    if (ctx.parser.isSet("json")) {
        QJsonObject obj;
        obj[QStringLiteral("libraries")] = libraryCount;
        obj[QStringLiteral("files")] = files.size();
        obj[QStringLiteral("hashed")] = hashed;
        QJsonObject bySystem;
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
            bySystem[it.key()] = it.value();
        obj[QStringLiteral("bySystem")] = bySystem;
        QTextStream out(stdout);
        out << QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented)).trimmed() << Qt::endl;
        return 0;
    }

    qInfo() << "=== Library Stats ===";
    qInfo() << "Libraries:" << libraryCount;
    qInfo() << "Files:" << files.size();
    qInfo() << "Hashed:" << hashed << "/" << files.size();
    qInfo() << "By system:";
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        qInfo().noquote() << QString("  %1: %2").arg(it.key()).arg(it.value());
    }
    return 0;
}

int handleInfoCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("info")) return 0;

    bool ok = false;
    int fileId = ctx.parser.value("info").toInt(&ok);
    if (!ok) { qCritical() << "Invalid file id"; return 1; }

    FileRecord file = ctx.db.getFileById(fileId);
    if (file.id == 0) { qCritical() << "File not found"; return 1; }

    qInfo() << "=== File Info ===";
    printFileInfo(file);
    auto match = ctx.db.getMatchForFile(fileId);
    if (match.matchId != 0) {
        qInfo() << "Match:" << match.gameTitle << "(" << match.confidence << "%)" << match.matchMethod;
    }
    return 0;
}

int handleInspectCommands(CliContext &ctx)
{
    if (ctx.parser.isSet("header-info")) {
        const QString path = ctx.parser.value("header-info");
        HeaderDetector hd;
        HeaderInfo info = hd.detect(path);
        if (!info.valid) {
            qWarning() << "No copier header detected.";
            qWarning() << "Supported formats: iNES, NES 2.0, SMC/SWC, Lynx, FDS, A78";
        } else {
            qInfo() << "=== Header Info ===";
            qInfo() << "Has header:" << info.hasHeader;
            qInfo() << "Header size:" << info.headerSize;
            qInfo() << "Type:" << info.headerType;
            qInfo() << "System hint:" << info.systemHint;
            if (!info.info.isEmpty()) qInfo() << "Info:" << info.info;
        }
    }

    if (ctx.parser.isSet("show-art")) {
        const QString imagePath = ctx.parser.value("show-art");
        if (!TerminalImage::display(imagePath)) {
            qCritical() << "Failed to display image:" << imagePath;
            return 1;
        }
    }
    return 0;
}

int handleScanCommand(CliContext &ctx)
{
    const bool scanRequested = ctx.parser.isSet("scan") || ctx.processRequested;
    if (!scanRequested) return 0;

    if (ctx.processRequested) {
        ctx.processFileScopeIds.clear();
    }

    const QString scanPath = ctx.parser.isSet("scan")
        ? ctx.parser.value("scan")
        : ctx.parser.value("process");

    if (scanPath.isEmpty()) { qCritical() << "Scan path not provided"; return 1; }

    if (ctx.processRequested) {
        const bool hasOutput = ctx.parser.isSet("process-output") || ctx.parser.isSet("bundle");
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
        if (!ctx.presetDisplayName.isEmpty())
            qInfo() << "Preset:" << ctx.presetDisplayName;
        qInfo() << "Source:" << scanPath;
        if (hasOutput) {
            const QString output = ctx.parser.isSet("process-output")
                ? ctx.parser.value("process-output")
                : ctx.parser.value("bundle");
            qInfo() << "Output:" << output;
        }
        if (hasOutput)
            qInfo() << "Archive:" << effectiveBundleFormat
                     << "| Disc:" << effectiveDiscFormat
                     << "| Folders:" << effectiveFolderNaming;
        qInfo() << "";
        QStringList stages = {QStringLiteral("scan"), QStringLiteral("per-system [hash → match → enrich")};
        if (hasOutput) {
            stages.last().append(QStringLiteral(" → bundle"));
        }
        stages.last().append(QStringLiteral("]"));
        if (hasOutput) stages << QStringLiteral("m3u");
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
        if (processed % 50 == 0) qInfo() << "Processed" << processed << "files...";
    });

    QList<ScanResult> results = scanner.scan(scanPath);
    qInfo() << "";
    qInfo() << "Scan complete:" << results.size() << "files found";

    int libraryId = ctx.db.insertLibrary(scanPath);
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
        return static_cast<int>(!left.isPrimary) < static_cast<int>(!right.isPrimary);
    });

    for (const ScanResult &result : orderedResults) {
        const QString systemDetectPath = result.isCompressed && !result.archiveInternalPath.isEmpty()
            ? result.archiveInternalPath : result.path;
        QString systemName = ctx.detector.detectSystem(result.extension, systemDetectPath);

        // For compressed disc images, extract and probe magic bytes for accurate system detection
        if (result.isCompressed && !result.archivePath.isEmpty()
            && DiscMagicDetector::isDiscImageExtension(result.extension)) {
            QTemporaryDir tempDir;
            if (tempDir.isValid()) {
                ArchiveExtractor extractor;
                const QString memberPath = result.archiveInternalPath.isEmpty()
                    ? result.filename : result.archiveInternalPath;
                ExtractionResult ex = extractor.extractFile(
                    result.archivePath, memberPath, tempDir.path());
                if (ex.success && !ex.extractedFiles.isEmpty()) {
                    DiscHeaderInfo discInfo = DiscMagicDetector::detect(ex.extractedFiles.first());
                    if (discInfo.detected && !discInfo.systemName.isEmpty()) {
                        systemName = discInfo.systemName;
                        qInfo() << "  Disc magic:" << systemName << "(from" << result.filename << ")";
                    }
                }
            }
        }

        const int systemId = systemName.isEmpty() ? 0 : ctx.db.getSystemId(systemName);

        FileRecord record;
        record.libraryId          = libraryId;
        record.originalPath       = result.path;
        record.currentPath        = result.path;
        record.filename           = result.filename;
        record.extension          = result.extension;
        record.fileSize           = result.fileSize;
        record.isCompressed       = result.isCompressed;
        record.archivePath        = result.archivePath;
        record.archiveInternalPath = result.archiveInternalPath;
        record.systemId           = systemId;
        record.isPrimary          = result.isPrimary;
        if (!result.parentFilePath.isEmpty()) {
            record.parentFileId = insertedIds.value(result.parentFilePath);
        }
        record.lastModified       = result.lastModified;

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

        const bool hasOutput = ctx.parser.isSet("process-output") || ctx.parser.isSet("bundle");
        const QList<int> systemIds = orderedProcessSystemIds(ctx.db, ctx.processFileScopeIds);
        Hasher hasher;

        for (int systemId : systemIds) {
            qInfo() << "";
            qInfo() << "=== System Batch ===" << processSystemLabel(ctx.db, systemId);

            const QList<FileRecord> filesToHash = ctx.db.getFilesWithoutHashes();
            int totalForSystem = 0;
            for (const FileRecord &file : filesToHash) {
                if (fileMatchesProcessScope(file, ctx.processFileScopeIds)
                    && fileMatchesSystemFilter(file, systemId)) {
                    totalForSystem++;
                }
            }

            qInfo() << "Hashing" << totalForSystem << "file(s)...";
            int hashedCount = 0;
            for (const FileRecord &file : filesToHash) {
                if (!fileMatchesProcessScope(file, ctx.processFileScopeIds)
                    || !fileMatchesSystemFilter(file, systemId)) continue;

                HashResult hashResult = hashFileRecord(file, hasher);
                if (hashResult.success) {
                    ctx.db.updateFileHashes(file.id, hashResult.crc32, hashResult.md5, hashResult.sha1);
                    hashedCount++;
                } else {
                    qWarning() << "  Hash failed for" << file.filename << ":" << hashResult.error;
                }
            }
            qInfo() << "Hash calculation complete:" << hashedCount << "files hashed";

            ctx.processSystemIdFilter = systemId;
            if (int rc = handleMatchCommand(ctx)) {
                ctx.processSystemIdFilter = -1;
                return rc;
            }
            if (int rc = handleEnrichCommand(ctx)) {
                ctx.processSystemIdFilter = -1;
                return rc;
            }
            if (hasOutput) {
                if (int rc = handleBundleCommand(ctx)) {
                    ctx.processSystemIdFilter = -1;
                    return rc;
                }
            }
        }

        ctx.processSystemIdFilter = -1;
        if (hasOutput) {
            if (int rc = handleGenerateM3uCommand(ctx)) {
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
        QList<FileRecord> filesToHash = ctx.db.getFilesWithoutHashes();
        int hashedCount = 0;

        for (const FileRecord &file : filesToHash) {
            HashResult hashResult = hashFileRecord(file, hasher);
            if (hashResult.success) {
                ctx.db.updateFileHashes(file.id, hashResult.crc32, hashResult.md5, hashResult.sha1);
                hashedCount++;
                if (hashedCount % 10 == 0)
                    qInfo() << "  Hashed" << hashedCount << "of" << filesToHash.size() << "files...";
            } else {
                qWarning() << "  Hash failed for" << file.filename << ":" << hashResult.error;
            }
        }
        qInfo() << "Hash calculation complete:" << hashedCount << "files hashed";
    }
    return 0;
}

int handleListCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("list")) return 0;

    QMap<QString, int> counts = ctx.db.getFileCountBySystem();
    int total = 0;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        total += it.value();

    if (ctx.parser.isSet("json")) {
        QJsonArray arr;
        for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
            QJsonObject obj;
            obj[QStringLiteral("system")] = it.key();
            obj[QStringLiteral("files")] = it.value();
            arr.append(obj);
        }
        QJsonObject root;
        root[QStringLiteral("systems")] = arr;
        root[QStringLiteral("total")] = total;
        QTextStream out(stdout);
        out << QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented)).trimmed() << Qt::endl;
        return 0;
    }

    qInfo() << "";
    qInfo() << "Files by system:";
    qInfo() << "─────────────────────────────────────";
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        qInfo().noquote() << QString("%1: %2 files").arg(it.key()).arg(it.value());
    }
    qInfo() << "─────────────────────────────────────";
    qInfo() << "Total:" << total << "files";
    return 0;
}

int handleHashAllCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("hash-all")) return 0;

    qInfo() << "";
    qInfo() << "Hashing files without hashes...";
    Hasher hasher;
    QList<FileRecord> filesToHash = ctx.db.getFilesWithoutHashes();
    int hashedCount = 0;

    for (const FileRecord &file : filesToHash) {
        HashResult hashResult = hashFileRecord(file, hasher);
        if (hashResult.success) {
            ctx.db.updateFileHashes(file.id, hashResult.crc32, hashResult.md5, hashResult.sha1);
            hashedCount++;
            if (hashedCount % 10 == 0)
                qInfo() << "  Hashed" << hashedCount << "of" << filesToHash.size() << "files...";
        } else {
            qWarning() << "  Hash failed for" << file.filename << ":" << hashResult.error;
        }
    }
    qInfo() << "Hashing complete:" << hashedCount << "files hashed";
    return 0;
}

int handleReclassifyIsoCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("reclassify-iso")) return 0;

    qInfo() << "";
    qInfo() << "Reclassifying ISO files...";

    QSqlQuery select(ctx.db.database());
    if (!select.exec(QStringLiteral(R"(
        SELECT id, extension, current_path, is_compressed, archive_internal_path, system_id
        FROM files
        WHERE LOWER(extension) = '.iso' AND is_primary = 1
    )"))) {
        qCritical() << "Failed to load ISO file rows:" << select.lastError().text();
        return 1;
    }

    QSqlQuery update(ctx.db.database());
    update.prepare(QStringLiteral("UPDATE files SET system_id = ? WHERE id = ?"));

    int scanned = 0;
    int changed = 0;
    int unchanged = 0;
    int unresolved = 0;

    while (select.next()) {
        const int fileId = select.value(0).toInt();
        const QString extension = select.value(1).toString();
        const QString currentPath = select.value(2).toString();
        const bool isCompressed = select.value(3).toBool();
        const QString archiveInternalPath = select.value(4).toString();
        const int currentSystemId = select.value(5).toInt();
        ++scanned;

        const QString detectPath = isCompressed && !archiveInternalPath.isEmpty()
            ? archiveInternalPath
            : currentPath;
        const QString detectedSystemName = ctx.detector.detectSystem(extension, detectPath);
        const int detectedSystemId = detectedSystemName.isEmpty()
            ? 0
            : ctx.db.getSystemId(detectedSystemName);

        if (detectedSystemId == 0) {
            ++unresolved;
            continue;
        }

        if (currentSystemId == detectedSystemId) {
            ++unchanged;
            continue;
        }

        update.bindValue(0, detectedSystemId);
        update.bindValue(1, fileId);
        if (!update.exec()) {
            qCritical() << "Failed to update system assignment for file" << fileId << ":"
                        << update.lastError().text();
            return 1;
        }

        ++changed;
    }

    qInfo() << "ISO reclassification complete:";
    qInfo() << "  - Scanned:" << scanned;
    qInfo() << "  - Changed:" << changed;
    qInfo() << "  - Unchanged:" << unchanged;
    qInfo() << "  - Unresolved:" << unresolved;
    return 0;
}
