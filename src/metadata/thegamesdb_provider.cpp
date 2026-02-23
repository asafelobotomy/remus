#include "thegamesdb_provider.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include "../core/constants/constants.h"

namespace Remus {

TheGamesDBProvider::TheGamesDBProvider(QObject *parent)
    : MetadataProvider(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_rateLimiter(new RateLimiter(this))
{
    m_rateLimiter->setInterval(Constants::Network::THEGAMESDB_RATE_LIMIT_MS);
}

void TheGamesDBProvider::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey;
}

QList<SearchResult> TheGamesDBProvider::searchByName(const QString &title,
                                                      const QString &system,
                                                      const QString &region)
{
    QList<SearchResult> results;

    m_rateLimiter->waitIfNeeded();

    // Build API URL
    QUrl url(QString(Constants::API::THEGAMESDB_BASE_URL) + Constants::API::THEGAMESDB_GAMES_ENDPOINT);
    QUrlQuery query;
    
    if (!m_apiKey.isEmpty()) {
        query.addQueryItem("apikey", m_apiKey);
    }
    query.addQueryItem("name", title);
    
    if (!system.isEmpty()) {
        // Get system ID from internal name, then get TheGamesDB platform ID
        int systemId = SystemResolver::systemIdByName(system);
        QString tgdbPlatformId = SystemResolver::providerName(systemId, Constants::Providers::THEGAMESDB);
        if (!tgdbPlatformId.isEmpty()) {
            query.addQueryItem("filter[platform]", tgdbPlatformId);
            qDebug() << "TheGamesDB: Using platform ID" << tgdbPlatformId << "for system" << system;
        }
    }

    url.setQuery(query);

    ApiResponse response = makeRequest(url);
    if (!response.success) {
        emit errorOccurred(response.error);
        return results;
    }

    // Parse JSON response
    QJsonDocument doc = QJsonDocument::fromJson(response.data);
    QJsonObject root = doc.object();
    
    if (root.contains("data") && root["data"].isObject()) {
        QJsonObject data = root["data"].toObject();
        QJsonArray games = data["games"].toArray();
        
        for (const QJsonValue &gameVal : games) {
            QJsonObject game = gameVal.toObject();
            
            SearchResult result;
            result.id = QString::number(game["id"].toInt());
            result.title = game["game_title"].toString();
            result.system = system;
            
            if (game.contains("release_date")) {
                QString date = game["release_date"].toString();
                if (date.length() >= 4) {
                    result.releaseYear = date.left(4).toInt();
                }
            }
            
            // Calculate basic match score
            QString gameTitle = result.title.toLower();
            QString searchTitle = title.toLower();
            
            if (gameTitle == searchTitle) {
                result.matchScore = 1.0f;
            } else if (gameTitle.contains(searchTitle) || searchTitle.contains(gameTitle)) {
                result.matchScore = 0.8f;
            } else {
                result.matchScore = 0.6f;
            }
            
            results.append(result);
        }
    }

    return results;
}

GameMetadata TheGamesDBProvider::getByHash(const QString &hash, const QString &system)
{
    // TheGamesDB does not support hash-based lookups
    GameMetadata metadata;
    emit errorOccurred("TheGamesDB does not support hash-based lookups");
    return metadata;
}

GameMetadata TheGamesDBProvider::getById(const QString &id)
{
    GameMetadata metadata;

    m_rateLimiter->waitIfNeeded();

    // Build API URL
    QUrl url(QString(Constants::API::THEGAMESDB_BASE_URL) + Constants::API::THEGAMESDB_GAMEINFO_ENDPOINT);
    QUrlQuery query;
    
    if (!m_apiKey.isEmpty()) {
        query.addQueryItem("apikey", m_apiKey);
    }
    query.addQueryItem("id", id);

    url.setQuery(query);

    ApiResponse response = makeRequest(url);
    if (!response.success) {
        emit errorOccurred(response.error);
        return metadata;
    }

    // Parse JSON
    QJsonDocument doc = QJsonDocument::fromJson(response.data);
    QJsonObject root = doc.object();
    
    if (root.contains("data") && root["data"].isObject()) {
        QJsonObject data = root["data"].toObject();
        QJsonArray games = data["games"].toArray();
        
        if (!games.isEmpty()) {
            metadata = parseGameJson(games[0].toObject());
        }
    }

    return metadata;
}

ArtworkUrls TheGamesDBProvider::getArtwork(const QString &id)
{
    ArtworkUrls artwork;

    // Artwork requires separate API call to Images endpoint
    m_rateLimiter->waitIfNeeded();

    QUrl url(QString(Constants::API::THEGAMESDB_BASE_URL) + Constants::API::THEGAMESDB_IMAGES_ENDPOINT);
    QUrlQuery query;
    
    if (!m_apiKey.isEmpty()) {
        query.addQueryItem("apikey", m_apiKey);
    }
    query.addQueryItem("games_id", id);

    url.setQuery(query);

    ApiResponse response = makeRequest(url);
    if (!response.success) {
        return artwork;
    }

    // Parse image URLs
    QJsonDocument doc = QJsonDocument::fromJson(response.data);
    QJsonObject root = doc.object();
    
    if (root.contains("data") && root["data"].isObject()) {
        QJsonObject data = root["data"].toObject();
        QString baseUrl = data["base_url"].toObject()["original"].toString();
        
        QJsonObject images = data["images"].toObject()[id].toObject();
        
        // Box art
        if (images.contains("boxart")) {
            QJsonArray boxart = images["boxart"].toArray();
            for (const QJsonValue &imgVal : boxart) {
                QJsonObject img = imgVal.toObject();
                if (img["side"].toString() == "front") {
                    artwork.boxFront = QUrl(baseUrl + img["filename"].toString());
                } else if (img["side"].toString() == "back") {
                    artwork.boxBack = QUrl(baseUrl + img["filename"].toString());
                }
            }
        }
        
        // Screenshots
        if (images.contains("screenshot")) {
            QJsonArray screenshots = images["screenshot"].toArray();
            if (!screenshots.isEmpty()) {
                artwork.screenshot = QUrl(baseUrl + screenshots[0].toObject()["filename"].toString());
            }
        }
        
        // Banners
        if (images.contains("banner")) {
            QJsonArray banners = images["banner"].toArray();
            if (!banners.isEmpty()) {
                artwork.banner = QUrl(baseUrl + banners[0].toObject()["filename"].toString());
            }
        }
    }

    return artwork;
}

TheGamesDBProvider::ApiResponse TheGamesDBProvider::makeRequest(const QUrl &url)
{
    ApiResponse response;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);

    QNetworkReply *reply = m_networkManager->get(request);
    
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(Constants::Network::THEGAMESDB_TIMEOUT_MS);

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeout.start();
    loop.exec();

    if (timeout.isActive()) {
        timeout.stop();
        
        if (reply->error() == QNetworkReply::NoError) {
            response.success = true;
            response.data = reply->readAll();
        } else {
            response.success = false;
            response.error = reply->errorString();
        }
    } else {
        response.success = false;
        response.error = "Request timeout";
    }

    reply->deleteLater();
    return response;
}

GameMetadata TheGamesDBProvider::parseGameJson(const QJsonObject &game)
{
    GameMetadata metadata;

    metadata.id = QString::number(game["id"].toInt());
    metadata.providerId = Constants::Providers::THEGAMESDB;
    metadata.fetchedAt = QDateTime::currentDateTime();

    metadata.title = game["game_title"].toString();
    metadata.releaseDate = game["release_date"].toString();
    metadata.description = game["overview"].toString();
    
    // Developers/Publishers
    QJsonArray developers = game["developers"].toArray();
    if (!developers.isEmpty()) {
        metadata.developer = developers[0].toString();
    }
    
    QJsonArray publishers = game["publishers"].toArray();
    if (!publishers.isEmpty()) {
        metadata.publisher = publishers[0].toString();
    }
    
    // Genres
    QJsonArray genres = game["genres"].toArray();
    for (const QJsonValue &genreVal : genres) {
        metadata.genres.append(genreVal.toInt() == 1 ? "Action" : "Other");  // Simplified
    }
    
    // Players
    if (game.contains("players")) {
        metadata.players = game["players"].toInt();
    }

    return metadata;
}



bool TheGamesDBProvider::isAvailable()
{
    // TheGamesDB is generally available without auth
    return true;
}

} // namespace Remus
