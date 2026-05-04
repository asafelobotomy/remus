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

    const Database::MatchResult match = ctx.db.getMatchForFile(fileId);

    if (ctx.parser.isSet("json")) {
        QJsonObject obj;
        obj[QStringLiteral("id")]             = file.id;
        obj[QStringLiteral("filename")]       = file.filename;
        obj[QStringLiteral("path")]           = file.currentPath;
        obj[QStringLiteral("originalPath")]   = file.originalPath;
        obj[QStringLiteral("extension")]      = file.extension;
        obj[QStringLiteral("size")]           = static_cast<qint64>(file.fileSize);
        obj[QStringLiteral("systemId")]       = file.systemId;
        obj[QStringLiteral("hashCalculated")] = file.hashCalculated;
        obj[QStringLiteral("crc32")]          = file.crc32;
        obj[QStringLiteral("md5")]            = file.md5;
        obj[QStringLiteral("sha1")]           = file.sha1;
        obj[QStringLiteral("isCompressed")]   = file.isCompressed;
        obj[QStringLiteral("isPrimary")]      = file.isPrimary;
        obj[QStringLiteral("fileType")]       = file.fileType;
        obj[QStringLiteral("isPatched")]      = file.isPatched;
        obj[QStringLiteral("patchName")]      = file.patchName;
        if (match.matchId != 0) {
            QJsonObject m;
            m[QStringLiteral("matchId")]     = match.matchId;
            m[QStringLiteral("gameId")]      = match.gameId;
            m[QStringLiteral("title")]       = match.gameTitle;
            m[QStringLiteral("confidence")]  = match.confidence;
            m[QStringLiteral("method")]      = match.matchMethod;
            m[QStringLiteral("publisher")]   = match.publisher;
            m[QStringLiteral("developer")]   = match.developer;
            m[QStringLiteral("releaseYear")] = match.releaseYear;
            m[QStringLiteral("genre")]       = match.genre;
            m[QStringLiteral("players")]     = match.players;
            m[QStringLiteral("rating")]      = static_cast<double>(match.rating);
            m[QStringLiteral("region")]      = match.region;
            m[QStringLiteral("description")] = match.description;
            m[QStringLiteral("confirmed")]   = match.isConfirmed;
            obj[QStringLiteral("match")]     = m;
        } else {
            obj[QStringLiteral("match")] = QJsonValue::Null;
        }
        QTextStream out(stdout);
        out << QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented)).trimmed() << Qt::endl;
        return 0;
    }

    qInfo() << "=== File Info ===";
    printFileInfo(file);
    if (match.matchId != 0) {
        qInfo() << "";
        qInfo() << "=== Match ===";
        qInfo() << "Title:      " << match.gameTitle;
        qInfo().noquote() << QStringLiteral("Confidence:  %1% [%2]").arg(match.confidence).arg(match.matchMethod);
        qInfo() << "Publisher:  " << (match.publisher.isEmpty()  ? QStringLiteral("-") : match.publisher);
        qInfo() << "Developer:  " << (match.developer.isEmpty()  ? QStringLiteral("-") : match.developer);
        qInfo() << "Release:    " << (match.releaseYear > 0      ? QString::number(match.releaseYear) : QStringLiteral("-"));
        qInfo() << "Genre:      " << (match.genre.isEmpty()      ? QStringLiteral("-") : match.genre);
        qInfo() << "Players:    " << (match.players.isEmpty()    ? QStringLiteral("-") : match.players);
        qInfo() << "Rating:     " << (match.rating > 0.0f        ? QString::number(match.rating, 'f', 1) : QStringLiteral("-"));
        qInfo() << "Region:     " << (match.region.isEmpty()     ? QStringLiteral("-") : match.region);
        qInfo() << "Confirmed:  " << (match.isConfirmed ? QStringLiteral("yes") : QStringLiteral("no"));
        if (!match.description.isEmpty()) {
            qInfo() << "Description:" << match.description.left(200) + (match.description.length() > 200 ? "..." : "");
        } else {
            qInfo() << "Description: -";
        }
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
    // --hash-all: explicit opt-in to hash everything in the DB.
    // --hash without --scan: same behaviour — hash all files lacking hashes.
    const bool scanRequested = ctx.parser.isSet("scan");
    const bool hashRequested = ctx.parser.isSet("hash") || ctx.parser.isSet("hash-all");
    if (!hashRequested) return 0;
    if (ctx.parser.isSet("hash") && scanRequested) return 0; // handled inside handleScanCommand

    qInfo() << "";
    qInfo() << "Hashing files without hashes...";
    Hasher hasher;
    QList<FileRecord> filesToHash = ctx.db.getFilesWithoutHashes();

    // Respect --file-id scope when provided
    if (!ctx.processFileScopeIds.isEmpty()) {
        filesToHash.erase(
            std::remove_if(filesToHash.begin(), filesToHash.end(),
                [&ctx](const FileRecord &f) {
                    return !fileMatchesProcessScope(f, ctx.processFileScopeIds);
                }),
            filesToHash.end());
    }

    if (filesToHash.isEmpty()) {
        qInfo() << "No files to hash (scope empty or all files already hashed).";
        return 0;
    }

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
