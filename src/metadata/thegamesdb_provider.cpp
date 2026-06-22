#include "thegamesdb_provider.h"
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QSettings>
#include <QDate>
#include "../core/constants/constants.h"

namespace Remus {

TheGamesDBProvider::TheGamesDBProvider(QObject *parent)
    : HttpMetadataProvider(QStringLiteral("thegamesdb"), Constants::Network::THEGAMESDB_RATE_LIMIT_MS, parent) {
    loadRequestCount();
}

void TheGamesDBProvider::setApiKey(const QString &apiKey) {
    m_apiKey = apiKey;
}

QList<SearchResult> TheGamesDBProvider::searchByName(
    const QString &title, const QString &system, const QString &region) {
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
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response.data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "TheGamesDB JSON parse error (search):" << parseError.errorString();
        emit errorOccurred("TheGamesDB JSON parse error: " + parseError.errorString());
        return results;
    }

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

GameMetadata TheGamesDBProvider::getByHash(const QString &hash, const QString &system) {
    Q_UNUSED(hash);
    Q_UNUSED(system);
    // TheGamesDB does not support hash-based lookups; return empty silently.
    // The orchestrator's supportsHashMatch flag prevents this path in normal use.
    return { };
}

GameMetadata TheGamesDBProvider::getById(const QString &id) {
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
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response.data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "TheGamesDB JSON parse error (getById):" << parseError.errorString();
        emit errorOccurred("TheGamesDB JSON parse error: " + parseError.errorString());
        return metadata;
    }

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

QList<GameMetadata> TheGamesDBProvider::fetchGamesByPlatformId(const QString &platformId, int page) {
    QList<GameMetadata> results;
    if (platformId.trimmed().isEmpty() || page < 1 || !isAvailable())
        return results;

    m_rateLimiter->waitIfNeeded();

    QUrl url(QString(Constants::API::THEGAMESDB_BASE_URL) + Constants::API::THEGAMESDB_BY_PLATFORM_ENDPOINT);
    QUrlQuery query;
    if (!m_apiKey.isEmpty())
        query.addQueryItem(QStringLiteral("apikey"), m_apiKey);
    query.addQueryItem(QStringLiteral("id"), platformId);
    query.addQueryItem(QStringLiteral("page"), QString::number(page));
    url.setQuery(query);

    const ApiResponse response = makeRequest(url);
    if (!response.success)
        return results;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(response.data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return results;

    const QJsonObject root = doc.object();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QJsonArray games = data.value(QStringLiteral("games")).toArray();
    results.reserve(games.size());
    for (const QJsonValue &gameVal : games) {
        const GameMetadata metadata = parseGameJson(gameVal.toObject());
        if (!metadata.title.isEmpty())
            results.append(metadata);
    }
    return results;
}

ArtworkUrls TheGamesDBProvider::getArtwork(const QString &id) {
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
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response.data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "TheGamesDB JSON parse error (artwork):" << parseError.errorString();
        return artwork;
    }

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

TheGamesDBProvider::ApiResponse TheGamesDBProvider::makeRequest(const QUrl &url) {
    if (m_monthlyRequestCount >= Constants::Network::THEGAMESDB_BLOCK_THRESHOLD) {
        qCritical() << "TheGamesDB: Monthly request limit reached (" << m_monthlyRequestCount << "/"
                    << Constants::Network::THEGAMESDB_MONTHLY_LIMIT << ") — blocking further requests until next month";
        ApiResponse blocked;
        blocked.error = QStringLiteral("TheGamesDB monthly request limit reached");
        return blocked;
    }

    incrementRequestCount();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);

    QNetworkReply *reply = m_networkManager->get(request);
    return waitForReply(reply, Constants::Network::THEGAMESDB_TIMEOUT_MS);
}

GameMetadata TheGamesDBProvider::parseGameJson(const QJsonObject &game) {
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

    // Genres — API v2 returns an array of objects with "id" and "name".
    // Older responses may return integers only; skip those since we have no
    // lookup table and would produce meaningless values.
    QJsonArray genres = game["genres"].toArray();
    for (const QJsonValue &genreVal : genres) {
        if (genreVal.isObject()) {
            const QString name = genreVal.toObject()["name"].toString();
            if (!name.isEmpty()) {
                metadata.genres.append(name);
            }
        }
    }

    // Players
    if (game.contains("players")) {
        metadata.players = game["players"].toInt();
    }

    return metadata;
}

bool TheGamesDBProvider::isAvailable() {
    // Unavailable when the monthly request cap has been reached.
    if (m_monthlyRequestCount >= Constants::Network::THEGAMESDB_BLOCK_THRESHOLD) {
        return false;
    }
    return true;
}

void TheGamesDBProvider::loadRequestCount() {
    m_currentMonth = QDate::currentDate().toString(QStringLiteral("yyyy-MM"));
    QSettings settings;
    m_monthlyRequestCount = settings.value(QStringLiteral("tgdb/requests/%1").arg(m_currentMonth), 0).toInt();
    qDebug() << "TheGamesDB: Loaded request count for" << m_currentMonth << ":" << m_monthlyRequestCount << "/"
             << Constants::Network::THEGAMESDB_MONTHLY_LIMIT;
}

void TheGamesDBProvider::incrementRequestCount() {
    QString month = QDate::currentDate().toString(QStringLiteral("yyyy-MM"));
    if (month != m_currentMonth) {
        m_currentMonth = month;
        m_monthlyRequestCount = 0;
    }

    ++m_monthlyRequestCount;

    QSettings settings;
    settings.setValue(QStringLiteral("tgdb/requests/%1").arg(m_currentMonth), m_monthlyRequestCount);

    if (m_monthlyRequestCount == Constants::Network::THEGAMESDB_WARN_THRESHOLD) {
        qWarning() << "TheGamesDB: 80% of monthly request limit reached (" << m_monthlyRequestCount << "/"
                   << Constants::Network::THEGAMESDB_MONTHLY_LIMIT << ")";
        emit errorOccurred(QStringLiteral("TheGamesDB: approaching monthly request limit (%1/%2)")
                .arg(m_monthlyRequestCount)
                .arg(Constants::Network::THEGAMESDB_MONTHLY_LIMIT));
    } else if (m_monthlyRequestCount == Constants::Network::THEGAMESDB_BLOCK_THRESHOLD) {
        qCritical() << "TheGamesDB: 95% of monthly limit reached (" << m_monthlyRequestCount << "/"
                    << Constants::Network::THEGAMESDB_MONTHLY_LIMIT << ") — requests will be blocked";
    }
}

} // namespace Remus
