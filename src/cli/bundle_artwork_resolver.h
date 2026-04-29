#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace Remus {

namespace Metadata {
class ArtworkDownloader;
}

namespace BundleArtworkResolver {

struct ScreenshotPlan {
    QString key;
    QList<QUrl> candidates;
};

QList<QUrl> thumbnailCandidatesForSystems(const QStringList &libretroSystemNames,
                                           const QString &displayName,
                                           const QString &type);

QStringList downloadScreenshots(const QList<ScreenshotPlan> &plan,
                                 int fileId,
                                 const QString &artCacheDir,
                                 bool dryRun,
                                 Metadata::ArtworkDownloader &downloader);

} // namespace BundleArtworkResolver
} // namespace Remus
