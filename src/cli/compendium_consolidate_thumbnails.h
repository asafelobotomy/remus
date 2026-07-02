#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include <functional>

using ConsolidateThumbnailsProgressCallback
    = std::function<void(int gamesScanned, int totalGames, const QString &detail)>;

struct ConsolidateThumbnailsOptions {
    QString acquisitionDir;
    QString outputDir;
    QStringList systems;
    QString format = QStringLiteral("webp");
    int snapQuality = 85;
    bool snapLossless = false;
    int maxWidth = 512;
    bool dryRun = false;
    bool pruneAcquisitionSources = false;
    ConsolidateThumbnailsProgressCallback onProgress;
};

struct ConsolidateThumbnailsStats {
    int gamesScanned = 0;
    int assetsWritten = 0;
    int assetsSkipped = 0;
    int assetsDeduplicated = 0;
    int misses = 0;
    qint64 bytesTotal = 0;
    QStringList prunedSystems;
};

namespace CompendiumThumbnails {

bool consolidateThumbnails(QSqlDatabase &database, const ConsolidateThumbnailsOptions &options,
    ConsolidateThumbnailsStats &stats, QString &error);

bool gcThumbnailBlobs(QSqlDatabase &database, const QString &thumbnailRoot, const QString &repoRoot, bool dryRun,
    int &orphansRemoved, QString &error);

bool exportRetroArchArtwork(QSqlDatabase &database, const QString &thumbnailRoot, const QString &repoRoot,
    const QString &exportDir, const QStringList &systems, int &filesExported, QString &error);

bool remusThumbnailsManifestExists(const QString &outputDir);

} // namespace CompendiumThumbnails
