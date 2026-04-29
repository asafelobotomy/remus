#include "bundle_artwork_resolver.h"
#include "../metadata/artwork_downloader.h"
#include "../metadata/thumbnail_url_helper.h"

#include <QDebug>
#include <QFileInfo>
#include <QSet>

namespace Remus {
namespace BundleArtworkResolver {

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

QStringList downloadScreenshots(const QList<ScreenshotPlan> &plan,
                                 int fileId,
                                 const QString &artCacheDir,
                                 bool dryRun,
                                 Metadata::ArtworkDownloader &downloader)
{
    QStringList screenshotPaths;
    QSet<QString> downloadedUrlSet;

    for (const ScreenshotPlan &shot : plan) {
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
            const QString destPath = artCacheDir + "/" +
                QString::number(fileId) + "_" + shot.key + "." + ext;

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

    return screenshotPaths;
}

} // namespace BundleArtworkResolver
} // namespace Remus
