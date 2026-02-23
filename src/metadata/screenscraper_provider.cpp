#include "screenscraper_provider.h"
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

ScreenScraperProvider::ScreenScraperProvider(QObject *parent)
    : MetadataProvider(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_rateLimiter(new RateLimiter(this))
{
    m_rateLimiter->setInterval(Constants::Network::SCREENSCRAPER_RATE_LIMIT_MS);
}

void ScreenScraperProvider::setCredentials(const QString &username, const QString &password)
{
    MetadataProvider::setCredentials(username, password);
}

void ScreenScraperProvider::setDeveloperCredentials(const QString &devId, const QString &devPassword)
{
    m_devId = devId;
    m_devPassword = devPassword;
}

QList<SearchResult> ScreenScraperProvider::searchByName(const QString &title,
                                                         const QString &system,
                                                         const QString &region)
{
    QList<SearchResult> results;

    if (!m_authenticated) {
        emit errorOccurred("ScreenScraper requires authentication");
        return results;
    }

    m_rateLimiter->waitIfNeeded();

    // Build API URL - jeuRecherche.php for name search
    QUrl url(QString(Constants::API::SCREENSCRAPER_BASE_URL) + Constants::API::SCREENSCRAPER_JEURECHERCHE_ENDPOINT);
    QUrlQuery query;
    
    query.addQueryItem("devid", m_devId);
    query.addQueryItem("devpassword", m_devPassword);
    query.addQueryItem("softname", m_softwareName);
    query.addQueryItem("output", "json");
    query.addQueryItem("ssid", m_username);
    query.addQueryItem("sspassword", m_password);
    query.addQueryItem("recherche", title);
    
    if (!system.isEmpty()) {
        QString ssSystem = mapSystemToScreenScraper(system);
        if (!ssSystem.isEmpty()) {
            query.addQueryItem("systemeid", ssSystem);
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
    
    if (root.contains("response") && root["response"].isObject()) {
        QJsonObject game = root["response"].toObject()["jeu"].toObject();
        
        SearchResult result;
        result.id = QString::number(game["id"].toInt());
        result.title = game["nom"].toObject()["text"].toString();
        result.system = system;
        
        if (game.contains("date")) {
            QString date = game["date"].toString();
            if (date.length() >= 4) {
                result.releaseYear = date.left(4).toInt();
            }
        }
        
        result.matchScore = 0.9f;  // Name-based search has lower confidence
        results.append(result);
    }

    return results;
}

GameMetadata ScreenScraperProvider::getByHash(const QString &hash, const QString &system)
{
    GameMetadata metadata;

    if (!m_authenticated) {
        emit errorOccurred("ScreenScraper requires authentication");
        return metadata;
    }

    m_rateLimiter->waitIfNeeded();

    // Build API URL - jeuInfos.php for hash-based ROM identification
    QUrl url(QString(Constants::API::SCREENSCRAPER_BASE_URL) + Constants::API::SCREENSCRAPER_JEUINFOS_ENDPOINT);
    QUrlQuery query;
    
    query.addQueryItem("devid", m_devId);
    query.addQueryItem("devpassword", m_devPassword);
    query.addQueryItem("softname", m_softwareName);
    query.addQueryItem("output", "json");
    query.addQueryItem("ssid", m_username);
    query.addQueryItem("sspassword", m_password);
    
    // Detect hash type and add to query
    QString hashType = detectHashType(hash);
    query.addQueryItem(hashType, hash);
    
    // Add system filter
    QString ssSystem = mapSystemToScreenScraper(system);
    if (!ssSystem.isEmpty()) {
        query.addQueryItem("systemeid", ssSystem);
    }

    url.setQuery(query);

    ApiResponse response = makeRequest(url);
    if (!response.success) {
        emit errorOccurred(response.error);
        return metadata;
    }

    metadata = parseGameJson(response.data);
    return metadata;
}

GameMetadata ScreenScraperProvider::getById(const QString &id)
{
    GameMetadata metadata;

    if (!m_authenticated) {
        emit errorOccurred("ScreenScraper requires authentication");
        return metadata;
    }

    m_rateLimiter->waitIfNeeded();

    // Build API URL
    QUrl url(QString(Constants::API::SCREENSCRAPER_BASE_URL) + Constants::API::SCREENSCRAPER_GETGAME_ENDPOINT);
    QUrlQuery query;
    
    query.addQueryItem("devid", m_devId);
    query.addQueryItem("devpassword", m_devPassword);
    query.addQueryItem("softname", m_softwareName);
    query.addQueryItem("output", "json");
    query.addQueryItem("ssid", m_username);
    query.addQueryItem("sspassword", m_password);
    query.addQueryItem("gameid", id);

    url.setQuery(query);

    ApiResponse response = makeRequest(url);
    if (!response.success) {
        emit errorOccurred(response.error);
        return metadata;
    }

    metadata = parseGameJson(response.data);
    return metadata;
}

ArtworkUrls ScreenScraperProvider::getArtwork(const QString &id)
{
    ArtworkUrls artwork;

    if (!m_authenticated) {
        emit errorOccurred("ScreenScraper requires authentication");
        return artwork;
    }

    m_rateLimiter->waitIfNeeded();

    QUrl url(QString(Constants::API::SCREENSCRAPER_BASE_URL) + Constants::API::SCREENSCRAPER_GETGAME_ENDPOINT);
    QUrlQuery query;

    query.addQueryItem("devid", m_devId);
    query.addQueryItem("devpassword", m_devPassword);
    query.addQueryItem("softname", m_softwareName);
    query.addQueryItem("output", "json");
    query.addQueryItem("ssid", m_username);
    query.addQueryItem("sspassword", m_password);
    query.addQueryItem("gameid", id);

    url.setQuery(query);

    ApiResponse response = makeRequest(url);
    if (!response.success) {
        emit errorOccurred(response.error);
        return artwork;
    }

    artwork = parseArtworkJson(response.data);

    return artwork;
}

ScreenScraperProvider::ApiResponse ScreenScraperProvider::makeRequest(const QUrl &url)
{
    ApiResponse response;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);

    QNetworkReply *reply = m_networkManager->get(request);
    
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(Constants::Network::SCREENSCRAPER_TIMEOUT_MS);

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
            
            // Check for rate limiting
            if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 429) {
                emit rateLimitReached();
            }
        }
    } else {
        response.success = false;
        response.error = "Request timeout";
    }

    reply->deleteLater();
    return response;
}

GameMetadata ScreenScraperProvider::parseGameJson(const QByteArray &json)
{
    GameMetadata metadata;

    QJsonDocument doc = QJsonDocument::fromJson(json);
    QJsonObject root = doc.object();

    if (!root.contains("response")) {
        return metadata;
    }

    QJsonObject response = root["response"].toObject();
    QJsonObject game = response["jeu"].toObject();

    metadata.id = QString::number(game["id"].toInt());
    metadata.providerId = Constants::Providers::SCREENSCRAPER;
    metadata.fetchedAt = QDateTime::currentDateTime();

    // Title (prefer US/World English)
    if (game.contains("noms")) {
        QJsonArray names = game["noms"].toArray();
        for (const QJsonValue &nameVal : names) {
            QJsonObject nameObj = nameVal.toObject();
            QString region = nameObj["region"].toString();
            if (region == "us" || region == "wor") {
                metadata.title = nameObj["text"].toString();
                break;
            }
        }
        if (metadata.title.isEmpty() && !names.isEmpty()) {
            metadata.title = names[0].toObject()["text"].toString();
        }
    }

    // System
    if (game.contains("systeme")) {
        metadata.system = game["systeme"].toObject()["text"].toString();
    }

    // Release date
    if (game.contains("dates")) {
        QJsonArray dates = game["dates"].toArray();
        if (!dates.isEmpty()) {
            metadata.releaseDate = dates[0].toObject()["text"].toString();
        }
    }

    // Developer/Publisher
    if (game.contains("developpeur")) {
        metadata.developer = game["developpeur"].toObject()["text"].toString();
    }
    if (game.contains("editeur")) {
        metadata.publisher = game["editeur"].toObject()["text"].toString();
    }

    // Genres
    if (game.contains("genres")) {
        QJsonArray genres = game["genres"].toArray();
        for (const QJsonValue &genreVal : genres) {
            QString genre = genreVal.toObject()["text"].toString();
            metadata.genres.append(genre);
        }
    }

    // Players
    if (game.contains("joueurs")) {
        metadata.players = game["joueurs"].toObject()["text"].toString().toInt();
    }

    // Rating
    if (game.contains("note")) {
        metadata.rating = game["note"].toObject()["text"].toString().toFloat();
    }

    // Description (synopsis)
    if (game.contains("synopsis")) {
        QJsonArray synopsis = game["synopsis"].toArray();
        for (const QJsonValue &synVal : synopsis) {
            QJsonObject synObj = synVal.toObject();
            if (synObj["langue"].toString() == "en") {
                metadata.description = synObj["text"].toString();
                break;
            }
        }
    }

    ArtworkUrls artwork = parseArtworkFromGameObject(game);
    if (!artwork.boxFront.isEmpty()) {
        metadata.boxArtUrl = artwork.boxFront.toString();
    }

    return metadata;
}

ArtworkUrls ScreenScraperProvider::parseArtworkJson(const QByteArray &json) const
{
    ArtworkUrls artwork;

    QJsonDocument doc = QJsonDocument::fromJson(json);
    QJsonObject root = doc.object();

    if (!root.contains("response")) {
        return artwork;
    }

    QJsonObject response = root["response"].toObject();
    QJsonObject game = response["jeu"].toObject();

    return parseArtworkFromGameObject(game);
}

ArtworkUrls ScreenScraperProvider::parseArtworkFromGameObject(const QJsonObject &game) const
{
    ArtworkUrls artwork;

    QJsonArray mediaArray;
    if (game.contains("medias")) {
        if (game["medias"].isArray()) {
            mediaArray = game["medias"].toArray();
        } else if (game["medias"].isObject()) {
            QJsonObject mediasObj = game["medias"].toObject();
            if (mediasObj.contains("media") && mediasObj["media"].isArray()) {
                mediaArray = mediasObj["media"].toArray();
            }
        }
    }

    for (const QJsonValue &mediaVal : mediaArray) {
        QJsonObject media = mediaVal.toObject();
        QString type = media["type"].toString().toLower();
        QString url = pickArtworkUrl(media);

        if (type.isEmpty() || url.isEmpty()) {
            continue;
        }

        if ((type.contains("box-2d") || type.contains("box2d") || type == "box") && type.contains("back")) {
            if (artwork.boxBack.isEmpty()) {
                artwork.boxBack = QUrl(url);
            }
        } else if (type.contains("box-2d") || type.contains("box2d") || type == "box") {
            if (artwork.boxFront.isEmpty()) {
                artwork.boxFront = QUrl(url);
            }
        } else if (type.contains("box-3d") || type.contains("box3d")) {
            if (artwork.boxFull.isEmpty()) {
                artwork.boxFull = QUrl(url);
            }
        } else if (type.contains("screenshot") || type == "ss" || type.contains("screen")) {
            if (artwork.screenshot.isEmpty()) {
                artwork.screenshot = QUrl(url);
            }
        } else if (type.contains("title")) {
            if (artwork.titleScreen.isEmpty()) {
                artwork.titleScreen = QUrl(url);
            }
        } else if (type.contains("clearlogo")) {
            if (artwork.clearLogo.isEmpty()) {
                artwork.clearLogo = QUrl(url);
            }
        } else if (type.contains("logo") || type.contains("wheel")) {
            if (artwork.logo.isEmpty()) {
                artwork.logo = QUrl(url);
            }
        } else if (type.contains("marquee") || type.contains("banner")) {
            if (artwork.banner.isEmpty()) {
                artwork.banner = QUrl(url);
            }
        }
    }

    return artwork;
}

QString ScreenScraperProvider::pickArtworkUrl(const QJsonObject &media) const
{
    QString url = media["url"].toString();
    if (!url.isEmpty()) {
        return url;
    }

    url = media["url_ori"].toString();
    if (!url.isEmpty()) {
        return url;
    }

    url = media["url_original"].toString();
    if (!url.isEmpty()) {
        return url;
    }

    url = media["url_thumb"].toString();
    if (!url.isEmpty()) {
        return url;
    }

    return media["url_small"].toString();
}

QString ScreenScraperProvider::mapSystemToScreenScraper(const QString &system)
{
    // Use SystemResolver for consistent system name mapping
    int systemId = SystemResolver::systemIdByName(system);
    if (systemId == 0) {
        return QString();  // System not found
    }
    
    return SystemResolver::providerName(systemId, Constants::Providers::SCREENSCRAPER);
}

QString ScreenScraperProvider::detectHashType(const QString &hash)
{
    QString cleaned = hash.toLower().trimmed();
    
    // Use HashAlgorithms utility for consistent hash detection
    QString detected = Constants::HashAlgorithms::detectFromLength(cleaned.length());
    if (!detected.isEmpty()) {
        return detected == Constants::HashAlgorithms::CRC32 ? "crc" : detected;
    }
    
    return "crc";  // Default to CRC32
}

bool ScreenScraperProvider::isAvailable()
{
    if (!m_authenticated || m_devId.isEmpty() || m_devPassword.isEmpty()) {
        return false;
    }

    m_rateLimiter->waitIfNeeded();

    QUrl url(QString(Constants::API::SCREENSCRAPER_BASE_URL) + "/ssuserInfos.php");
    QUrlQuery query;
    query.addQueryItem("devid", m_devId);
    query.addQueryItem("devpassword", m_devPassword);
    query.addQueryItem("softname", m_softwareName);
    query.addQueryItem("ssid", m_username);
    query.addQueryItem("sspassword", m_password);
    url.setQuery(query);

    ApiResponse response = makeRequest(url);
    return response.success;
}

} // namespace Remus
