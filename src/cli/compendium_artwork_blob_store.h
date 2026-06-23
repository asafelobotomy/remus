#pragma once

#include <QSqlDatabase>
#include <QString>

namespace CompendiumArtworkBlobStore {

struct IngestRemoteImageResult {
    QString storagePath;
    QString contentSha256;
    qint64 byteSize = 0;
    int width = 0;
    int height = 0;
    bool deduplicated = false;
};

/**
 * Download (or read local) image bytes, transcode to WebP, store in remus-thumbnails CAS.
 */
bool ingestRemoteImageToBlobStore(QSqlDatabase &database, const QString &repoRoot, const QString &thumbnailOutputDir,
    const QString &sourceUrl, bool lossless, int snapQuality, int maxWidth, IngestRemoteImageResult &result,
    QString &error);

bool upsertGameAssetFromBlob(QSqlDatabase &database, const QString &gameId, const QString &assetType,
    const QString &sourceId, const QString &sourcePath, const IngestRemoteImageResult &blob, QString &error);

} // namespace CompendiumArtworkBlobStore
