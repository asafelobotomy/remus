#include "cli_commands.h"
#include "cli_helpers.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>
#include "../core/organize_engine.h"
#include "../core/rom_bundler.h"
#include "../core/m3u_generator.h"
#include "../core/constants/constants.h"
#include "../core/constants/folder_naming.h"
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

    auto orchestrator = buildOrchestrator(ctx.parser, &ctx.db);
    int downloadedCount = 0, failedCount = 0;

    for (const FileRecord &file : getHashedFiles(ctx.db)) {
        const QString displayName = getMatchingDisplayName(file);
        const QString systemName = getProviderLookupSystemName(file);
        qInfo() << "Processing:" << displayName;
        GameMetadata metadata = orchestrator->searchWithFallback(
            selectBestHash(file), displayName, systemName,
            file.crc32, file.md5, file.sha1, QString(), true);

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
        } else {
            QString savedPath;
            if (!downloader.download(url, destPath, &savedPath)) {
                qInfo() << "  ✗ Download failed" << url.toString();
                failedCount++;
                continue;
            }

            qInfo() << "  ✓ Saved" << (savedPath.isEmpty() ? destPath : savedPath);
            downloadedCount++;
        }
    }

    qInfo() << "";
    qInfo() << "Artwork download complete:";
    qInfo() << "  Downloaded:" << downloadedCount;
    qInfo() << "  Failed:"     << failedCount;
    return 0;
}

int handleBundleCommand(CliContext &ctx)
{
    // Trigger from --bundle directly, or from --process/--library when output is provided
    const bool bundleExplicit = ctx.parser.isSet("bundle");
    const bool bundleFromProcess = ctx.processRequested && !ctx.processOutputPath.isEmpty();
    if (!bundleExplicit && !bundleFromProcess) return 0;
    if (ctx.processRequested && ctx.processHandled) return 0;

    const QString destination  = ctx.parser.isSet("bundle")
        ? ctx.parser.value("bundle")
        : ctx.processOutputPath;

    // Preset values serve as defaults; explicit CLI flags override them
    const QString formatStr    = resolveCliOptionValue(ctx.parser,
                                                       QStringLiteral("bundle-format"),
                                                       ctx.presetBundleFormat);
    const QString discFormat   = resolveCliOptionValue(ctx.parser,
                                                       QStringLiteral("bundle-disc-format"),
                                                       ctx.presetDiscFormat).toLower();
    const QString folderStr    = resolveCliOptionValue(ctx.parser,
                                                       QStringLiteral("folder-naming"),
                                                       ctx.presetFolderNaming);
    const bool    dryRun       = ctx.parser.isSet("dry-run") || ctx.dryRunAll;
    const auto    folderNaming = Constants::FolderNaming::schemeFromString(folderStr);
    const QString artworkDir   = ctx.parser.value("bundle-art-dir");

    const ArchiveFormat fmt = (formatStr == "7z") ? ArchiveFormat::SevenZip
                                                  : ArchiveFormat::ZIP;
    if (discFormat != "original" && discFormat != "chd" && discFormat != "rvz") {
        qCritical() << "✗ Unsupported bundle disc format:" << discFormat;
        qInfo() << "Supported values: original, chd, rvz";
        return 1;
    }

    const RomBundler::DiscOutputFormat discOutputFormat =
        discFormat == QStringLiteral("rvz") ? RomBundler::DiscOutputFormat::Rvz :
        discFormat == QStringLiteral("chd") ? RomBundler::DiscOutputFormat::Chd :
                                              RomBundler::DiscOutputFormat::Original;

    qInfo() << "";
    qInfo() << "=== Bundle Matched ROMs ===";
    if (!ctx.presetDisplayName.isEmpty())
        qInfo() << "Preset:"     << ctx.presetDisplayName;
    qInfo() << "Destination:" << destination;
    qInfo() << "Format:"      << formatStr;
    qInfo() << "Disc Media:"  << discFormat;
    if (folderNaming != Constants::FolderNaming::Scheme::None)
        qInfo() << "Folder naming:" << Constants::FolderNaming::schemeDisplayName(folderNaming);
    qInfo() << "Mode:"        << (dryRun ? "DRY RUN (preview only)" : "EXECUTE");
    qInfo() << "";

    RomBundler bundler(ctx.db);
    ArtworkDownloader downloader;
    QTemporaryDir tempArtworkDir;

    // Build orchestrator only when we might need box art URLs from providers.
    // Match metadata is already persisted in the DB — no need to re-fetch titles.
    std::unique_ptr<ProviderOrchestrator> orchestrator;

    QObject::connect(&bundler, &RomBundler::progressMessage,
        [](const QString &msg) { qInfo() << " " << msg; });

    QMap<int, Database::MatchResult> matches = ctx.db.getAllMatches();
    QList<FileRecord> files = ctx.db.getExistingFiles();

    if (files.isEmpty()) {
        qInfo() << "No files to bundle";
        return 0;
    }

    int bundled = 0, skipped = 0, failed = 0;

    for (const FileRecord &file : files) {
        if (!fileMatchesProcessScope(file, ctx.processFileScopeIds)) continue;
        if (!matches.contains(file.id)) continue;

        const Database::MatchResult &match = matches.value(file.id);
        if (match.isRejected) continue;
        if (!fileMatchesSystemFilter(file, ctx.processSystemIdFilter, &match)) continue;

        const int bundledSystemId = resolveMatchedSystemId(file, &match);
        const QString systemName = getProviderLookupSystemName(file, &match);

        // Build GameMetadata from the DB-cached match — no provider round-trip
        GameMetadata metadata;
        metadata.title       = match.gameTitle;
        metadata.publisher   = match.publisher;
        metadata.developer   = match.developer;
        metadata.description = match.description;
        metadata.rating      = match.rating;
        metadata.region      = match.region;
        metadata.matchMethod = match.matchMethod;
        metadata.matchScore  = match.confidence;
        if (!match.genre.isEmpty())
            metadata.genres = match.genre.split(QStringLiteral(", "), Qt::SkipEmptyParts);

        // Resolve artwork path for this specific file
        QString artworkPath;
        // 1. Check process-run artwork cache (populated by earlier system batches;
        //    avoids duplicate provider round-trips within a single --process run)
        if (artworkPath.isEmpty() && !ctx.processArtworkCacheDir.isEmpty()) {
            const QString cached = ctx.processArtworkCacheDir + "/" + QString::number(file.id) + ".jpg";
            if (QFile::exists(cached))
                artworkPath = cached;
        }
        // 2. Check explicit --bundle-art-dir
        if (!artworkPath.isEmpty()) {
        } else if (!artworkDir.isEmpty()) {
            const QString candidate = artworkDir + "/" +
                QFileInfo(file.filename).completeBaseName() + ".jpg";
            if (QFile::exists(candidate))
                artworkPath = candidate;
        }

        // If no pre-downloaded art, try providers to get a box art URL
        if (artworkPath.isEmpty()) {
            if (!orchestrator)
                orchestrator = buildOrchestrator(ctx.parser, &ctx.db);

            const QString displayName = getMatchingDisplayName(file);
            GameMetadata providerMeta = orchestrator->searchWithFallback(
                selectBestHash(file), displayName, systemName,
                file.crc32, file.md5, file.sha1, QString(), true);

            if (!providerMeta.boxArtUrl.isEmpty()) {
                const QUrl boxArtUrl(providerMeta.boxArtUrl);
                if (boxArtUrl.isValid()) {
                    const QString ext = QFileInfo(boxArtUrl.path()).suffix().isEmpty()
                        ? QStringLiteral("jpg")
                        : QFileInfo(boxArtUrl.path()).suffix().toLower();
                    // Prefer persistent process cache; fall back to per-invocation temp dir
                    const QString artCacheDir = ctx.processArtworkCacheDir.isEmpty()
                        ? tempArtworkDir.path()
                        : ctx.processArtworkCacheDir;
                    const QString destPath = artCacheDir + "/" +
                        QString::number(file.id) + "." + ext;

                    if (dryRun) {
                        qInfo() << "  [DRY-RUN] Would download box art from:" << boxArtUrl;
                    } else {
                        QString savedPath;
                        if (downloader.download(boxArtUrl, destPath, &savedPath)) {
                            artworkPath = savedPath.isEmpty() ? destPath : savedPath;
                            qInfo() << "  ✓ Downloaded box art:" << artworkPath;
                        } else {
                            qWarning() << "  ⚠ Failed to download box art from:" << boxArtUrl;
                        }
                    }
                }
            }
        }

        // Resolve system subfolder when folder-naming is active
        QString effectiveDestination = destination;
        if (folderNaming != Constants::FolderNaming::Scheme::None && bundledSystemId > 0) {
            const QString systemFolder = Constants::FolderNaming::folderNameForSystemId(
                bundledSystemId, folderNaming);
            if (!systemFolder.isEmpty()) {
                effectiveDestination = QDir(destination).filePath(systemFolder);
            }
        }

        const RomBundler::BundleConfig config{
            /*includeBoxArt*/ true,
            /*dryRun*/        dryRun,
            /*outputFormat*/  fmt,
            /*artworkPath*/   artworkPath,
            /*discOutputFormat*/ discOutputFormat
        };

        qInfo() << "Bundling:" << file.filename;

        RomBundler::BundleResult result = bundler.bundle(
            file, match, metadata, effectiveDestination, config);

        if (result.skippedAlreadyBundled) {
            qInfo() << "  ↷ Skipped (already bundled)";
            skipped++;
        } else if (result.success) {
            if (!result.outputPath.isEmpty())
                ctx.db.updateFilePath(file.id, result.outputPath);
            bundled++;
        } else {
            qWarning() << "  ✗ Failed:" << result.error;
            failed++;
        }
    }

    qInfo() << "";
    qInfo() << "Bundle" << (dryRun ? "preview" : "complete") << ":";
    qInfo() << "  Bundled:" << bundled;
    qInfo() << "  Skipped:" << skipped;
    qInfo() << "  Failed:"  << failed;
    return (failed > 0 && bundled == 0) ? 1 : 0;
}

int handleOrganizeCommand(CliContext &ctx)
{
    const bool organizeExplicit = ctx.parser.isSet("organize");
    const bool organizeFromProcess = ctx.processRequested &&
        !ctx.processOutputPath.isEmpty() && !ctx.processHandled;
    if (!organizeExplicit && !organizeFromProcess) return 0;

    const QString destination = organizeExplicit
        ? ctx.parser.value("organize")
        : ctx.processOutputPath;
    const QString templateStr = ctx.parser.value("template");
    // When triggered from process/library, default to system subfolders if user
    // has not set --folder-naming explicitly and no preset has already set it.
    const QString folderNamingStr = !ctx.parser.value("folder-naming").isEmpty() &&
                                    ctx.parser.value("folder-naming") != QStringLiteral("none")
        ? ctx.parser.value("folder-naming")
        : (!ctx.presetFolderNaming.isEmpty()
            ? ctx.presetFolderNaming
            : (organizeFromProcess ? QStringLiteral("default") : ctx.parser.value("folder-naming")));
    const bool dryRun = ctx.parser.isSet("dry-run") || ctx.dryRunAll;
    const auto folderNaming = Constants::FolderNaming::schemeFromString(folderNamingStr);

    qInfo() << "";
    qInfo() << "=== Organize & Rename Files (M4) ===";
    qInfo() << "Destination:" << destination;
    qInfo() << "Template:"    << templateStr;
    qInfo() << "Folder naming:" << Constants::FolderNaming::schemeDisplayName(folderNaming);
    qInfo() << "Mode:"        << (dryRun ? "DRY RUN (preview only)" : "EXECUTE");
    qInfo() << "";

    OrganizeEngine organizer(ctx.db);
    organizer.setTemplate(templateStr);
    organizer.setDryRun(dryRun);
    organizer.setCollisionStrategy(CollisionStrategy::Rename);
    organizer.setFolderNaming(folderNaming);

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
    const bool hasOutput = !ctx.processOutputPath.isEmpty();
    if (!ctx.parser.isSet("generate-m3u") && !(ctx.processRequested && hasOutput)) return 0;
    if (ctx.processRequested && ctx.processHandled) return 0;

    const QString m3uDir = ctx.parser.isSet("m3u-dir")
        ? ctx.parser.value("m3u-dir")
        : (ctx.processRequested && hasOutput ? ctx.processOutputPath : QString());

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

    int count = ctx.processFileScopeIds.isEmpty()
        ? generator.generateAll(QString(), m3uDir)
        : generator.generateAll(ctx.processFileScopeIds, m3uDir);
    qInfo() << "";
    qInfo() << "Generated" << count << "M3U playlists";
    return 0;
}
