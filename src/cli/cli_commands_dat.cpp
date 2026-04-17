#include "cli_commands.h"
#include "cli_helpers.h"
#include "../metadata/local_database_provider.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

// ── --update-dats ─────────────────────────────────────────────────────────────
int handleUpdateDatsCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("update-dats")) return 0;

    const bool fetchAll = ctx.parser.isSet("update-dats-all");

    // Locate the update script relative to the binary or cwd
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd    = QDir::currentPath();
    const QStringList candidates = {
        cwd    + "/scripts/update_dats.sh",
        appDir + "/scripts/update_dats.sh",
        appDir + "/../scripts/update_dats.sh",
        appDir + "/../../scripts/update_dats.sh",
    };

    QString scriptPath;
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c)) {
            scriptPath = QDir::cleanPath(c);
            break;
        }
    }

    if (scriptPath.isEmpty()) {
        qCritical() << "✗ Could not find scripts/update_dats.sh";
        qInfo() << "Searched:";
        for (const QString &c : candidates)
            qInfo() << "  " << QDir::cleanPath(c);
        return 1;
    }

    qInfo() << "";
    qInfo() << "=== Updating DAT Databases ===";
    qInfo() << "Script:" << scriptPath;
    qInfo() << "";

    QProcess proc;
    proc.setProcessChannelMode(QProcess::ForwardedChannels);

    QStringList args;
    if (fetchAll) args << "--all";
    proc.start(QStringLiteral("bash"), QStringList{scriptPath} + args);

    if (!proc.waitForStarted(5000)) {
        qCritical() << "✗ Failed to start update script — is bash installed?";
        return 1;
    }

    proc.waitForFinished(-1);   // no timeout — git clone can be slow

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        qCritical() << "✗ Update script exited with code" << proc.exitCode();
        return 1;
    }

    qInfo() << "";
    qInfo() << "DAT update complete. Re-run --match to use the new databases.";
    return 0;
}

// ── --import-dat ──────────────────────────────────────────────────────────────
int handleImportDatCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("import-dat")) return 0;

    const QString datFile = ctx.parser.value("import-dat");
    QFileInfo info(datFile);
    if (!info.exists() || !info.isFile()) {
        qCritical() << "✗ DAT file not found:" << datFile;
        return 1;
    }

    const QString destDir = findDatabaseDir();
    if (destDir.isEmpty()) {
        // Create the default directory next to the binary
        const QString fallback = QCoreApplication::applicationDirPath()
                                 + QStringLiteral("/data/databases");
        QDir().mkpath(fallback);
        qInfo() << "Created DAT directory:" << fallback;
    }

    const QString targetDir = destDir.isEmpty()
        ? QCoreApplication::applicationDirPath() + QStringLiteral("/data/databases")
        : destDir;

    const QString destPath = targetDir + QDir::separator() + info.fileName();
    if (QFile::exists(destPath)) {
        // Check if the incoming file is newer
        LocalDatabaseProvider tempProvider;
        tempProvider.loadDatabase(destPath);
        if (!tempProvider.isDatNewer(datFile)) {
            qInfo() << "DAT file is not newer than existing — skipping:" << info.fileName();
            return 0;
        }
        QFile::remove(destPath);
    }

    if (!QFile::copy(datFile, destPath)) {
        qCritical() << "✗ Failed to copy DAT file to:" << destPath;
        return 1;
    }

    // Validate by loading
    LocalDatabaseProvider provider;
    int count = provider.loadDatabase(destPath);
    if (count <= 0) {
        qCritical() << "✗ DAT file loaded 0 entries — may be malformed:" << info.fileName();
        QFile::remove(destPath);
        return 1;
    }

    qInfo() << "";
    qInfo() << "=== DAT File Imported ===";
    qInfo() << "File:" << info.fileName();
    qInfo() << "Entries:" << count;
    qInfo() << "Installed to:" << destPath;
    return 0;
}

// ── --remove-dat ─────────────────────────────────────────────────────────────
int handleRemoveDatCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("remove-dat")) return 0;

    const QString datName = ctx.parser.value("remove-dat");
    const QString destDir = findDatabaseDir();
    if (destDir.isEmpty()) {
        qCritical() << "✗ No DAT directory found";
        return 1;
    }

    // Try exact filename first, then append .dat
    QString target = destDir + QDir::separator() + datName;
    if (!QFile::exists(target) && !datName.endsWith(".dat")) {
        target = destDir + QDir::separator() + datName + ".dat";
    }

    if (!QFile::exists(target)) {
        qCritical() << "✗ DAT file not found:" << datName;
        qInfo() << "Available DATs in" << destDir << ":";
        QDir dir(destDir);
        for (const auto &f : dir.entryList({"*.dat"}, QDir::Files)) {
            qInfo() << "  " << f;
        }
        return 1;
    }

    if (!QFile::remove(target)) {
        qCritical() << "✗ Failed to remove:" << target;
        return 1;
    }

    qInfo() << "Removed DAT:" << QFileInfo(target).fileName();
    return 0;
}

// ── --list-dats ──────────────────────────────────────────────────────────────
int handleListDatsCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("list-dats")) return 0;

    const QString destDir = findDatabaseDir();
    if (destDir.isEmpty()) {
        qInfo() << "No DAT directory found. Use --import-dat to add DAT files.";
        return 0;
    }

    QDir dir(destDir);
    QFileInfoList dats = dir.entryInfoList({"*.dat"}, QDir::Files, QDir::Name);

    if (dats.isEmpty()) {
        qInfo() << "No DAT files installed in" << destDir;
        return 0;
    }

    qInfo() << "";
    qInfo() << "=== Installed DAT Files ===";
    qInfo() << "Directory:" << destDir;
    qInfo() << "";

    LocalDatabaseProvider provider;
    for (const QFileInfo &fi : dats) {
        int count = provider.loadDatabase(fi.absoluteFilePath());
        qInfo().noquote() << QString("  %1  (%2 entries)")
            .arg(fi.fileName(), -50)
            .arg(count);
    }

    qInfo() << "";
    qInfo() << "Total:" << dats.size() << "DAT files";
    return 0;
}

// ── --edit-metadata ──────────────────────────────────────────────────────────
int handleEditMetadataCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("edit-metadata")) return 0;

    const QString fileId = ctx.parser.value("edit-metadata");
    bool ok = false;
    int id = fileId.toInt(&ok);
    if (!ok || id <= 0) {
        qCritical() << "✗ Invalid file ID:" << fileId;
        return 1;
    }

    // Retrieve current file record
    FileRecord file = ctx.db.getFileById(id);
    if (file.id == 0) {
        qCritical() << "✗ File not found with ID:" << id;
        return 1;
    }

    // Find the matched game for this file
    MatchResult match = ctx.db.getMatchForFile(id);
    if (match.gameId == 0) {
        qCritical() << "✗ No metadata match found for file ID:" << id;
        qInfo() << "Run --match first to associate metadata with this file.";
        return 1;
    }

    // Collect edits from flags (empty string = keep existing per updateGame semantics)
    const QString title     = ctx.parser.isSet("set-title")     ? ctx.parser.value("set-title")     : QString();
    const QString region    = ctx.parser.isSet("set-region")    ? ctx.parser.value("set-region")    : QString();
    const QString genre     = ctx.parser.isSet("set-genre")     ? ctx.parser.value("set-genre")     : QString();
    const QString developer = ctx.parser.isSet("set-developer") ? ctx.parser.value("set-developer") : QString();
    const QString publisher = ctx.parser.isSet("set-publisher") ? ctx.parser.value("set-publisher") : QString();

    if (title.isEmpty() && region.isEmpty() && genre.isEmpty()
        && developer.isEmpty() && publisher.isEmpty()) {
        qInfo() << "No metadata fields specified. Available flags:";
        qInfo() << "  --set-title, --set-region, --set-genre, --set-developer, --set-publisher";
        return 0;
    }

    // updateGame signature: (gameId, publisher, developer, releaseDate, description, genres, players, rating)
    // We only update the fields the user specified; empty strings are kept existing
    if (!ctx.db.updateGame(match.gameId, publisher, developer,
                            /*releaseDate*/ QString(),
                            /*description*/ QString(),
                            genre,
                            /*players*/ QString(),
                            /*rating*/ -1.0f)) {
        qCritical() << "✗ Failed to update game metadata";
        return 1;
    }

    qInfo() << "";
    qInfo() << "=== Metadata Updated ===";
    qInfo() << "File ID:" << id;
    qInfo() << "Game ID:" << match.gameId;
    if (!title.isEmpty())     qInfo() << "  Title →" << title;
    if (!region.isEmpty())    qInfo() << "  Region →" << region;
    if (!genre.isEmpty())     qInfo() << "  Genre →" << genre;
    if (!developer.isEmpty()) qInfo() << "  Developer →" << developer;
    if (!publisher.isEmpty()) qInfo() << "  Publisher →" << publisher;
    return 0;
}
