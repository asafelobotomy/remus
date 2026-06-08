#include "cli_commands.h"
#include "cli_helpers.h"
#include <QDir>
#include <QFileInfo>
#include "../core/organize_engine.h"
#include "../core/m3u_generator.h"
#include "../core/constants/constants.h"
#include "../core/constants/folder_naming.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

// handleArtworkCommand and handleBundleCommand extracted to cli_commands_bundle.cpp

int handleOrganizeCommand(CliContext &ctx) {
    const bool organizeExplicit = ctx.parser.isSet("organize");
    const bool organizeFromProcess = ctx.processRequested && !ctx.processOutputPath.isEmpty() && !ctx.processHandled;
    if (!organizeExplicit && !organizeFromProcess)
        return 0;

    const QString destination = organizeExplicit ? ctx.parser.value("organize") : ctx.processOutputPath;
    const QString templateStr = ctx.parser.value("template");
    // When triggered from process/library, default to system subfolders if user
    // has not set --folder-naming explicitly and no preset has already set it.
    const QString folderNamingStr
        = !ctx.parser.value("folder-naming").isEmpty() && ctx.parser.value("folder-naming") != QStringLiteral("none")
        ? ctx.parser.value("folder-naming")
        : (!ctx.presetFolderNaming.isEmpty()
                  ? ctx.presetFolderNaming
                  : (organizeFromProcess ? QStringLiteral("default") : ctx.parser.value("folder-naming")));
    const bool dryRun = ctx.parser.isSet("dry-run") || ctx.dryRunAll;
    const auto folderNaming = Constants::FolderNaming::schemeFromString(folderNamingStr);

    qInfo() << "";
    qInfo() << "=== Organize & Rename Files (M4) ===";
    qInfo() << "Destination:" << destination;
    qInfo() << "Template:" << templateStr;
    qInfo() << "Folder naming:" << Constants::FolderNaming::schemeDisplayName(folderNaming);
    qInfo() << "Mode:" << (dryRun ? "DRY RUN (preview only)" : "EXECUTE");
    qInfo() << "";

    OrganizeEngine organizer(ctx.db);
    organizer.setTemplate(templateStr);
    organizer.setDryRun(dryRun);
    organizer.setCollisionStrategy(CollisionStrategy::Rename);
    organizer.setFolderNaming(folderNaming);

    QObject::connect(
        &organizer, &OrganizeEngine::operationStarted, [](int fileId, const QString &oldPath, const QString &newPath) {
            qInfo() << "\u2192 File" << fileId << ":" << oldPath << "->" << newPath;
        });
    QObject::connect(
        &organizer, &OrganizeEngine::operationCompleted, [](int /*fileId*/, bool success, const QString &error) {
            if (success)
                qInfo() << "  \u2713 Success";
            else
                qInfo() << "  \u2717 Failed:" << error;
        });
    QObject::connect(&organizer, &OrganizeEngine::dryRunPreview,
        [](const QString &oldPath, const QString &newPath, FileOperation op) {
            const QString opName = (op == FileOperation::Move) ? "MOVE" : "COPY";
            qInfo() << "  [PREVIEW]" << opName << ":" << oldPath << "\u2192" << newPath;
        });

    QMap<int, Database::MatchResult> matches = ctx.db.getAllMatches();
    QList<FileRecord> files = ctx.db.getExistingFiles();

    if (files.isEmpty()) {
        qInfo() << "No files to organize";
        return 0;
    }
    qInfo() << "Processing" << files.size() << "files...";
    qInfo() << "";

    for (const FileRecord &file : files) {
        if (!matches.contains(file.id))
            continue;
        const auto match = matches.value(file.id);
        GameMetadata metadata;
        metadata.title = match.gameTitle;
        metadata.region = match.region;
        metadata.system = ctx.db.getSystemDisplayName(file.systemId);
        organizer.organizeFile(file.id, metadata, destination, FileOperation::Move);
    }

    qInfo() << "";
    qInfo() << "Organization" << (dryRun ? "preview" : "complete");
    return 0;
}

int handleGenerateM3uCommand(CliContext &ctx) {
    const bool hasOutput = !ctx.processOutputPath.isEmpty();
    if (!ctx.parser.isSet("generate-m3u") && !(ctx.processRequested && hasOutput))
        return 0;
    if (ctx.processRequested && ctx.processHandled)
        return 0;

    const QString m3uDir = ctx.parser.isSet("m3u-dir")
        ? ctx.parser.value("m3u-dir")
        : (ctx.processRequested && hasOutput ? ctx.processOutputPath : QString());

    if (ctx.dryRunAll) {
        qInfo() << "[DRY-RUN] Skipping M3U generation";
        return 0;
    }

    qInfo() << "";
    qInfo() << "=== Generate M3U Playlists ===";
    if (!m3uDir.isEmpty())
        qInfo() << "Output directory:" << m3uDir;
    else
        qInfo() << "Output: Same directory as game files";
    qInfo() << "";

    M3UGenerator generator(ctx.db);

    QObject::connect(&generator, &M3UGenerator::playlistGenerated, [](const QString &path, int discCount) {
        qInfo() << "\u2713 Generated:" << path << "(" << discCount << "discs)";
    });
    QObject::connect(
        &generator, &M3UGenerator::errorOccurred, [](const QString &error) { qWarning() << "\u2717 Error:" << error; });

    int count = ctx.processFileScopeIds.isEmpty() ? generator.generateAll(QString(), m3uDir)
                                                  : generator.generateAll(ctx.processFileScopeIds, m3uDir);
    qInfo() << "";
    qInfo() << "Generated" << count << "M3U playlists";
    return 0;
}
