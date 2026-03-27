#include "screenscraper_provider.h"
#include "../core/system_resolver.h"
#include "../core/constants/providers.h"
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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
    : HttpMetadataProvider(Constants::Network::SCREENSCRAPER_RATE_LIMIT_MS, parent)
{
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
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response.data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "ScreenScraper JSON parse error (search):" << parseError.errorString();
        emit errorOccurred("ScreenScraper JSON parse error: " + parseError.errorString());
        return results;
    }

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
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);

    QNetworkReply *reply = m_networkManager->get(request);
    ApiResponse response = waitForReply(reply, Constants::Network::SCREENSCRAPER_TIMEOUT_MS);

    if (!response.success && response.httpStatusCode == Constants::Network::HTTP_TOO_MANY_REQUESTS) {
        emit rateLimitReached();
    }

    return response;
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
