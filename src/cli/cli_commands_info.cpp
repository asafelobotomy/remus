#include "cli_commands.h"
#include "cli_helpers.h"
#include <QDir>
#include <QMap>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

int handleStatsCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("stats")) return 0;

    QList<FileRecord> files = ctx.db.getExistingFiles();
    QMap<QString, int> counts = ctx.db.getFileCountBySystem();
    int hashed = 0;
    for (const FileRecord &f : files) {
        if (f.hashCalculated) hashed++;
    }
    const int systemCount = counts.size();  // Actual populated systems, not total definitions

    if (ctx.parser.isSet("json")) {
        QJsonObject obj;
        obj[QStringLiteral("libraries")] = systemCount;
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
    qInfo() << "Libraries:" << systemCount;
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

    const QString scanPath = ctx.parser.isSet("scan")
        ? ctx.parser.value("scan")
        : ctx.parser.value("process");

    if (scanPath.isEmpty()) { qCritical() << "Scan path not provided"; return 1; }

    if (ctx.processRequested) {
        const bool hasOutput = ctx.parser.isSet("process-output") || ctx.parser.isSet("bundle");
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
        if (!ctx.presetBundleFormat.isEmpty())
            qInfo() << "Archive:" << ctx.presetBundleFormat
                     << "| Disc:" << ctx.presetDiscFormat
                     << "| Folders:" << ctx.presetFolderNaming;
        qInfo() << "";
        QStringList stages = {QStringLiteral("scan"), QStringLiteral("hash"), QStringLiteral("match"),
                              QStringLiteral("enrich")};
        if (hasOutput) stages << QStringLiteral("bundle");
        qInfo().noquote() << "Stages:" << stages.join(QStringLiteral(" → "));
        qInfo() << "";
    }

    qInfo() << "Scanning directory:" << scanPath;
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

    for (const ScanResult &result : results) {
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
        record.lastModified       = result.lastModified;

        if (ctx.db.insertFile(record) > 0) insertedCount++; else skippedCount++;
    }

    qInfo() << "";
    qInfo() << "Database updated:";
    qInfo() << "  - Inserted:" << insertedCount << "files";
    qInfo() << "  - Skipped:" << skippedCount << "files";

    if (ctx.parser.isSet("hash") || ctx.processRequested) {
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
