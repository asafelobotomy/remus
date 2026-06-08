#ifndef GAMETDB_PROVIDER_H
#define GAMETDB_PROVIDER_H

#include "metadata_provider.h"
#include "../core/constants/providers.h"
#include <QMap>
#include <QHash>
#include <QMutex>

namespace Remus {

/**
 * @brief A single game entry parsed from a GameTDB XML database
 */
struct GameTDBEntry {
    QString id; // Platform-specific game ID (e.g. "RMCP01")
    QString type; // "Wii", "GameCube", "DS", "3DS", "WiiU", "Switch", "PS3"
    QString region; // "PAL", "NTSC-U", "NTSC-J", etc.
    QString title; // English title (from locale lang="EN")
    QString synopsis; // English description
    QString developer;
    QString publisher;
    QString genre; // Comma-separated genres
    int players = 0; // Max local players
    int year = 0; // Release year
    int month = 0; // Release month
    int day = 0; // Release day
    QString crc32; // ROM CRC32 (uppercase, from <rom> element)
    QString md5; // ROM MD5 (uppercase)
    QString sha1; // ROM SHA1 (uppercase)
};

/**
 * @brief Offline metadata provider using GameTDB XML databases
 *
 * Parses wiitdb.xml, dstdb.xml, 3dstdb.xml, wiiutdb.xml, switchtdb.xml,
 * ps3tdb.xml for Nintendo and PlayStation 3 platform metadata.
 *
 * Priority: 60 (above TheGamesDB, below ScreenScraper)
 *
 * XML files location: data/gametdb/*.xml
 *
 * Features:
 * - Hash-based matching via CRC32/MD5/SHA1 from <rom> elements
 * - Name-based search via title substring matching
 * - Artwork URL construction via art.gametdb.com CDN
 * - No authentication required
 */
class GameTDBProvider : public MetadataProvider {
    Q_OBJECT

public:
    explicit GameTDBProvider(QObject *parent = nullptr);
    ~GameTDBProvider() override;

    /**
     * @brief Load all XML databases from a directory
     * @param directory Path to directory containing *.xml files
     * @return Total number of game entries loaded
     */
    int loadDatabases(const QString &directory);

    /**
     * @brief Load a single GameTDB XML file
     * @param filePath Path to .xml file
     * @return Number of entries loaded from this file
     */
    int loadDatabase(const QString &filePath);

    // MetadataProvider interface
    QList<SearchResult> searchByName(
        const QString &title, const QString &system, const QString &region = QString()) override;

    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;

    QString name() const override {
        return QStringLiteral("GameTDB");
    }
    bool requiresAuth() const override {
        return false;
    }

    QString providerName() const {
        return QStringLiteral("GameTDB");
    }
    int priority() const {
        return Constants::Providers::Priority::GAMETDB;
    }

    /**
     * @brief Map a GameTDB type string to the CDN platform code
     */
    static QString cdnPlatformCode(const QString &gameType);

    /**
     * @brief Map a GameTDB region string to the CDN region code
     */
    static QString cdnRegionCode(const QString &gameRegion);

    /**
     * @brief Build a GameTDB artwork CDN URL
     */
    static QString buildArtworkUrl(const QString &platformCode, const QString &artType, const QString &regionCode,
        const QString &gameId, const QString &extension);

    /**
     * @brief O(1) lookup by normalized (lowercase trimmed) title.
     * Returns the first matching game ID, or an empty string if not found.
     * Used by the enrichment pipeline to avoid the O(N) searchByName scan.
     */
    QString gameIdByNormalizedTitle(const QString &normalizedTitle) const;

private:
    GameMetadata entryToMetadata(const GameTDBEntry &entry) const;
    QString normalizeHash(const QString &hash) const;

    mutable QMutex m_mutex;
    QHash<QString, GameTDBEntry> m_idIndex; // gameId -> entry
    QHash<QString, QString> m_crc32Index; // CRC32 -> gameId
    QHash<QString, QString> m_md5Index; // MD5 -> gameId
    QHash<QString, QString> m_sha1Index; // SHA1 -> gameId
    QHash<QString, QString> m_titleIndex; // normalized title -> gameId (first seen)
};

} // namespace Remus

#endif // GAMETDB_PROVIDER_H
