#include "artwork_controller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QMetaObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include "../../core/logging_categories.h"
#include "../../core/constants/settings.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug() qCDebug(logUi)
#define qInfo() qCInfo(logUi)
#define qWarning() qCWarning(logUi)
#define qCritical() qCCritical(logUi)

namespace Remus {

ArtworkController::ArtworkController(Database *db, 
                                     ProviderOrchestrator *orchestrator,
                                     QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_orchestrator(orchestrator)
    , m_downloader(new ArtworkDownloader(this))
{
    // Default artwork path
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_artworkBasePath = dataPath + "/" + Constants::Settings::Files::ARTWORK_SUBDIR;
    
    // Ensure base directory exists
    QDir().mkpath(m_artworkBasePath);

    // Connect downloader signals
    connect(m_downloader, &ArtworkDownloader::downloadCompleted,
            this, [this](const QUrl &url, const QString &filePath) {
                Q_UNUSED(url);
                Q_UNUSED(filePath);
                m_downloadProgress++;
                emit downloadProgressChanged();
            });

    connect(m_downloader, &ArtworkDownloader::downloadFailed,
            this, [this](const QUrl &url, const QString &error) {
                qWarning() << "Artwork download failed:" << url << error;
            });
}

void ArtworkController::setArtworkBasePath(const QString &path)
{
    if (m_artworkBasePath != path) {
        m_artworkBasePath = path;
        QDir().mkpath(m_artworkBasePath);
        emit artworkBasePathChanged();
    }
}

QString ArtworkController::typeToSubfolder(const QString &type) const
{
    // Map artwork types to subfolder names
    static const QHash<QString, QString> mapping = {
        {"boxart", "boxart"},
        {"box_art", "boxart"},
        {"cover", "boxart"},
        {"screenshot", "screenshots"},
        {"snap", "screenshots"},
        {"banner", "banners"},
        {"logo", "logos"},
        {"fanart", "fanart"},
        {"background", "fanart"},
        {"titlescreen", "titlescreens"},
        {"title", "titlescreens"}
    };
    
    return mapping.value(type.toLower(), type.toLower());
}

QString ArtworkController::getArtworkFilename(int gameId, const QString &type) const
{
    // Get game title for filename
    QSqlQuery query(m_db->database());
    query.prepare("SELECT title FROM games WHERE id = ?");
    query.addBindValue(gameId);
    
    QString title;
    if (query.exec() && query.next()) {
        title = query.value(0).toString();
    } else {
        title = QString("game_%1").arg(gameId);
    }
    
    // Sanitize filename
    title = title.replace(QRegularExpression("[/\\\\:*?\"<>|]"), "_");
    
    return title + ".png";
}

QString ArtworkController::getArtworkPath(int gameId, const QString &type)
{
    QString subfolder = typeToSubfolder(type);
    QString filename = getArtworkFilename(gameId, type);
    
    return m_artworkBasePath + "/" + subfolder + "/" + filename;
}

QUrl ArtworkController::getArtworkUrl(int gameId, const QString &type)
{
    QString localPath = getArtworkPath(gameId, type);
    
    // If local file exists, return file URL
    if (QFile::exists(localPath)) {
        return QUrl::fromLocalFile(localPath);
    }
    
    // Otherwise, get remote URL from metadata
    QSqlQuery query(m_db->database());
    query.prepare(R"(
        SELECT ms.artwork_urls 
        FROM games g
        JOIN metadata_sources ms ON g.id = ms.game_id
        WHERE g.id = ?
        ORDER BY ms.priority DESC
        LIMIT 1
    )");
    query.addBindValue(gameId);
    
    if (query.exec() && query.next()) {
        QString artworkJson = query.value(0).toString();
        // Parse JSON to get the right type
        QJsonDocument doc = QJsonDocument::fromJson(artworkJson.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString urlKey = type.toLower();
            if (obj.contains(urlKey)) {
                return QUrl(obj[urlKey].toString());
            }
            // Try alternate keys
            if (urlKey == "boxart" && obj.contains("box_art")) {
                return QUrl(obj["box_art"].toString());
            }
        }
    }
    
    return QUrl();  // No artwork available
}

bool ArtworkController::hasLocalArtwork(int gameId, const QString &type)
{
    QString localPath = getArtworkPath(gameId, type);
    return QFile::exists(localPath);
}

void ArtworkController::downloadArtwork(int gameId, const QStringList &types)
{
    QStringList artworkTypes = types;
    if (artworkTypes.isEmpty()) {
        artworkTypes = {"boxart", "screenshot", "banner", "logo"};
    }
    
    m_downloading = true;
    m_downloadProgress = 0;
    m_downloadTotal = artworkTypes.size();
    emit downloadingChanged();
    emit downloadTotalChanged();
    
    for (const QString &type : artworkTypes) {
        if (m_cancelRequested) break;
        
        QUrl url = getArtworkUrl(gameId, type);
        if (url.isValid() && !url.isLocalFile()) {
            downloadSingleArtwork(gameId, type, url);
        }
    }
    
    m_downloading = false;
    m_cancelRequested = false;
    emit downloadingChanged();
}

void ArtworkController::downloadSingleArtwork(int gameId, const QString &type, const QUrl &url)
{
    QString destPath = getArtworkPath(gameId, type);
    
    // Ensure directory exists
    QFileInfo fileInfo(destPath);
    QDir().mkpath(fileInfo.absolutePath());
    
    if (m_downloader->download(url, destPath)) {
        emit artworkDownloaded(gameId, type, destPath);
    } else {
        emit artworkFailed(gameId, type, "Download failed");
    }
}

void ArtworkController::downloadAllArtwork(const QString &systemFilter, bool overwrite)
{
    // Get all matched games
    QSqlQuery query(m_db->database());
    
    QString sql = R"(
        SELECT DISTINCT g.id 
        FROM games g
        JOIN matches m ON g.id = m.game_id
        WHERE m.confidence >= 60
    )";
    
    if (!systemFilter.isEmpty()) {
        sql += " AND g.system = ?";
        query.prepare(sql);
        query.addBindValue(systemFilter);
    } else {
        query.prepare(sql);
    }
    
    QList<int> gameIds;
    if (query.exec()) {
        while (query.next()) {
            gameIds.append(query.value(0).toInt());
        }
    }
    
    if (gameIds.isEmpty()) {
        emit batchDownloadCompleted(0, 0);
        return;
    }
    
    m_downloading = true;
    m_cancelRequested = false;
    m_downloadProgress = 0;
    m_downloadTotal = gameIds.size();
    emit downloadingChanged();
    emit downloadTotalChanged();
    
    int downloaded = 0;
    int failed = 0;
    
    QStringList types = {"boxart", "screenshot"};  // Primary types for batch
    
    for (int gameId : gameIds) {
        if (m_cancelRequested) break;
        
        bool anySuccess = false;
        for (const QString &type : types) {
            if (m_cancelRequested) break;
            
            // Skip if already exists and not overwriting
            if (!overwrite && hasLocalArtwork(gameId, type)) {
                anySuccess = true;
                continue;
            }
            
            QUrl url = getArtworkUrl(gameId, type);
            if (url.isValid() && !url.isLocalFile()) {
                QString destPath = getArtworkPath(gameId, type);
                QFileInfo fileInfo(destPath);
                QDir().mkpath(fileInfo.absolutePath());
                
                if (m_downloader->download(url, destPath)) {
                    anySuccess = true;
                    emit artworkDownloaded(gameId, type, destPath);
                }
            }
        }
        
        if (anySuccess) {
            downloaded++;
        } else {
            failed++;
        }
        
        m_downloadProgress++;
        emit downloadProgressChanged();
    }
    
    m_downloading = false;
    m_cancelRequested = false;
    emit downloadingChanged();
    emit batchDownloadCompleted(downloaded, failed);
}

void ArtworkController::cancelDownloads()
{
    m_cancelRequested = true;
}

QVariantMap ArtworkController::getArtworkStats()
{
    QVariantMap stats;
    
    // Count total matched games
    QSqlQuery query(m_db->database());
    query.exec("SELECT COUNT(DISTINCT game_id) FROM matches WHERE confidence >= 60");
    int totalGames = query.next() ? query.value(0).toInt() : 0;
    
    // Count games with local boxart
    int withArtwork = 0;
    query.exec("SELECT DISTINCT g.id FROM games g JOIN matches m ON g.id = m.game_id WHERE m.confidence >= 60");
    while (query.next()) {
        int gameId = query.value(0).toInt();
        if (hasLocalArtwork(gameId, "boxart")) {
            withArtwork++;
        }
    }
    
    // Calculate storage used
    qint64 storageBytes = 0;
    QDir artworkDir(m_artworkBasePath);
    QFileInfoList files = artworkDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &info : files) {
        if (info.isDir()) {
            QDir subdir(info.absoluteFilePath());
            for (const QFileInfo &subfile : subdir.entryInfoList(QDir::Files)) {
                storageBytes += subfile.size();
            }
        } else {
            storageBytes += info.size();
        }
    }
    
    stats["totalGames"] = totalGames;
    stats["withArtwork"] = withArtwork;
    stats["missingArtwork"] = totalGames - withArtwork;
    stats["storageUsedMB"] = static_cast<double>(storageBytes) / (1024.0 * 1024.0);
    stats["artworkPath"] = m_artworkBasePath;
    
    return stats;
}

QVariantList ArtworkController::getGamesMissingArtwork(const QString &type, int limit)
{
    QVariantList result;
    
    QSqlQuery query(m_db->database());
    query.prepare(R"(
        SELECT g.id, g.title, g.system
        FROM games g
        JOIN matches m ON g.id = m.game_id
        WHERE m.confidence >= 60
        LIMIT ?
    )");
    query.addBindValue(limit * 2);  // Fetch extra since we filter
    
    if (query.exec()) {
        int count = 0;
        while (query.next() && count < limit) {
            int gameId = query.value(0).toInt();
            if (!hasLocalArtwork(gameId, type)) {
                QVariantMap game;
                game["id"] = gameId;
                game["title"] = query.value(1).toString();
                game["system"] = query.value(2).toString();
                result.append(game);
                count++;
            }
        }
    }
    
    return result;
}

bool ArtworkController::deleteArtwork(int gameId, const QString &type)
{
    if (type.isEmpty()) {
        // Delete all artwork for game
        QStringList types = {"boxart", "screenshot", "banner", "logo", "fanart", "titlescreen"};
        bool success = true;
        for (const QString &t : types) {
            QString path = getArtworkPath(gameId, t);
            if (QFile::exists(path)) {
                success &= QFile::remove(path);
            }
        }
        return success;
    } else {
        QString path = getArtworkPath(gameId, type);
        return QFile::remove(path);
    }
}

void ArtworkController::clearArtworkCache()
{
    QDir artworkDir(m_artworkBasePath);
    
    // Remove all subdirectories and files
    QStringList subdirs = {"boxart", "screenshots", "banners", "logos", "fanart", "titlescreens"};
    for (const QString &subdir : subdirs) {
        QDir dir(m_artworkBasePath + "/" + subdir);
        if (dir.exists()) {
            dir.removeRecursively();
        }
    }
    
    // Recreate base directory
    QDir().mkpath(m_artworkBasePath);
}

} // namespace Remus
