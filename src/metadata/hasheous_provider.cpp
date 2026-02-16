#include "hasheous_provider.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QEventLoop>
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

HasheousProvider::HasheousProvider(QObject *parent)
    : MetadataProvider(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_rateLimiter(new RateLimiter(this))
{
    m_rateLimiter->setInterval(1000); // 1 req/second (conservative)
    qInfo() << "Hasheous provider initialized (no auth required)";
}

QString HasheousProvider::detectHashType(const QString &hash) const
{
    // Hasheous supports MD5 (32 chars) and SHA1 (40 chars)
    // Use HashAlgorithms utility for consistent hash detection
    return Constants::HashAlgorithms::detectFromLength(hash.length());
}

QJsonObject HasheousProvider::makeRequest(const QString &endpoint, const QUrlQuery &params)
{
    m_rateLimiter->waitIfNeeded();
    
    QUrl url(QString(API_BASE) + endpoint);
    url.setQuery(params);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Remus/1.0");
    
    QNetworkReply *reply = m_network->get(request);
    
    // Synchronous wait for response
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    QJsonObject result;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        result = doc.object();
    } else {
        qWarning() << "Hasheous API error:" << reply->errorString();
        emit errorOccurred("Hasheous API error: " + reply->errorString());
    }
    
    reply->deleteLater();
    return result;
}

GameMetadata HasheousProvider::parseGameJson(const QJsonObject &json) const
{
    GameMetadata metadata;
    
    if (json.isEmpty() || !json.contains("data")) {
        return metadata;
    }
    
    QJsonObject data = json["data"].toObject();
    
    // Basic info (proxied from IGDB)
    metadata.title = data["name"].toString();
    metadata.description = data["summary"].toString();
    metadata.releaseDate = data["first_release_date"].toString();
    
    // Genres (array)
    QJsonArray genresArray = data["genres"].toArray();
    for (const QJsonValue &genre : genresArray) {
        metadata.genres.append(genre.toString());
    }
    
    // Rating (IGDB uses 0-100 scale, convert to 0-10)
    if (data.contains("aggregated_rating")) {
        metadata.rating = data["aggregated_rating"].toDouble() / 10.0;
    }
    
    // Cover art URL
    if (data.contains("cover")) {
        QJsonObject cover = data["cover"].toObject();
        QString coverUrl = cover["url"].toString();
        if (!coverUrl.isEmpty()) {
            // Hasheous provides full IGDB image URLs
            metadata.boxArtUrl = coverUrl;
        }
    }
    
    // RetroAchievements ID (if available)
    if (data.contains("retroachievements_id")) {
        metadata.externalIds["retroachievements"] = data["retroachievements_id"].toString();
    }
    
    // IGDB ID
    if (data.contains("igdb_id")) {
        metadata.externalIds["igdb"] = QString::number(data["igdb_id"].toInt());
    }
    
    return metadata;
}

QList<SearchResult> HasheousProvider::searchByName(const QString &title,
                                                                   const QString &system,
                                                                   const QString &region)
{
    Q_UNUSED(title);
    Q_UNUSED(system);
    Q_UNUSED(region);
    
    // Hasheous doesn't support name-based search
    qWarning() << "Hasheous does not support name-based search, use hash matching instead";
    emit errorOccurred("Hasheous only supports hash-based matching");
    
    return QList<SearchResult>();
}

GameMetadata HasheousProvider::getByHash(const QString &hash, const QString &system)
{
    Q_UNUSED(system); // Hasheous uses hash only, system is inferred
    
    QString hashType = detectHashType(hash);
    
    if (hashType.isEmpty()) {
        qWarning() << "Hasheous: Invalid hash length, expected MD5 (32) or SHA1 (40), got:" << hash.length();
        emit errorOccurred("Invalid hash length for Hasheous");
        return GameMetadata();
    }
    
    qInfo() << "Hasheous: Looking up hash" << hash << "(" << hashType << ")";
    
    QUrlQuery params;
    params.addQueryItem("hash", hash.toLower());
    params.addQueryItem("hash_type", hashType);
    
    QJsonObject response = makeRequest("/lookup", params);
    
    if (!response.isEmpty() && response.contains("data")) {
        GameMetadata metadata = parseGameJson(response);
        
        if (!metadata.title.isEmpty()) {
            qInfo() << "Hasheous: Found match:" << metadata.title;
            emit metadataFetched(metadata);
            return metadata;
        }
    }
    
    qInfo() << "Hasheous: No match found for hash:" << hash;
    return GameMetadata();
}

GameMetadata HasheousProvider::getById(const QString &id)
{
    Q_UNUSED(id);
    
    // Hasheous doesn't support ID-based lookup (it uses hashes)
    qWarning() << "Hasheous does not support ID-based lookup";
    emit errorOccurred("Hasheous only supports hash-based matching");
    
    return GameMetadata();
}

ArtworkUrls HasheousProvider::getArtwork(const QString &id)
{
    Q_UNUSED(id);
    
    // Artwork is included in the hash lookup response
    qWarning() << "Hasheous artwork is included in hash lookup, use getByHash() instead";
    
    return ArtworkUrls();
}

} // namespace Remus
