#ifndef REMUS_METADATA_PROVIDER_H
#define REMUS_METADATA_PROVIDER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QDateTime>
#include <QPixmap>

namespace Remus {

/**
 * @brief Game metadata from provider
 */
struct GameMetadata {
    QString id;                  // Provider-specific ID
    QString title;
    QString system;
    QString region;
    QString publisher;
    QString developer;
    QStringList genres;
    QString releaseDate;         // ISO 8601
    QString description;
    int players = 0;             // Max players
    float rating = 0.0f;         // 0.0 to 10.0
    QString ratingSource;        // e.g., "MobyGames", "IGDB", "Metacritic"
    
    // URLs for artwork (can be populated by getArtwork or included directly)
    QString boxArtUrl;           // Box art URL
    
    // External IDs for cross-referencing
    QMap<QString, QString> externalIds;  // e.g., {"igdb": "1234", "retroachievements": "5678"}
    
    // Provider info
    QString providerId;          // "screenscraper", "thegamesdb", "igdb", "hasheous"
    QDateTime fetchedAt;
    
    // Match quality (set when matched via orchestrator)
    float matchScore = 0.0f;     // 0.0 to 1.0 (1.0 = perfect hash match)
    QString matchMethod;         // "hash", "name-exact", "name-fuzzy"
};

/**
 * @brief Artwork URLs from provider
 */
struct ArtworkUrls {
    QUrl boxFront;
    QUrl boxBack;
    QUrl boxFull;               // 3D box or full packaging
    QUrl screenshot;            // Primary screenshot (gameplay)
    QUrl screenshot2;           // Secondary screenshot
    QUrl titleScreen;           // Title/start screen
    QUrl banner;
    QUrl logo;                  // Game logo
    QUrl clearLogo;             // Logo with transparent background
    QUrl systemLogo;            // System/platform logo
};

/**
 * @brief Search result from provider
 */
struct SearchResult {
    QString id;
    QString title;
    QString system;
    QString region;
    int releaseYear = 0;
    float matchScore = 0.0f;    // 0.0 to 1.0
    QString provider;           // Provider name that returned this result
};

/**
 * @brief Base interface for metadata providers
 */
class MetadataProvider : public QObject {
    Q_OBJECT

public:
    explicit MetadataProvider(QObject *parent = nullptr);
    virtual ~MetadataProvider() = default;

    /**
     * @brief Get provider name
     */
    virtual QString name() const = 0;

    /**
     * @brief Check if provider requires authentication
     */
    virtual bool requiresAuth() const = 0;

    /**
     * @brief Set authentication credentials
     */
    virtual void setCredentials(const QString &username, const QString &password);

    /**
     * @brief Search games by name
     * @param title Game title
     * @param system System name (optional)
     * @param region Region (optional)
     * @return List of search results
     */
    virtual QList<SearchResult> searchByName(const QString &title,
                                             const QString &system = QString(),
                                             const QString &region = QString()) = 0;

    /**
     * @brief Get metadata by hash (for No-Intro/Redump verification)
     * @param hash CRC32, MD5, or SHA1 hash
     * @param system System name
     * @return Game metadata (empty if not found)
     */
    virtual GameMetadata getByHash(const QString &hash, const QString &system) = 0;

    /**
     * @brief Get metadata by provider ID
     * @param id Provider-specific game ID
     * @return Game metadata
     */
    virtual GameMetadata getById(const QString &id) = 0;

    /**
     * @brief Get artwork URLs for game
     * @param id Provider-specific game ID
     * @return Artwork URLs
     */
    virtual ArtworkUrls getArtwork(const QString &id) = 0;

    /**
     * @brief Download image from URL
     * @param url Image URL
     * @return Downloaded image data
     */
    QByteArray downloadImage(const QUrl &url);

    /**
     * @brief Check if provider is available/online
     */
    virtual bool isAvailable();

signals:
    void searchCompleted(const QList<SearchResult> &results);
    void metadataFetched(const GameMetadata &metadata);
    void artworkFetched(const ArtworkUrls &artwork);
    void errorOccurred(const QString &error);
    void rateLimitReached();

protected:
    QString m_username;
    QString m_password;
    bool m_authenticated = false;
};

} // namespace Remus

#endif // REMUS_METADATA_PROVIDER_H
