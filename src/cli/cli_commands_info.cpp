#include "cli_commands.h"
#include "cli_helpers.h"
#include <QDir>
#include <QMap>
#include "../core/scanner.h"
#include "../core/hasher.h"
#include "../core/header_detector.h"
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
    const int systemCount = Systems::getSystemInternalNames().size();
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
            qWarning() << "Header not detected or invalid";
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
        const QString systemName = ctx.detector.detectSystem(result.extension, systemDetectPath);
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

    qInfo() << "";
    qInfo() << "Files by system:";
    qInfo() << "─────────────────────────────────────";

    QMap<QString, int> counts = ctx.db.getFileCountBySystem();
    int total = 0;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        qInfo().noquote() << QString("%1: %2 files").arg(it.key()).arg(it.value());
        total += it.value();
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
