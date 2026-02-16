#include "igdb_provider.h"
#include "../core/system_resolver.h"
#include "../core/constants/providers.h"
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

IGDBProvider::IGDBProvider(QObject *parent)
    : MetadataProvider(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_rateLimiter(new RateLimiter(this))
{
    m_rateLimiter->setInterval(REQUEST_DELAY_MS);
}

void IGDBProvider::setCredentials(const QString &clientId, const QString &clientSecret)
{
    m_clientId = clientId;
    m_clientSecret = clientSecret;
    m_authenticated = !clientId.isEmpty() && !clientSecret.isEmpty();
}

bool IGDBProvider::authenticate()
{
    // Check if token is still valid
    if (!m_accessToken.isEmpty() && QDateTime::currentDateTime() < m_tokenExpiry) {
        return true;
    }

    // Request new access token from Twitch
    QUrl url("https://id.twitch.tv/oauth2/token");
    QUrlQuery query;
    query.addQueryItem("client_id", m_clientId);
    query.addQueryItem("client_secret", m_clientSecret);
    query.addQueryItem("grant_type", "client_credentials");

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QNetworkReply *reply = m_networkManager->post(request, query.toString().toUtf8());
    
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object();
        
        m_accessToken = obj["access_token"].toString();
        int expiresIn = obj["expires_in"].toInt();
        m_tokenExpiry = QDateTime::currentDateTime().addSecs(expiresIn);
        
        reply->deleteLater();
        return true;
    }

    reply->deleteLater();
    return false;
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
    QJsonDocument doc = QJsonDocument::fromJson(response.data);
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
                           "involved_companies.publisher,aggregated_rating; where id = %1;")
                       .arg(id);

    ApiResponse response = makeRequest("/games", body);
    if (!response.success) {
        emit errorOccurred(response.error);
        return metadata;
    }

    QJsonDocument doc = QJsonDocument::fromJson(response.data);
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

    QJsonDocument doc = QJsonDocument::fromJson(response.data);
    QJsonArray games = doc.array();
    
    if (!games.isEmpty()) {
        QJsonObject game = games[0].toObject();
        
        // Cover
        if (game.contains("cover")) {
            QString coverUrl = game["cover"].toObject()["url"].toString();
            if (!coverUrl.isEmpty()) {
                artwork.boxFront = QUrl("https:" + coverUrl.replace("t_thumb", "t_cover_big"));
            }
        }
        
        // Screenshots
        if (game.contains("screenshots")) {
            QJsonArray screenshots = game["screenshots"].toArray();
            if (!screenshots.isEmpty()) {
                QString screenshotUrl = screenshots[0].toObject()["url"].toString();
                artwork.screenshot = QUrl("https:" + screenshotUrl.replace("t_thumb", "t_screenshot_big"));
            }
        }
    }

    return artwork;
}

IGDBProvider::ApiResponse IGDBProvider::makeRequest(const QString &endpoint, const QString &body)
{
    ApiResponse response;

    QUrl url("https://api.igdb.com/v4" + endpoint);
    QNetworkRequest request(url);
    
    request.setRawHeader("Client-ID", m_clientId.toUtf8());
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_accessToken).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");

    QNetworkReply *reply = m_networkManager->post(request, body.toUtf8());
    
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(30000);

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
        metadata.rating = game["aggregated_rating"].toDouble() / 10.0;  // Convert to 0-10 scale
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
