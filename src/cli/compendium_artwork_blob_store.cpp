#include "compendium_artwork_blob_store.h"
#include "compendium_artwork_transcode.h"

#include "../metadata/artwork_downloader.h"
#include "../metadata/thumbnail_url_helper.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>

namespace {

bool transcodeBytesToWebp(const QByteArray &inputBytes, const QString &destPath, const QString &repoRoot, bool lossless,
    int snapQuality, int maxWidth, int &outWidth, int &outHeight, QString &error) {
    QImage image;
    if (!image.loadFromData(inputBytes)) {
        error = QStringLiteral("Failed to decode image bytes");
        return false;
    }
    if (maxWidth > 0 && image.width() > maxWidth) {
        image = image.scaledToWidth(maxWidth, Qt::SmoothTransformation);
    }
    outWidth = image.width();
    outHeight = image.height();

    return CompendiumArtworkTranscode::transcodeImageToWebp(image, destPath, repoRoot, lossless, snapQuality, error);
}

QString repoRelativeBlobPath(const QString &sha256) {
    return QStringLiteral("data/remus-thumbnails/blobs/%1/%2/%3.webp").arg(sha256.left(2), sha256.mid(2, 2), sha256);
}

} // namespace

namespace CompendiumArtworkBlobStore {

bool ingestRemoteImageToBlobStore(QSqlDatabase &database, const QString &repoRoot, const QString &thumbnailOutputDir,
    const QString &sourceUrl, bool lossless, int snapQuality, int maxWidth, IngestRemoteImageResult &result,
    QString &error) {
    Q_UNUSED(database);
    QByteArray bytes;
    const QUrl url(sourceUrl);
    if (url.isLocalFile() || sourceUrl.startsWith(QStringLiteral("data/"))) {
        const QString path = url.isLocalFile() ? url.toLocalFile() : sourceUrl;
        const QString resolved = Remus::Metadata::ThumbnailUrlHelper::resolveStoragePath(repoRoot, path);
        QFile file(resolved);
        if (!file.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("Failed to read local image: %1").arg(resolved);
            return false;
        }
        bytes = file.readAll();
    } else {
        Remus::ArtworkDownloader downloader;
        bytes = downloader.downloadToMemory(url);
        if (bytes.isEmpty()) {
            error = QStringLiteral("Failed to download image: %1").arg(sourceUrl);
            return false;
        }
    }

    QDir outputDir(thumbnailOutputDir);
    outputDir.mkpath(QStringLiteral("blobs"));
    const QString tempWebp = outputDir.filePath(QStringLiteral("blobs/.tmp_ingest.webp"));
    int width = 0;
    int height = 0;
    if (!transcodeBytesToWebp(bytes, tempWebp, repoRoot, lossless, snapQuality, maxWidth, width, height, error)) {
        return false;
    }

    QFile tempFile(tempWebp);
    if (!tempFile.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Failed to read transcoded blob");
        return false;
    }
    const QByteArray webpBytes = tempFile.readAll();
    tempFile.close();

    const QString sha256 = QString::fromLatin1(QCryptographicHash::hash(webpBytes, QCryptographicHash::Sha256).toHex());
    const QString storagePath = repoRelativeBlobPath(sha256);
    const QString blobAbs
        = outputDir.filePath(QStringLiteral("blobs/%1/%2/%3.webp").arg(sha256.left(2), sha256.mid(2, 2), sha256));

    result.deduplicated = QFileInfo::exists(blobAbs);
    if (!result.deduplicated) {
        QDir().mkpath(QFileInfo(blobAbs).absolutePath());
        if (!QFile::rename(tempWebp, blobAbs)) {
            if (!QFile::copy(tempWebp, blobAbs)) {
                QFile::remove(tempWebp);
                error = QStringLiteral("Failed to install blob");
                return false;
            }
            QFile::remove(tempWebp);
        }
    } else {
        QFile::remove(tempWebp);
    }

    result.storagePath = storagePath;
    result.contentSha256 = sha256;
    result.byteSize = webpBytes.size();
    result.width = width;
    result.height = height;

    QSqlQuery invQ(database);
    invQ.prepare(QStringLiteral("INSERT INTO blob_inventory (content_sha256, storage_path, mime_type, byte_size, "
                                "ref_count) VALUES (?, ?, 'image/webp', ?, 0) "
                                "ON CONFLICT(content_sha256) DO UPDATE SET byte_size = excluded.byte_size"));
    invQ.addBindValue(sha256);
    invQ.addBindValue(storagePath);
    invQ.addBindValue(result.byteSize);
    if (!invQ.exec()) {
        error = invQ.lastError().text();
        return false;
    }

    return true;
}

bool upsertGameAssetFromBlob(QSqlDatabase &database, const QString &gameId, const QString &assetType,
    const QString &sourceId, const QString &sourcePath, const IngestRemoteImageResult &blob, QString &error) {
    QSqlQuery q(database);
    q.prepare(QStringLiteral("INSERT INTO game_assets (game_id, asset_type, storage_path, content_sha256, byte_size, "
                             "width, height, mime_type, source_id, source_path, snapshot_id, confidence) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, 'image/webp', ?, ?, '', 0.9) "
                             "ON CONFLICT(game_id, asset_type) DO UPDATE SET "
                             "storage_path = excluded.storage_path, "
                             "content_sha256 = excluded.content_sha256, "
                             "byte_size = excluded.byte_size, "
                             "width = excluded.width, "
                             "height = excluded.height, "
                             "source_id = excluded.source_id, "
                             "source_path = excluded.source_path"));
    q.addBindValue(gameId);
    q.addBindValue(assetType);
    q.addBindValue(blob.storagePath);
    q.addBindValue(blob.contentSha256);
    q.addBindValue(blob.byteSize);
    q.addBindValue(blob.width);
    q.addBindValue(blob.height);
    q.addBindValue(sourceId);
    q.addBindValue(sourcePath);
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    return true;
}

} // namespace CompendiumArtworkBlobStore
