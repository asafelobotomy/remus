#include "igdb_provider.h"
#include "../core/system_resolver.h"
#include "../core/constants/providers.h"
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include "../core/constants/constants.h"

namespace Remus {

IGDBProvider::IGDBProvider(QObject *parent)
    : HttpMetadataProvider(Constants::Network::IGDB_RATE_LIMIT_MS, parent)
{
}

void IGDBProvider::setCredentials(const QString &clientId, const QString &clientSecret)
{
    m_clientId = clientId;
    m_clientSecret = clientSecret;
    m_authenticated = !clientId.isEmpty() && !clientSecret.isEmpty();
}

bool IGDBProvider::authenticate()
{
    // Check if token is still valid with proactive refresh buffer
    if (!m_accessToken.isEmpty() && QDateTime::currentDateTime() < m_tokenExpiry.addSecs(-Constants::Network::IGDB_TOKEN_REFRESH_BUFFER_SECS)) {
        return true;
    }

    if (!m_accessToken.isEmpty()) {
        qInfo() << "IGDB: token expiring within" << Constants::Network::IGDB_TOKEN_REFRESH_BUFFER_SECS / 3600 << "hours, refreshing proactively";
    }

    // Request new access token from Twitch
    QUrl url(Constants::API::IGDB_AUTH_URL);
    QUrlQuery query;
    query.addQueryItem("client_id", m_clientId);
    query.addQueryItem("client_secret", m_clientSecret);
    query.addQueryItem("grant_type", "client_credentials");

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QNetworkReply *reply = m_networkManager->post(request, query.toString().toUtf8());
    ApiResponse response = waitForReply(reply, Constants::Network::IGDB_TIMEOUT_MS);

    if (!response.success) {
        qWarning() << "IGDB: authentication request failed:" << response.error;
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response.data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "IGDB auth JSON parse error:" << parseError.errorString();
        return false;
    }

    QJsonObject obj = doc.object();

    m_accessToken = obj["access_token"].toString();
    int expiresIn = obj["expires_in"].toInt();
    m_tokenExpiry = QDateTime::currentDateTime().addSecs(expiresIn);

    return true;
}

QList<SearchResult> IGDBProvider::searchByName(const QString &title,
                                                const QString &system,
                                                const QString &region)
{
    QList<SearchResult> results;

    if (!m_authenticated || !authenticate()) {
        emit errorOccurred("IGDB authentication failed");
        return results;
    }

    m_rateLimiter->waitIfNeeded();

    // Build IGDB query (using Apicalypse query language)
    QString body = QString("search \"%1\"; fields name,first_release_date,platforms; limit 10;")
                       .arg(title);

    ApiResponse response = makeRequest("/games", body);
    if (!response.success) {
        emit errorOccurred(response.error);
        return results;
    }

    // Parse JSON array
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response.data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "IGDB search JSON parse error:" << parseError.errorString();
        emit errorOccurred("IGDB JSON parse error: " + parseError.errorString());
        return results;
    }

    QJsonArray games = doc.array();
    
    for (const QJsonValue &gameVal : games) {
        QJsonObject game = gameVal.toObject();
        
        SearchResult result;
        result.id = QString::number(game["id"].toInt());
        result.title = game["name"].toString();
        result.system = system;
        
        if (game.contains("first_release_date")) {
            qint64 timestamp = game["first_release_date"].toVariant().toLongLong();
            QDateTime releaseDate = QDateTime::fromSecsSinceEpoch(timestamp);
            result.releaseYear = releaseDate.date().year();
        }
        
        result.matchScore = 0.85f;
        results.append(result);
    }

    return results;
}

GameMetadata IGDBProvider::getByHash(const QString &hash, const QString &system)
{
    // IGDB does not support hash-based lookups
    GameMetadata metadata;
    emit errorOccurred("IGDB does not support hash-based lookups");
    return metadata;
}

GameMetadata IGDBProvider::getById(const QString &id)
{
    GameMetadata metadata;

    if (!m_authenticated || !authenticate()) {
        emit errorOccurred("IGDB authentication failed");
        return metadata;
    }

    m_rateLimiter->waitIfNeeded();

    QString body = QString("fields name,summary,genres.name,first_release_date,"
                           "involved_companies.company.name,involved_companies.developer,"
                           "involved_companies.publisher,aggregated_rating,"
                           "multiplayer_modes.offlinemax; where id = %1;")
                       .arg(id);

    ApiResponse response = makeRequest("/games", body);
    if (!response.success) {
        emit errorOccurred(response.error);
        return metadata;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response.data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "IGDB game JSON parse error:" << parseError.errorString();
        emit errorOccurred("IGDB JSON parse error: " + parseError.errorString());
        return metadata;
    }

    QJsonArray games = doc.array();
    
    if (!games.isEmpty()) {
        metadata = parseGameJson(games[0].toObject());
    }

    return metadata;
}

ArtworkUrls IGDBProvider::getArtwork(const QString &id)
{
    ArtworkUrls artwork;

    if (!m_authenticated || !authenticate()) {
        return artwork;
    }

    m_rateLimiter->waitIfNeeded();

    QString body = QString("fields cover.url,screenshots.url,artworks.url; where id = %1;").arg(id);

    ApiResponse response = makeRequest("/games", body);
    if (!response.success) {
        return artwork;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response.data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "IGDB artwork JSON parse error:" << parseError.errorString();
        return artwork;
    }

    QJsonArray games = doc.array();
    
    if (!games.isEmpty()) {
        QJsonObject game = games[0].toObject();
        
        // Cover
        if (game.contains("cover")) {
            QString coverUrl = game["cover"].toObject()["url"].toString();
            if (!coverUrl.isEmpty()) {
                artwork.boxFront = QUrl("https:" + coverUrl.replace(Constants::API::IGDB_IMG_THUMB, Constants::API::IGDB_IMG_COVER_BIG));
            }
        }
        
        // Screenshots
        if (game.contains("screenshots")) {
            QJsonArray screenshots = game["screenshots"].toArray();
            if (!screenshots.isEmpty()) {
                QString screenshotUrl = screenshots[0].toObject()["url"].toString();
                artwork.screenshot = QUrl("https:" + screenshotUrl.replace(Constants::API::IGDB_IMG_THUMB, Constants::API::IGDB_IMG_SCREENSHOT_BIG));
            }
        }
    }

    return artwork;
}

IGDBProvider::ApiResponse IGDBProvider::makeRequest(const QString &endpoint, const QString &body)
{
    QUrl url(QString(Constants::API::IGDB_BASE_URL) + endpoint);
    QNetworkRequest request(url);

    request.setRawHeader(Constants::API::IGDB_CLIENT_ID_HEADER, m_clientId.toUtf8());
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_accessToken).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");

    QNetworkReply *reply = m_networkManager->post(request, body.toUtf8());
    return waitForReply(reply, Constants::Network::IGDB_TIMEOUT_MS);
}

GameMetadata IGDBProvider::parseGameJson(const QJsonObject &game)
{
    GameMetadata metadata;

    metadata.id = QString::number(game["id"].toInt());
    metadata.providerId = Constants::Providers::IGDB;
    metadata.fetchedAt = QDateTime::currentDateTime();

    metadata.title = game["name"].toString();
    metadata.description = game["summary"].toString();
    
    // Release date
    if (game.contains("first_release_date")) {
        qint64 timestamp = game["first_release_date"].toVariant().toLongLong();
        QDateTime releaseDate = QDateTime::fromSecsSinceEpoch(timestamp);
        metadata.releaseDate = releaseDate.toString(Qt::ISODate);
    }
    
    // Genres
    if (game.contains("genres")) {
        QJsonArray genres = game["genres"].toArray();
        for (const QJsonValue &genreVal : genres) {
            metadata.genres.append(genreVal.toObject()["name"].toString());
        }
    }
    
    // Companies (developer/publisher)
    if (game.contains("involved_companies")) {
        QJsonArray companies = game["involved_companies"].toArray();
        for (const QJsonValue &compVal : companies) {
            QJsonObject comp = compVal.toObject();
            QString companyName = comp["company"].toObject()["name"].toString();
            
            if (comp["developer"].toBool() && metadata.developer.isEmpty()) {
                metadata.developer = companyName;
            }
            if (comp["publisher"].toBool() && metadata.publisher.isEmpty()) {
                metadata.publisher = companyName;
            }
        }
    }
    
    // Rating
    if (game.contains("aggregated_rating")) {
        metadata.rating = game["aggregated_rating"].toDouble() / Constants::API::IGDB_RATING_SCALE;  // Convert to 0-10 scale
    }

    // Players from multiplayer_modes.offlinemax
    if (game.contains("multiplayer_modes")) {
        QJsonArray modes = game["multiplayer_modes"].toArray();
        int maxPlayers = 1;
        for (const QJsonValue &modeVal : modes) {
            int offline = modeVal.toObject()["offlinemax"].toInt();
            if (offline > maxPlayers) {
                maxPlayers = offline;
            }
        }
        metadata.players = maxPlayers;
    }

    return metadata;
}

QString IGDBProvider::mapSystemToIGDB(const QString &system)
{
    // Use SystemResolver for consistent system name mapping
    int systemId = SystemResolver::systemIdByName(system);
    if (systemId == 0) {
        return QString();  // System not found
    }
    
    return SystemResolver::providerName(systemId, Constants::Providers::IGDB);
}

bool IGDBProvider::isAvailable()
{
    return m_authenticated && !m_clientId.isEmpty();
}

} // namespace Remus
