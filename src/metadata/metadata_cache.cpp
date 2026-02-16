#include "metadata_cache.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include "../core/logging_categories.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug() qCDebug(logMetadata)
#define qInfo() qCInfo(logMetadata)
#define qWarning() qCWarning(logMetadata)
#define qCritical() qCCritical(logMetadata)

namespace Remus {

MetadataCache::MetadataCache(QSqlDatabase &db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

GameMetadata MetadataCache::getByHash(const QString &hash, const QString &system)
{
    GameMetadata metadata;

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT cache_value FROM cache 
        WHERE cache_key = ? AND expiry > datetime('now')
    )");
    
    QString cacheKey = QString("metadata:hash:%1:%2").arg(system, hash);
    query.addBindValue(cacheKey);

    if (query.exec() && query.next()) {
        QByteArray data = query.value(0).toByteArray();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject json = doc.object();
            
            metadata.id = json["id"].toString();
            metadata.title = json["title"].toString();
            metadata.system = json["system"].toString();
            metadata.region = json["region"].toString();
            metadata.publisher = json["publisher"].toString();
            metadata.developer = json["developer"].toString();
            metadata.releaseDate = json["releaseDate"].toString();
            metadata.description = json["description"].toString();
            metadata.players = json["players"].toInt();
            metadata.rating = static_cast<float>(json["rating"].toDouble());
            metadata.providerId = json["providerId"].toString();
            metadata.boxArtUrl = json["boxArtUrl"].toString();
            metadata.matchMethod = json["matchMethod"].toString();
            metadata.matchScore = static_cast<float>(json["matchScore"].toDouble());
            
            // Deserialize genres
            QJsonArray genresArray = json["genres"].toArray();
            for (const QJsonValue &genre : genresArray) {
                metadata.genres.append(genre.toString());
            }
            
            // Deserialize external IDs
            QJsonObject externalIds = json["externalIds"].toObject();
            for (auto it = externalIds.begin(); it != externalIds.end(); ++it) {
                metadata.externalIds[it.key()] = it.value().toString();
            }
            
            // Deserialize timestamp
            QString fetchedAtStr = json["fetchedAt"].toString();
            if (!fetchedAtStr.isEmpty()) {
                metadata.fetchedAt = QDateTime::fromString(fetchedAtStr, Qt::ISODate);
            }
            
            qCDebug(logMetadata) << "Cache hit for hash:" << hash << "- Title:" << metadata.title;
        } else {
            qCWarning(logMetadata) << "Failed to parse cached metadata JSON for hash:" << hash;
        }
    }

    return metadata;
}

GameMetadata MetadataCache::getByProviderId(const QString &providerId, const QString &gameId)
{
    GameMetadata metadata;

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT cache_value FROM cache 
        WHERE cache_key = ? AND expiry > datetime('now')
    )");
    
    QString cacheKey = QString("metadata:%1:%2").arg(providerId, gameId);
    query.addBindValue(cacheKey);

    if (query.exec() && query.next()) {
        QByteArray data = query.value(0).toByteArray();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject json = doc.object();
            
            metadata.id = json["id"].toString();
            metadata.title = json["title"].toString();
            metadata.system = json["system"].toString();
            metadata.region = json["region"].toString();
            metadata.publisher = json["publisher"].toString();
            metadata.developer = json["developer"].toString();
            metadata.releaseDate = json["releaseDate"].toString();
            metadata.description = json["description"].toString();
            metadata.players = json["players"].toInt();
            metadata.rating = static_cast<float>(json["rating"].toDouble());
            metadata.providerId = json["providerId"].toString();
            metadata.boxArtUrl = json["boxArtUrl"].toString();
            metadata.matchMethod = json["matchMethod"].toString();
            metadata.matchScore = static_cast<float>(json["matchScore"].toDouble());
            
            // Deserialize genres
            QJsonArray genresArray = json["genres"].toArray();
            for (const QJsonValue &genre : genresArray) {
                metadata.genres.append(genre.toString());
            }
            
            // Deserialize external IDs
            QJsonObject externalIds = json["externalIds"].toObject();
            for (auto it = externalIds.begin(); it != externalIds.end(); ++it) {
                metadata.externalIds[it.key()] = it.value().toString();
            }
            
            // Deserialize timestamp
            QString fetchedAtStr = json["fetchedAt"].toString();
            if (!fetchedAtStr.isEmpty()) {
                metadata.fetchedAt = QDateTime::fromString(fetchedAtStr, Qt::ISODate);
            }
            
            qCDebug(logMetadata) << "Cache hit for" << providerId << "ID:" << gameId << "- Title:" << metadata.title;
        } else {
            qCWarning(logMetadata) << "Failed to parse cached metadata JSON for" << providerId << gameId;
        }
    }

    return metadata;
}

bool MetadataCache::store(const GameMetadata &metadata, const QString &hash, const QString &system)
{
    // Serialize metadata to JSON
    QJsonObject json;
    json["id"] = metadata.id;
    json["title"] = metadata.title;
    json["system"] = metadata.system;
    json["region"] = metadata.region;
    json["publisher"] = metadata.publisher;
    json["developer"] = metadata.developer;
    json["genres"] = QJsonArray::fromStringList(metadata.genres);
    json["releaseDate"] = metadata.releaseDate;
    json["description"] = metadata.description;
    json["players"] = metadata.players;
    json["rating"] = metadata.rating;
    json["providerId"] = metadata.providerId;
    json["boxArtUrl"] = metadata.boxArtUrl;
    json["matchMethod"] = metadata.matchMethod;
    json["matchScore"] = metadata.matchScore;
    
    // Serialize external IDs
    QJsonObject externalIds;
    for (auto it = metadata.externalIds.begin(); it != metadata.externalIds.end(); ++it) {
        externalIds[it.key()] = it.value();
    }
    json["externalIds"] = externalIds;
    
    // Serialize timestamp
    if (metadata.fetchedAt.isValid()) {
        json["fetchedAt"] = metadata.fetchedAt.toString(Qt::ISODate);
    }

    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    // Store with provider ID as key
    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR REPLACE INTO cache (cache_key, cache_value, expiry)
        VALUES (?, ?, datetime('now', '+3650 days'))
    )");

    QString cacheKey = QString("metadata:%1:%2").arg(metadata.providerId, metadata.id);
    query.addBindValue(cacheKey);
    query.addBindValue(data);

    if (!query.exec()) {
        qCWarning(logMetadata) << "Failed to store metadata in cache:" << query.lastError().text();
        return false;
    }

    // Also store by hash if provided
    if (!hash.isEmpty() && !system.isEmpty()) {
        query.prepare(R"(
            INSERT OR REPLACE INTO cache (cache_key, cache_value, expiry)
            VALUES (?, ?, datetime('now', '+3650 days'))
        )");

        QString hashKey = QString("metadata:hash:%1:%2").arg(system, hash);
        query.addBindValue(hashKey);
        query.addBindValue(data);
        query.exec();
    }

    return true;
}

bool MetadataCache::storeArtwork(const QString &gameId, const ArtworkUrls &artwork)
{
    QJsonObject json;
    json["boxFront"] = artwork.boxFront.toString();
    json["boxBack"] = artwork.boxBack.toString();
    json["boxFull"] = artwork.boxFull.toString();
    json["screenshot"] = artwork.screenshot.toString();
    json["titleScreen"] = artwork.titleScreen.toString();
    json["banner"] = artwork.banner.toString();
    json["logo"] = artwork.logo.toString();
    json["clearLogo"] = artwork.clearLogo.toString();

    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT OR REPLACE INTO cache (cache_key, cache_value, expiry)
        VALUES (?, ?, datetime('now', '+3650 days'))
    )");

    QString cacheKey = QString("artwork:%1").arg(gameId);
    query.addBindValue(cacheKey);
    query.addBindValue(data);

    if (!query.exec()) {
        qCWarning(logMetadata) << "Failed to store artwork in cache:" << query.lastError().text();
        return false;
    }

    return true;
}

ArtworkUrls MetadataCache::getArtwork(const QString &gameId)
{
    ArtworkUrls artwork;

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT cache_value FROM cache 
        WHERE cache_key = ? AND expiry > datetime('now')
    )");
    
    QString cacheKey = QString("artwork:%1").arg(gameId);
    query.addBindValue(cacheKey);

    if (query.exec() && query.next()) {
        QByteArray data = query.value(0).toByteArray();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject json = doc.object();

        artwork.boxFront = QUrl(json["boxFront"].toString());
        artwork.boxBack = QUrl(json["boxBack"].toString());
        artwork.boxFull = QUrl(json["boxFull"].toString());
        artwork.screenshot = QUrl(json["screenshot"].toString());
        artwork.titleScreen = QUrl(json["titleScreen"].toString());
        artwork.banner = QUrl(json["banner"].toString());
        artwork.logo = QUrl(json["logo"].toString());
        artwork.clearLogo = QUrl(json["clearLogo"].toString());
    }

    return artwork;
}

int MetadataCache::clearOldCache(int days)
{
    QSqlQuery query(m_db);
    query.prepare(R"(
        DELETE FROM cache 
        WHERE expiry < datetime('now', ? || ' days')
    )");
    query.addBindValue(-days);

    if (query.exec()) {
        return query.numRowsAffected();
    }

    return 0;
}

MetadataCache::CacheStats MetadataCache::getStats()
{
    CacheStats stats;

    QSqlQuery query(m_db);
    
    // Total entries
    query.exec("SELECT COUNT(*) FROM cache WHERE cache_key LIKE 'metadata:%'");
    if (query.next()) {
        stats.totalEntries = query.value(0).toInt();
    }

    // Entries this week
    query.exec(R"(
        SELECT COUNT(*) FROM cache 
        WHERE cache_key LIKE 'metadata:%' 
        AND created_at > datetime('now', '-7 days')
    )");
    if (query.next()) {
        stats.entriesThisWeek = query.value(0).toInt();
    }

    // Total size
    query.exec("SELECT SUM(LENGTH(cache_value)) FROM cache WHERE cache_key LIKE 'metadata:%'");
    if (query.next()) {
        stats.totalSizeBytes = query.value(0).toLongLong();
    }

    return stats;
}

} // namespace Remus
