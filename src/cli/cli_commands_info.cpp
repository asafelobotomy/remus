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
