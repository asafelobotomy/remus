#include "cli_commands.h"
#include "cli_helpers.h"
#include <QDir>
#include <QUrl>
#include "../core/organize_engine.h"
#include "../core/m3u_generator.h"
#include "../core/constants/constants.h"
#include "../metadata/artwork_downloader.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

int handleArtworkCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("download-artwork")) return 0;

    QString artworkDirStr = ctx.parser.value("artwork-dir");
    const QString artworkTypes = ctx.parser.value("artwork-types");

    qInfo() << "";
    qInfo() << "=== Download Artwork ===";

    if (artworkDirStr.isEmpty())
        artworkDirStr = QDir::homePath() + "/.local/share/Remus/" + Constants::Settings::Files::ARTWORK_SUBDIR + "/";

    qInfo() << "Artwork directory:" << artworkDirStr;
    qInfo() << "Types to download:" << artworkTypes;
    qInfo() << "";

    QDir().mkpath(artworkDirStr);

    ArtworkDownloader downloader;
    downloader.setMaxConcurrent(4);

    auto orchestrator = buildOrchestrator(ctx.parser);
    int downloadedCount = 0, failedCount = 0;

    for (const FileRecord &file : getHashedFiles(ctx.db)) {
        qInfo() << "Processing:" << file.filename;
        GameMetadata metadata = orchestrator->searchWithFallback(
            selectBestHash(file), file.filename, "",
            file.crc32, file.md5, file.sha1);

        if (metadata.boxArtUrl.isEmpty()) {
            qInfo() << "  ✗ No box art URL";
            failedCount++;
            continue;
        }
        QUrl url(metadata.boxArtUrl);
        if (!url.isValid()) {
            qInfo() << "  ✗ Invalid URL" << metadata.boxArtUrl;
            failedCount++;
            continue;
        }
        const QString destPath = artworkDirStr + "/" +
            QFileInfo(file.filename).completeBaseName() + ".jpg";

        if (ctx.dryRunAll) {
            qInfo() << "  [DRY-RUN] would save" << destPath << "from" << url.toString();
            downloadedCount++;
        } else if (downloader.download(url, destPath)) {
            qInfo() << "  ✓ Saved" << destPath;
            downloadedCount++;
        } else {
            qInfo() << "  ✗ Download failed" << url.toString();
            failedCount++;
        }
    }

    qInfo() << "";
    qInfo() << "Artwork download complete:";
    qInfo() << "  Downloaded:" << downloadedCount;
    qInfo() << "  Failed:"     << failedCount;
    return 0;
}

int handleOrganizeCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("organize")) return 0;

    const QString destination = ctx.parser.value("organize");
    const QString templateStr = ctx.parser.value("template");
    const bool dryRun = ctx.parser.isSet("dry-run") || ctx.dryRunAll;

    qInfo() << "";
    qInfo() << "=== Organize & Rename Files (M4) ===";
    qInfo() << "Destination:" << destination;
    qInfo() << "Template:"    << templateStr;
    qInfo() << "Mode:"        << (dryRun ? "DRY RUN (preview only)" : "EXECUTE");
    qInfo() << "";

    OrganizeEngine organizer(ctx.db);
    organizer.setTemplate(templateStr);
    organizer.setDryRun(dryRun);
    organizer.setCollisionStrategy(CollisionStrategy::Rename);

    QObject::connect(&organizer, &OrganizeEngine::operationStarted,
        [](int fileId, const QString &oldPath, const QString &newPath) {
            qInfo() << "→ File" << fileId << ":" << oldPath << "->" << newPath;
        });
    QObject::connect(&organizer, &OrganizeEngine::operationCompleted,
        [](int /*fileId*/, bool success, const QString &error) {
            if (success) qInfo() << "  ✓ Success";
            else         qInfo() << "  ✗ Failed:" << error;
        });
    QObject::connect(&organizer, &OrganizeEngine::dryRunPreview,
        [](const QString &oldPath, const QString &newPath, FileOperation op) {
            const QString opName = (op == FileOperation::Move) ? "MOVE" : "COPY";
            qInfo() << "  [PREVIEW]" << opName << ":" << oldPath << "→" << newPath;
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
        if (!matches.contains(file.id)) continue;
        const auto match = matches.value(file.id);
        GameMetadata metadata;
        metadata.title  = match.gameTitle;
        metadata.region = match.region;
        metadata.system = ctx.db.getSystemDisplayName(file.systemId);
        organizer.organizeFile(file.id, metadata, destination, FileOperation::Move);
    }

    qInfo() << "";
    qInfo() << "Organization" << (dryRun ? "preview" : "complete");
    return 0;
}

int handleGenerateM3uCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("generate-m3u")) return 0;

    const QString m3uDir = ctx.parser.value("m3u-dir");

    if (ctx.dryRunAll) {
        qInfo() << "[DRY-RUN] Skipping M3U generation";
        return 0;
    }

    qInfo() << "";
    qInfo() << "=== Generate M3U Playlists ===";
    if (!m3uDir.isEmpty()) qInfo() << "Output directory:" << m3uDir;
    else                   qInfo() << "Output: Same directory as game files";
    qInfo() << "";

    M3UGenerator generator(ctx.db);

    QObject::connect(&generator, &M3UGenerator::playlistGenerated,
        [](const QString &path, int discCount) {
            qInfo() << "✓ Generated:" << path << "(" << discCount << "discs)";
        });
    QObject::connect(&generator, &M3UGenerator::errorOccurred,
        [](const QString &error) { qWarning() << "✗ Error:" << error; });

    int count = generator.generateAll(QString(), m3uDir);
    qInfo() << "";
    qInfo() << "Generated" << count << "M3U playlists";
    return 0;
}
