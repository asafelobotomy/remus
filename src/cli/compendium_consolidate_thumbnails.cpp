#include "compendium_consolidate_thumbnails.h"
#include "cli_compendium_build_phases.h"
#include "compendium_artwork_transcode.h"
#include "compendium_cwebp_resolver.h"
#include "compendium_enrichment_sql.h"

#include "../metadata/thumbnail_url_helper.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <fcntl.h>
#include <unistd.h>

using Remus::Metadata::ThumbnailUrlHelper;

namespace {

class LibpngStderrSuppressScope {
public:
    LibpngStderrSuppressScope() {
        fflush(stderr);
        savedFd_ = dup(STDERR_FILENO);
        if (savedFd_ < 0) {
            return;
        }
        const int nullFd = open("/dev/null", O_WRONLY);
        if (nullFd >= 0) {
            dup2(nullFd, STDERR_FILENO);
            close(nullFd);
        }
    }

    ~LibpngStderrSuppressScope() {
        if (savedFd_ < 0) {
            return;
        }
        fflush(stderr);
        dup2(savedFd_, STDERR_FILENO);
        close(savedFd_);
    }

private:
    int savedFd_ = -1;
};

QString libretroFolderForAssetType(const QString &assetType) {
    if (assetType == QStringLiteral("box")) {
        return QStringLiteral("Named_Boxarts");
    }
    if (assetType == QStringLiteral("snap")) {
        return QStringLiteral("Named_Snaps");
    }
    if (assetType == QStringLiteral("title")) {
        return QStringLiteral("Named_Titles");
    }
    if (assetType == QStringLiteral("logo")) {
        return QStringLiteral("Named_Logos");
    }
    return { };
}

QStringList assetTypesInOrder() {
    return {
        QStringLiteral("box"),
        QStringLiteral("snap"),
        QStringLiteral("title"),
        QStringLiteral("logo"),
    };
}

QStringList candidateGameNames(const QString &canonicalTitle) {
    QStringList names;
    QSet<QString> seen;
    auto add = [&](const QString &name) {
        const QString trimmed = name.trimmed();
        if (trimmed.isEmpty() || seen.contains(trimmed)) {
            return;
        }
        seen.insert(trimmed);
        names.append(trimmed);
    };
    add(canonicalTitle);
    const QString stripped = ThumbnailUrlHelper::stripLanguageTags(canonicalTitle);
    if (stripped != canonicalTitle) {
        add(stripped);
    }
    return names;
}

QString findAcquisitionPng(
    const QString &acquisitionDir, const QString &libretroName, const QString &folder, const QString &canonicalTitle) {
    const QDir base(acquisitionDir);
    for (const QString &name : candidateGameNames(canonicalTitle)) {
        const QString sanitized = ThumbnailUrlHelper::sanitizeThumbnailName(name);
        const QString rel
            = libretroName + QLatin1Char('/') + folder + QLatin1Char('/') + sanitized + QStringLiteral(".png");
        const QString abs = base.filePath(rel);
        if (QFileInfo::exists(abs)) {
            return abs;
        }
    }
    return { };
}

bool webpWriterAvailable() {
    static const bool available = QImageWriter::supportedImageFormats().contains("webp");
    return available;
}

bool cwebpAvailable(const QString &repoRoot) {
    return CompendiumCwebp::isAvailable(repoRoot);
}

QString desiredSnapPolicy(const ConsolidateThumbnailsOptions &options) {
    if (options.snapLossless) {
        return QStringLiteral("webp_lossless");
    }
    return QStringLiteral("webp_lossy_q%1").arg(qBound(1, options.snapQuality, 100));
}

bool isLosslessForAssetType(const QString &assetType, const ConsolidateThumbnailsOptions &options) {
    if (assetType == QStringLiteral("snap")) {
        return options.snapLossless;
    }
    return true;
}

QString sourceReuseKey(
    const QString &relSource, const QString &assetType, const ConsolidateThumbnailsOptions &options) {
    if (assetType == QStringLiteral("snap")) {
        return relSource + QLatin1Char('|') + desiredSnapPolicy(options);
    }
    return relSource;
}

QString readManifestSnapPolicy(const QString &outputDir) {
    const QString manifestPath = QDir(outputDir).filePath(QStringLiteral("manifest.json"));
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return { };
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return { };
    }
    return doc.object().value(QStringLiteral("transcode")).toObject().value(QStringLiteral("snap_policy")).toString();
}

struct TranscodeFormat {
    QString extension;
    QString mimeType;
};

bool storedAssetPolicyMatches(const QString &assetType, const QString &storedMime, const QString &storedSnapPolicy,
    const TranscodeFormat &targetFormat, const ConsolidateThumbnailsOptions &options) {
    if (CompendiumArtworkTranscode::storedAssetNeedsUpgrade(storedMime, targetFormat.mimeType)) {
        return false;
    }
    if (assetType != QStringLiteral("snap")) {
        return true;
    }
    const QString effectivePolicy = storedSnapPolicy.isEmpty() ? QStringLiteral("webp_lossy_q85") : storedSnapPolicy;
    return effectivePolicy == desiredSnapPolicy(options);
}

TranscodeFormat resolveTranscodeFormat(
    const ConsolidateThumbnailsOptions &options, const QString &repoRoot, bool &autoFallback) {
    autoFallback = false;
    if (options.format.compare(QStringLiteral("png"), Qt::CaseInsensitive) == 0) {
        return { QStringLiteral("png"), QStringLiteral("image/png") };
    }
    if (webpWriterAvailable() || cwebpAvailable(repoRoot)) {
        return { QStringLiteral("webp"), QStringLiteral("image/webp") };
    }
    autoFallback = true;
    return { QStringLiteral("png"), QStringLiteral("image/png") };
}

bool transcodeToPng(const QImage &image, const QString &destPath, QString &error) {
    QImageWriter writer(destPath, "png");
    CompendiumArtworkTranscode::configureLosslessPngWriter(writer);
    if (!writer.write(image)) {
        error = QStringLiteral("Failed to write PNG blob: %1").arg(destPath);
        return false;
    }
    return true;
}

bool transcodeImage(const QString &srcPath, const QString &destPath, const TranscodeFormat &format,
    const QString &repoRoot, bool lossless, int snapQuality, int maxWidth, int &outWidth, int &outHeight,
    QString &error) {
    QImage image;
    {
        const LibpngStderrSuppressScope suppressLibpng;
        image = QImage(srcPath);
    }
    if (image.isNull()) {
        error = QStringLiteral("Failed to load image: %1").arg(srcPath);
        return false;
    }
    if (maxWidth > 0 && image.width() > maxWidth) {
        image = image.scaledToWidth(maxWidth, Qt::SmoothTransformation);
    }
    outWidth = image.width();
    outHeight = image.height();

    if (format.extension == QStringLiteral("png")) {
        return transcodeToPng(image, destPath, error);
    }
    return CompendiumArtworkTranscode::transcodeImageToWebp(image, destPath, repoRoot, lossless, snapQuality, error);
}

struct ReusableSourceAsset {
    QString storagePath;
    QString contentSha256;
    qint64 byteSize = 0;
    int width = 0;
    int height = 0;
    QString mimeType;
};

bool lookupReusableSourceAsset(QSqlDatabase &database, const QString &repoRoot, const QString &relSource,
    const QString &assetType, const TranscodeFormat &targetFormat, const ConsolidateThumbnailsOptions &options,
    const QHash<QString, ReusableSourceAsset> &runCache, ReusableSourceAsset &asset) {
    const QString cacheKey = sourceReuseKey(relSource, assetType, options);
    if (relSource.isEmpty()) {
        return false;
    }

    const auto cacheIt = runCache.constFind(cacheKey);
    if (cacheIt != runCache.constEnd()) {
        asset = cacheIt.value();
        const QString absPath = ThumbnailUrlHelper::resolveStoragePath(repoRoot, asset.storagePath);
        return QFileInfo::exists(absPath);
    }

    if (assetType == QStringLiteral("snap")) {
        return false;
    }

    QSqlQuery q(database);
    q.prepare(QStringLiteral("SELECT storage_path, content_sha256, byte_size, width, height, mime_type "
                             "FROM game_assets WHERE source_path = ? AND mime_type = ? LIMIT 1"));
    q.addBindValue(relSource);
    q.addBindValue(targetFormat.mimeType);
    if (!q.exec() || !q.next()) {
        return false;
    }

    asset.storagePath = q.value(0).toString();
    asset.contentSha256 = q.value(1).toString();
    asset.byteSize = q.value(2).toLongLong();
    asset.width = q.value(3).toInt();
    asset.height = q.value(4).toInt();
    asset.mimeType = q.value(5).toString();

    const QString absPath = ThumbnailUrlHelper::resolveStoragePath(repoRoot, asset.storagePath);
    return QFileInfo::exists(absPath);
}

QString repoRelativeBlobPath(const QString &sha256, const QString &extension) {
    return QStringLiteral("data/remus-thumbnails/blobs/%1/%2/%3.%4")
        .arg(sha256.left(2), sha256.mid(2, 2), sha256, extension);
}

bool upsertBlobInventory(QSqlDatabase &db, const QString &sha256, const QString &storagePath, const QString &mimeType,
    qint64 byteSize, QString &error) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO blob_inventory (content_sha256, storage_path, mime_type, byte_size, "
                             "ref_count) "
                             "VALUES (?, ?, ?, ?, 0) "
                             "ON CONFLICT(content_sha256) DO UPDATE SET "
                             "storage_path = excluded.storage_path, "
                             "mime_type = excluded.mime_type, "
                             "byte_size = excluded.byte_size"));
    q.addBindValue(sha256);
    q.addBindValue(storagePath);
    q.addBindValue(mimeType);
    q.addBindValue(byteSize);
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    return true;
}

bool upsertGameAsset(QSqlDatabase &db, const QString &gameId, const QString &assetType, const QString &storagePath,
    const QString &sha256, qint64 byteSize, int width, int height, const QString &mimeType, const QString &sourcePath,
    QString &error) {
    QSqlQuery q(db);
    q.prepare(QStringLiteral("INSERT INTO game_assets (game_id, asset_type, storage_path, content_sha256, byte_size, "
                             "width, height, mime_type, source_id, source_path, snapshot_id, confidence) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'remus-thumbnails', ?, '', 1.0) "
                             "ON CONFLICT(game_id, asset_type) DO UPDATE SET "
                             "storage_path = excluded.storage_path, "
                             "content_sha256 = excluded.content_sha256, "
                             "byte_size = excluded.byte_size, "
                             "width = excluded.width, "
                             "height = excluded.height, "
                             "mime_type = excluded.mime_type, "
                             "source_path = excluded.source_path"));
    q.addBindValue(gameId);
    q.addBindValue(assetType);
    q.addBindValue(storagePath);
    q.addBindValue(sha256);
    q.addBindValue(byteSize);
    q.addBindValue(width);
    q.addBindValue(height);
    q.addBindValue(mimeType);
    q.addBindValue(sourcePath);
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    return true;
}

bool installReusableAsset(QSqlDatabase &database, const QString &gameId, const QString &assetType,
    const ReusableSourceAsset &asset, const QString &relSource, const TranscodeFormat &format, QString &error) {
    if (!upsertBlobInventory(
            database, asset.contentSha256, asset.storagePath, format.mimeType, asset.byteSize, error)) {
        return false;
    }
    return upsertGameAsset(database, gameId, assetType, asset.storagePath, asset.contentSha256, asset.byteSize,
        asset.width, asset.height, format.mimeType, relSource, error);
}

bool refreshBlobRefCounts(QSqlDatabase &db, QString &error) {
    QSqlQuery zeroQ(db);
    if (!zeroQ.exec(QStringLiteral("UPDATE blob_inventory SET ref_count = 0"))) {
        error = zeroQ.lastError().text();
        return false;
    }
    QSqlQuery countQ(db);
    if (!countQ.exec(QStringLiteral("SELECT content_sha256, COUNT(*) FROM game_assets GROUP BY content_sha256"))) {
        error = countQ.lastError().text();
        return false;
    }
    QSqlQuery updateQ(db);
    updateQ.prepare(QStringLiteral("UPDATE blob_inventory SET ref_count = ? WHERE content_sha256 = ?"));
    while (countQ.next()) {
        updateQ.bindValue(0, countQ.value(1));
        updateQ.bindValue(1, countQ.value(0));
        if (!updateQ.exec()) {
            error = updateQ.lastError().text();
            return false;
        }
    }
    return true;
}

bool writeManifest(const QString &outputDir, const ConsolidateThumbnailsOptions &options,
    const ConsolidateThumbnailsStats &stats, const QStringList &systemsSynced) {
    QJsonObject manifest {
        { QStringLiteral("version"), 1 },
        { QStringLiteral("build_id"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
        { QStringLiteral("sources"),
            QJsonObject {
                { QStringLiteral("libretro-thumbnails"),
                    QJsonObject {
                        { QStringLiteral("systems_synced"), QJsonArray::fromStringList(systemsSynced) },
                    } },
            } },
        { QStringLiteral("transcode"),
            QJsonObject {
                { QStringLiteral("box_policy"), QStringLiteral("webp_lossless") },
                { QStringLiteral("snap_policy"), desiredSnapPolicy(options) },
                { QStringLiteral("title_policy"), QStringLiteral("webp_lossless") },
                { QStringLiteral("logo_policy"), QStringLiteral("webp_lossless") },
                { QStringLiteral("max_width"), options.maxWidth },
            } },
        { QStringLiteral("stats"),
            QJsonObject {
                { QStringLiteral("games_scanned"), stats.gamesScanned },
                { QStringLiteral("assets_written"), stats.assetsWritten },
                { QStringLiteral("assets_skipped"), stats.assetsSkipped },
                { QStringLiteral("assets_deduplicated"), stats.assetsDeduplicated },
                { QStringLiteral("bytes_total"), static_cast<double>(stats.bytesTotal) },
                { QStringLiteral("misses"), stats.misses },
            } },
        { QStringLiteral("acquisition_pruned"), QJsonArray::fromStringList(stats.prunedSystems) },
    };

    const QString manifestPath = QDir(outputDir).filePath(QStringLiteral("manifest.json"));
    QFile file(manifestPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    bumpRemusThumbnailsFingerprintSidecar(outputDir);
    return true;
}

} // namespace

namespace CompendiumThumbnails {

bool remusThumbnailsManifestExists(const QString &outputDir) {
    return QFileInfo::exists(QDir(outputDir).filePath(QStringLiteral("manifest.json")));
}

bool consolidateThumbnails(QSqlDatabase &database, const ConsolidateThumbnailsOptions &options,
    ConsolidateThumbnailsStats &stats, QString &error) {
    if (!database.isOpen()) {
        error = QStringLiteral("Database is not open");
        return false;
    }

    if (!QFileInfo::exists(options.acquisitionDir)) {
        error = QStringLiteral("Acquisition directory not found: %1").arg(options.acquisitionDir);
        return false;
    }

    QDir outputDir(options.outputDir);
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral("."))) {
        error = QStringLiteral("Failed to create output directory: %1").arg(options.outputDir);
        return false;
    }
    QDir(outputDir.filePath(QStringLiteral("blobs"))).mkpath(QStringLiteral("."));

    const QString repoRoot = QDir::cleanPath(QDir(options.outputDir).absoluteFilePath(QStringLiteral("../..")));

    QString systemFilterSql;
    if (!options.systems.isEmpty()) {
        QStringList quoted;
        for (const QString &sys : options.systems) {
            quoted.append(QStringLiteral("'%1'").arg(QString(sys).replace(QLatin1Char('\''), QStringLiteral("''"))));
        }
        systemFilterSql = QStringLiteral(" AND s.libretro_name IN (%1)").arg(quoted.join(QLatin1Char(',')));
    }

    QSqlQuery gamesQ(database);
    const QString gamesSql = QStringLiteral("SELECT g.game_id, g.canonical_title, s.libretro_name "
                                            "FROM games g "
                                            "JOIN systems s ON s.system_id = g.system_id "
                                            "WHERE s.libretro_name IS NOT NULL AND TRIM(s.libretro_name) != ''")
        + systemFilterSql;

    int totalGames = 0;
    {
        QSqlQuery countQ(database);
        const QString countSql = QStringLiteral("SELECT COUNT(*) FROM games g "
                                                "JOIN systems s ON s.system_id = g.system_id "
                                                "WHERE s.libretro_name IS NOT NULL AND TRIM(s.libretro_name) != ''")
            + systemFilterSql;
        if (countQ.exec(countSql) && countQ.next())
            totalGames = countQ.value(0).toInt();
    }

    if (!gamesQ.exec(gamesSql)) {
        error = gamesQ.lastError().text();
        return false;
    }

    QStringList systemsSeen;
    QSet<QString> systemsSeenSet;
    QHash<QString, ReusableSourceAsset> sourceAssetCache;

    bool formatAutoFallback = false;
    const TranscodeFormat format = resolveTranscodeFormat(options, repoRoot, formatAutoFallback);
    const QString storedSnapPolicy = readManifestSnapPolicy(options.outputDir);
    if (formatAutoFallback) {
        qWarning().noquote() << QStringLiteral("[consolidate-thumbnails] WebP backend unavailable (run npm install for "
                                               "cwebp-bin or qt imageformats-webp); "
                                               "using PNG blobs for this run");
    }

    if (!database.transaction()) {
        error = database.lastError().text();
        return false;
    }

    if (options.onProgress) {
        options.onProgress(0, totalGames, QStringLiteral("starting thumbnail consolidation"));
    }

    while (gamesQ.next()) {
        const QString gameId = gamesQ.value(0).toString();
        const QString title = gamesQ.value(1).toString();
        const QString libretroName = gamesQ.value(2).toString();
        if (gameId.isEmpty() || title.isEmpty() || libretroName.isEmpty()) {
            continue;
        }

        ++stats.gamesScanned;
        if (options.onProgress && (stats.gamesScanned % 500 == 0 || stats.gamesScanned == totalGames)) {
            options.onProgress(stats.gamesScanned, totalGames,
                QStringLiteral("%1 written, %2 skipped, %3 deduped")
                    .arg(stats.assetsWritten)
                    .arg(stats.assetsSkipped)
                    .arg(stats.assetsDeduplicated));
        }
        if (!systemsSeenSet.contains(libretroName)) {
            systemsSeenSet.insert(libretroName);
            systemsSeen.append(libretroName);
        }

        QString boxStoragePath;

        for (const QString &assetType : assetTypesInOrder()) {
            const QString folder = libretroFolderForAssetType(assetType);
            if (folder.isEmpty()) {
                continue;
            }

            const QString srcPath = findAcquisitionPng(options.acquisitionDir, libretroName, folder, title);
            if (srcPath.isEmpty()) {
                ++stats.misses;
                continue;
            }

            const bool lossless = isLosslessForAssetType(assetType, options);
            const QString relSource = QDir(options.acquisitionDir).relativeFilePath(srcPath);

            QSqlQuery existingQ(database);
            existingQ.prepare(QStringLiteral("SELECT storage_path, mime_type FROM game_assets "
                                             "WHERE game_id = ? AND asset_type = ?"));
            existingQ.addBindValue(gameId);
            existingQ.addBindValue(assetType);
            if (existingQ.exec() && existingQ.next()) {
                const QString existingPath = existingQ.value(0).toString();
                const QString existingMime = existingQ.value(1).toString();
                const QString absExisting = ThumbnailUrlHelper::resolveStoragePath(repoRoot, existingPath);
                if (QFileInfo::exists(absExisting)
                    && storedAssetPolicyMatches(assetType, existingMime, storedSnapPolicy, format, options)) {
                    ++stats.assetsSkipped;
                    if (assetType == QStringLiteral("box")) {
                        boxStoragePath = existingPath;
                    }
                    continue;
                }
            }

            ReusableSourceAsset reusable;
            if (lookupReusableSourceAsset(
                    database, repoRoot, relSource, assetType, format, options, sourceAssetCache, reusable)) {
                if (!options.dryRun) {
                    if (!installReusableAsset(database, gameId, assetType, reusable, relSource, format, error)) {
                        database.rollback();
                        return false;
                    }
                }
                sourceAssetCache.insert(sourceReuseKey(relSource, assetType, options), reusable);
                ++stats.assetsDeduplicated;
                if (assetType == QStringLiteral("box")) {
                    boxStoragePath = reusable.storagePath;
                }
                continue;
            }

            if (options.dryRun) {
                ++stats.assetsWritten;
                if (assetType == QStringLiteral("box")) {
                    boxStoragePath = QStringLiteral("dry-run");
                }
                continue;
            }

            const QString tempBlob = QDir(outputDir).filePath(
                QStringLiteral("blobs/.tmp_%1_%2.%3").arg(gameId, assetType, format.extension));
            int width = 0;
            int height = 0;
            if (!transcodeImage(srcPath, tempBlob, format, repoRoot, lossless, options.snapQuality, options.maxWidth,
                    width, height, error)) {
                qWarning().noquote() << QStringLiteral("[consolidate-thumbnails] Skipping unloadable source: %1 (%2)")
                                            .arg(srcPath, error);
                ++stats.misses;
                continue;
            }

            QFile tempFile(tempBlob);
            if (!tempFile.open(QIODevice::ReadOnly)) {
                error = QStringLiteral("Failed to read transcoded file: %1").arg(tempBlob);
                database.rollback();
                return false;
            }
            const QByteArray bytes = tempFile.readAll();
            tempFile.close();

            const QString sha256
                = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
            const QString storagePath = repoRelativeBlobPath(sha256, format.extension);
            const QString blobAbs = QDir(options.outputDir)
                                        .absoluteFilePath(QStringLiteral("blobs/%1/%2/%3.%4")
                                                .arg(sha256.left(2), sha256.mid(2, 2), sha256, format.extension));

            const bool dedupHit = QFileInfo::exists(blobAbs);
            if (!dedupHit) {
                QDir().mkpath(QFileInfo(blobAbs).absolutePath());
                if (!QFile::rename(tempBlob, blobAbs)) {
                    if (!QFile::copy(tempBlob, blobAbs)) {
                        error = QStringLiteral("Failed to install blob: %1").arg(blobAbs);
                        QFile::remove(tempBlob);
                        database.rollback();
                        return false;
                    }
                    QFile::remove(tempBlob);
                }
                ++stats.assetsWritten;
                stats.bytesTotal += bytes.size();
            } else {
                QFile::remove(tempBlob);
                ++stats.assetsDeduplicated;
            }

            if (!upsertBlobInventory(database, sha256, storagePath, format.mimeType, bytes.size(), error)) {
                database.rollback();
                return false;
            }
            if (!upsertGameAsset(database, gameId, assetType, storagePath, sha256, bytes.size(), width, height,
                    format.mimeType, relSource, error)) {
                database.rollback();
                return false;
            }

            sourceAssetCache.insert(sourceReuseKey(relSource, assetType, options),
                ReusableSourceAsset { storagePath, sha256, bytes.size(), width, height, format.mimeType });

            if (assetType == QStringLiteral("box")) {
                boxStoragePath = storagePath;
            }
        }

        if (!boxStoragePath.isEmpty() && !options.dryRun && boxStoragePath != QStringLiteral("dry-run")) {
            QSqlQuery coverQ(database);
            coverQ.prepare(QStringLiteral("UPDATE games SET cover_url = ? WHERE game_id = ? "
                                          "AND (cover_url IS NULL OR TRIM(cover_url) = '' "
                                          "OR cover_url LIKE 'https://%' "
                                          "OR cover_url LIKE 'http://%')"));
            coverQ.addBindValue(boxStoragePath);
            coverQ.addBindValue(gameId);
            if (!coverQ.exec()) {
                error = coverQ.lastError().text();
                database.rollback();
                return false;
            }
        }
    }

    if (!refreshBlobRefCounts(database, error)) {
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        error = database.lastError().text();
        return false;
    }

    if (!options.dryRun) {
        if (!writeManifest(options.outputDir, options, stats, systemsSeen)) {
            qWarning() << "Failed to write remus-thumbnails manifest.json";
        }
    }

    if (options.pruneAcquisitionSources && !options.dryRun) {
        for (const QString &system : systemsSeen) {
            const QString systemDir = QDir(options.acquisitionDir).filePath(system);
            if (QDir(systemDir).exists()) {
                QDir(systemDir).removeRecursively();
                QFile::remove(systemDir + QStringLiteral(".remus-sync-sha256"));
                stats.prunedSystems.append(system);
            }
        }
    }

    return true;
}

bool gcThumbnailBlobs(QSqlDatabase &database, const QString &thumbnailRoot, const QString &repoRoot, bool dryRun,
    int &orphansRemoved, QString &error) {
    QSqlQuery q(database);
    if (!q.exec(QStringLiteral("SELECT content_sha256, storage_path FROM blob_inventory "
                               "WHERE content_sha256 NOT IN (SELECT DISTINCT content_sha256 FROM game_assets)"))) {
        error = q.lastError().text();
        return false;
    }

    while (q.next()) {
        const QString sha = q.value(0).toString();
        const QString storagePath = q.value(1).toString();
        QString absPath = ThumbnailUrlHelper::resolveStoragePath(repoRoot, storagePath);
        if (!QFileInfo::exists(absPath) && storagePath.isEmpty()) {
            absPath = QDir(thumbnailRoot)
                          .filePath(QStringLiteral("blobs/%1/%2/%3.webp").arg(sha.left(2), sha.mid(2, 2), sha));
        }
        if (QFileInfo::exists(absPath) && !dryRun) {
            QFile::remove(absPath);
        }
        if (!dryRun) {
            QSqlQuery delQ(database);
            delQ.prepare(QStringLiteral("DELETE FROM blob_inventory WHERE content_sha256 = ?"));
            delQ.addBindValue(sha);
            if (!delQ.exec()) {
                error = delQ.lastError().text();
                return false;
            }
        }
        ++orphansRemoved;
    }
    return true;
}

bool exportRetroArchArtwork(QSqlDatabase &database, const QString &thumbnailRoot, const QString &repoRoot,
    const QString &exportDir, const QStringList &systems, int &filesExported, QString &error) {
    Q_UNUSED(thumbnailRoot);
    QString systemFilterSql;
    if (!systems.isEmpty()) {
        QStringList quoted;
        for (const QString &sys : systems) {
            quoted.append(QStringLiteral("'%1'").arg(QString(sys).replace(QLatin1Char('\''), QStringLiteral("''"))));
        }
        systemFilterSql = QStringLiteral(" AND s.libretro_name IN (%1)").arg(quoted.join(QLatin1Char(',')));
    }

    QSqlQuery q(database);
    const QString sql = QStringLiteral("SELECT g.canonical_title, s.libretro_name, ga.asset_type, ga.storage_path "
                                       "FROM game_assets ga "
                                       "JOIN games g ON g.game_id = ga.game_id "
                                       "JOIN systems s ON s.system_id = g.system_id "
                                       "WHERE 1=1")
        + systemFilterSql;
    if (!q.exec(sql)) {
        error = q.lastError().text();
        return false;
    }

    while (q.next()) {
        const QString title = q.value(0).toString();
        const QString libretroName = q.value(1).toString();
        const QString assetType = q.value(2).toString();
        const QString storagePath = q.value(3).toString();
        const QString folder = libretroFolderForAssetType(assetType);
        if (folder.isEmpty()) {
            continue;
        }

        const QString srcAbs = ThumbnailUrlHelper::resolveStoragePath(repoRoot, storagePath);
        if (!QFileInfo::exists(srcAbs)) {
            continue;
        }

        QImage image(srcAbs);
        if (image.isNull()) {
            continue;
        }

        const QString sanitized = ThumbnailUrlHelper::sanitizeThumbnailName(title);
        const QString destDir = QDir(exportDir).filePath(libretroName + QLatin1Char('/') + folder);
        QDir().mkpath(destDir);
        const QString destPath = QDir(destDir).filePath(sanitized + QStringLiteral(".png"));
        if (image.save(destPath, "PNG")) {
            ++filesExported;
        }
    }
    return true;
}

} // namespace CompendiumThumbnails
