#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QVariantList>
#include "../../core/database.h"
#include "../../metadata/artwork_downloader.h"
#include "../../metadata/provider_orchestrator.h"

namespace Remus {

/**
 * @brief Controller for artwork management in UI
 * 
 * Handles artwork downloading, caching, and display.
 * Exposed to QML as a context property.
 */
class ArtworkController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool downloading READ isDownloading NOTIFY downloadingChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)
    Q_PROPERTY(int downloadTotal READ downloadTotal NOTIFY downloadTotalChanged)
    Q_PROPERTY(QString artworkBasePath READ artworkBasePath WRITE setArtworkBasePath NOTIFY artworkBasePathChanged)

public:
    explicit ArtworkController(Database *db, 
                               ProviderOrchestrator *orchestrator,
                               QObject *parent = nullptr);

    bool isDownloading() const { return m_downloading; }
    int downloadProgress() const { return m_downloadProgress; }
    int downloadTotal() const { return m_downloadTotal; }
    QString artworkBasePath() const { return m_artworkBasePath; }
    void setArtworkBasePath(const QString &path);

    // Artwork types
    enum class ArtworkType {
        BoxArt,
        Screenshot,
        Banner,
        Logo,
        Fanart,
        Titlescreen
    };
    Q_ENUM(ArtworkType)

    /**
     * @brief Get local artwork path for a game
     * @param gameId Game database ID
     * @param type Artwork type (boxart, screenshot, etc.)
     * @return Local file path (may not exist if not downloaded)
     */
    Q_INVOKABLE QString getArtworkPath(int gameId, const QString &type);

    /**
     * @brief Get artwork URL (local if available, remote otherwise)
     * @param gameId Game database ID
     * @param type Artwork type
     * @return URL suitable for QML Image source
     */
    Q_INVOKABLE QUrl getArtworkUrl(int gameId, const QString &type);

    /**
     * @brief Check if artwork exists locally
     */
    Q_INVOKABLE bool hasLocalArtwork(int gameId, const QString &type);

    /**
     * @brief Download artwork for a specific game
     * @param gameId Game database ID
     * @param types List of artwork types to download (empty = all)
     */
    Q_INVOKABLE void downloadArtwork(int gameId, const QStringList &types = {});

    /**
     * @brief Batch download artwork for all matched games
     * @param systemFilter Optional: only download for specific system
     * @param overwrite Whether to re-download existing artwork
     */
    Q_INVOKABLE void downloadAllArtwork(const QString &systemFilter = QString(), 
                                        bool overwrite = false);

    /**
     * @brief Cancel ongoing downloads
     */
    Q_INVOKABLE void cancelDownloads();

    /**
     * @brief Get artwork statistics
     * @return Map with counts: total, downloaded, missing
     */
    Q_INVOKABLE QVariantMap getArtworkStats();

    /**
     * @brief Get list of games missing artwork
     * @param type Artwork type to check
     * @param limit Max results
     */
    Q_INVOKABLE QVariantList getGamesMissingArtwork(const QString &type, int limit = 100);

    /**
     * @brief Delete local artwork for a game
     */
    Q_INVOKABLE bool deleteArtwork(int gameId, const QString &type = QString());

    /**
     * @brief Clear all artwork cache
     */
    Q_INVOKABLE void clearArtworkCache();

signals:
    void downloadingChanged();
    void downloadProgressChanged();
    void downloadTotalChanged();
    void artworkBasePathChanged();

    void artworkDownloaded(int gameId, const QString &type, const QString &path);
    void artworkFailed(int gameId, const QString &type, const QString &error);
    void batchDownloadCompleted(int downloaded, int failed);

private:
    void downloadSingleArtwork(int gameId, const QString &type, const QUrl &url);
    QString typeToSubfolder(const QString &type) const;
    QString getArtworkFilename(int gameId, const QString &type) const;

    Database *m_db;
    ProviderOrchestrator *m_orchestrator;
    ArtworkDownloader *m_downloader;
    
    bool m_downloading = false;
    bool m_cancelRequested = false;
    int m_downloadProgress = 0;
    int m_downloadTotal = 0;
    QString m_artworkBasePath;
};

} // namespace Remus
