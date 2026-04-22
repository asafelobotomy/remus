#include "cli_commands.h"
#include "cli_helpers.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <QUrl>
#include "../core/rom_bundler.h"
#include "../core/constants/constants.h"
#include "../core/constants/folder_naming.h"
#include "../core/system_resolver.h"
#include "../metadata/artwork_downloader.h"
#include "../metadata/thumbnail_url_helper.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

namespace {

QList<QUrl> thumbnailCandidatesForSystems(const QStringList &libretroSystemNames,
                                          const QString &displayName,
                                          const QString &type)
{
    QList<QUrl> results;
    QSet<QString> seen;
    for (const QString &systemName : libretroSystemNames) {
        if (systemName.trimmed().isEmpty()) {
            continue;
        }
        const QStringList candidates = Metadata::ThumbnailUrlHelper::generateThumbnailCandidates(
            systemName,
            displayName,
            type);
        for (const QString &candidate : candidates) {
            const QUrl url(candidate);
            if (!url.isValid()) {
                continue;
            }
            const QString normalized = url.toString(QUrl::FullyEncoded);
            if (seen.contains(normalized)) {
                continue;
            }
            seen.insert(normalized);
            results.append(url);
        }
    }
    return results;
}

} // namespace

int handleBundleCommand(CliContext &ctx)
{
    // Trigger from --bundle directly, or from --process when output is provided
    const bool bundleExplicit = ctx.parser.isSet("bundle");
    const bool bundleFromProcess = ctx.processRequested &&
        (ctx.parser.isSet("process-output") || ctx.parser.isSet("bundle"));
    if (!bundleExplicit && !bundleFromProcess) return 0;
    if (ctx.processRequested && ctx.processHandled) return 0;

    const QString destination  = ctx.parser.isSet("bundle")
        ? ctx.parser.value("bundle")
        : ctx.parser.value("process-output");

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

    // Build system-aware filename hints up front so the orchestrator can scope
    // LocalDatabaseProvider DAT loading to only the systems being bundled.
    QMap<int, QStringList> systemFileHints;
    for (const FileRecord &file : files) {
        if (!fileMatchesProcessScope(file, ctx.processFileScopeIds)) continue;
        if (!matches.contains(file.id)) continue;

        const Database::MatchResult &match = matches.value(file.id);
        if (match.isRejected) continue;
        if (!fileMatchesSystemFilter(file, ctx.processSystemIdFilter, &match)) continue;

        const int bundledSystemId = resolveMatchedSystemId(file, &match);
        if (bundledSystemId > 0) {
            systemFileHints[bundledSystemId].append(file.filename);
        }
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
        QStringList screenshotPaths;
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

        // Resolve provider artwork metadata when we still need box art and/or screenshots.
        if (artworkPath.isEmpty() || screenshotPaths.isEmpty()) {
            if (!orchestrator)
                orchestrator = buildOrchestrator(ctx.parser, &ctx.db, systemFileHints);

            const QString displayName = getMatchingDisplayName(file);
            GameMetadata providerMeta = orchestrator->searchWithFallback(
                selectBestHash(file), displayName, systemName,
                file.crc32, file.md5, file.sha1, QString(), true);

            if (metadata.providerId.isEmpty() && !providerMeta.providerId.isEmpty())
                metadata.providerId = providerMeta.providerId;

            if (artworkPath.isEmpty() && !providerMeta.boxArtUrl.isEmpty()) {
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

            QUrl titleScreenUrl;
            QList<QUrl> gameplayUrls;
            QList<QUrl> titleCandidates;
            QList<QUrl> gameplayCandidates;

            auto appendUniqueUrl = [](QList<QUrl> &target, const QUrl &url) {
                if (!url.isValid()) {
                    return;
                }
                const QString normalized = url.toString(QUrl::FullyEncoded);
                for (const QUrl &existing : target) {
                    if (existing.toString(QUrl::FullyEncoded) == normalized) {
                        return;
                    }
                }
                target.append(url);
            };

            if (!providerMeta.id.isEmpty()) {
                const ArtworkUrls artwork = orchestrator->getArtworkWithFallback(
                    providerMeta.id, systemName, providerMeta.providerId);
                if (artwork.titleScreen.isValid()) {
                    titleScreenUrl = artwork.titleScreen;
                    appendUniqueUrl(titleCandidates, artwork.titleScreen);
                }
                if (artwork.screenshot.isValid()) {
                    gameplayUrls.append(artwork.screenshot);
                    appendUniqueUrl(gameplayCandidates, artwork.screenshot);
                }
                if (artwork.screenshot2.isValid()) {
                    gameplayUrls.append(artwork.screenshot2);
                    appendUniqueUrl(gameplayCandidates, artwork.screenshot2);
                }
            }

            for (const QString &urlString : providerMeta.screenshotUrls) {
                const QUrl screenshotUrl(urlString);
                if (screenshotUrl.isValid()) {
                    gameplayUrls.append(screenshotUrl);
                    appendUniqueUrl(gameplayCandidates, screenshotUrl);
                }
            }

            QStringList libretroSystemNames;
            QStringList thumbnailLookupNames;
            if (!metadata.title.trimmed().isEmpty()) {
                thumbnailLookupNames << metadata.title.trimmed();
            }
            if (!displayName.trimmed().isEmpty()) {
                thumbnailLookupNames << displayName.trimmed();
            }

            // Include raw ROM/container names to preserve region/revision tags
            // such as "(USA) (Rev A)" that exist on thumbnails.libretro.com.
            const QString romBaseName = QFileInfo(file.filename).completeBaseName().trimmed();
            if (!romBaseName.isEmpty()) {
                thumbnailLookupNames << romBaseName;
            }
            if (!file.archiveInternalPath.trimmed().isEmpty()) {
                const QString archiveEntryBase = QFileInfo(file.archiveInternalPath).completeBaseName().trimmed();
                if (!archiveEntryBase.isEmpty()) {
                    thumbnailLookupNames << archiveEntryBase;
                }
            }

            thumbnailLookupNames.removeAll(QString());
            thumbnailLookupNames.removeDuplicates();
            if (bundledSystemId > 0) {
                libretroSystemNames << Exports::retroArchPlaylistNameForSystemId(bundledSystemId)
                                    << SystemResolver::providerName(bundledSystemId, Providers::LOCAL_DATABASE)
                                    << systemName;
            }
            libretroSystemNames.removeAll(QString());
            libretroSystemNames.removeDuplicates();

            if (!libretroSystemNames.isEmpty()) {
                for (const QString &lookupName : thumbnailLookupNames) {
                    const QList<QUrl> libretroTitleCandidates = thumbnailCandidatesForSystems(
                        libretroSystemNames,
                        lookupName,
                        QStringLiteral("Named_Titles"));
                    for (const QUrl &url : libretroTitleCandidates) {
                        appendUniqueUrl(titleCandidates, url);
                    }

                    const QList<QUrl> libretroSnapCandidates = thumbnailCandidatesForSystems(
                        libretroSystemNames,
                        lookupName,
                        QStringLiteral("Named_Snaps"));
                    for (const QUrl &url : libretroSnapCandidates) {
                        appendUniqueUrl(gameplayCandidates, url);
                    }
                }
            }

            if (!titleScreenUrl.isValid() && !titleCandidates.isEmpty()) {
                titleScreenUrl = titleCandidates.first();
            }

            if (!titleScreenUrl.isValid() && !gameplayUrls.isEmpty()) {
                titleScreenUrl = gameplayUrls.takeFirst();
                appendUniqueUrl(titleCandidates, titleScreenUrl);
            }

            if (!gameplayUrls.isEmpty()) {
                for (const QUrl &url : gameplayUrls) {
                    appendUniqueUrl(gameplayCandidates, url);
                }
            }

            struct ScreenshotPlan {
                QString key;
                QList<QUrl> candidates;
            };
            QList<ScreenshotPlan> screenshotPlan;
            if (titleScreenUrl.isValid()) {
                if (!titleCandidates.isEmpty()) {
                    screenshotPlan.append({QStringLiteral("title"), titleCandidates});
                } else {
                    screenshotPlan.append({QStringLiteral("title"), {titleScreenUrl}});
                }
            }
            if (!gameplayCandidates.isEmpty()) {
                screenshotPlan.append({QStringLiteral("gameplay1"), gameplayCandidates});
            }
            if (gameplayCandidates.size() > 1) {
                QList<QUrl> gameplay2Candidates = gameplayCandidates;
                gameplay2Candidates.removeFirst();
                screenshotPlan.append({QStringLiteral("gameplay2"), gameplay2Candidates});
            }

            QSet<QString> downloadedUrlSet;
            for (const ScreenshotPlan &shot : screenshotPlan) {
                bool slotDownloaded = false;
                for (const QUrl &candidateUrl : shot.candidates) {
                    if (!candidateUrl.isValid()) {
                        continue;
                    }

                    const QString normalizedUrl = candidateUrl.toString(QUrl::FullyEncoded);
                    if (downloadedUrlSet.contains(normalizedUrl)) {
                        continue;
                    }

                    const QString ext = QFileInfo(candidateUrl.path()).suffix().isEmpty()
                        ? QStringLiteral("jpg")
                        : QFileInfo(candidateUrl.path()).suffix().toLower();
                    const QString artCacheDir = ctx.processArtworkCacheDir.isEmpty()
                        ? tempArtworkDir.path()
                        : ctx.processArtworkCacheDir;
                    const QString destPath = artCacheDir + "/" +
                        QString::number(file.id) + "_" + shot.key + "." + ext;

                    if (dryRun) {
                        qInfo() << "  [DRY-RUN] Would download" << shot.key << "screenshot from:" << candidateUrl;
                        screenshotPaths.append(destPath);
                        downloadedUrlSet.insert(normalizedUrl);
                        slotDownloaded = true;
                        break;
                    }

                    QString savedPath;
                    if (downloader.download(candidateUrl, destPath, &savedPath)) {
                        const QString resolvedPath = savedPath.isEmpty() ? destPath : savedPath;
                        screenshotPaths.append(resolvedPath);
                        downloadedUrlSet.insert(normalizedUrl);
                        qInfo() << "  ✓ Downloaded" << shot.key << "screenshot:" << resolvedPath;
                        slotDownloaded = true;
                        break;
                    }

                    qWarning() << "  ⚠ Failed to download" << shot.key << "screenshot from:" << candidateUrl;
                }

                if (!slotDownloaded) {
                    qWarning() << "  ⚠ No valid" << shot.key << "screenshot candidates succeeded";
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
            /*includeScreenshots*/ true,
            /*dryRun*/        dryRun,
            /*outputFormat*/  fmt,
            /*artworkPath*/   artworkPath,
            /*screenshotPaths*/ screenshotPaths,
            /*discOutputFormat*/ discOutputFormat
        };

        qInfo() << "Bundling:" << file.filename;

        RomBundler::BundleResult result = bundler.bundle(
            file, match, metadata, effectiveDestination, config);

        if (result.skippedAlreadyBundled) {
            qInfo() << "  ↷ Skipped (already bundled)";
            skipped++;
        } else if (result.success) {
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
