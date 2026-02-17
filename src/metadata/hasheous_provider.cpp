#include "hasheous_provider.h"
#include "../core/constants/providers.h"
#include "../core/constants/settings.h"
#include "../core/system_resolver.h"
#include "../core/constants/systems.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QEventLoop>
#include <QDebug>
#include <QSet>
#include <QTimeZone>
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

namespace {
constexpr const char *DEFAULT_CLIENT_API_KEY = "yFCtSh1zpMqwdOx27SB9huyyMPMGqLLXm2GlE71SNtvJk9-wnKEqBNhqiJ7PZcOD";
}

HasheousProvider::HasheousProvider(QObject *parent)
    : MetadataProvider(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_rateLimiter(new RateLimiter(this))
{
    QSettings settings;
    m_clientApiKey = settings.value(Constants::Settings::Providers::HASHEOUS_CLIENT_API_KEY,
                                    QString::fromLatin1(DEFAULT_CLIENT_API_KEY)).toString();

    m_rateLimiter->setInterval(1000); // 1 req/second (conservative)
    qInfo() << "Hasheous provider initialized (no auth required)";
}

QString HasheousProvider::detectHashType(const QString &hash) const
{
    return Constants::HashAlgorithms::detectFromLength(hash.trimmed().length());
}

QJsonObject HasheousProvider::makeRequest(const QString &endpoint, const QUrlQuery &params)
{
    m_rateLimiter->waitIfNeeded();
    
    QUrl url(QString(API_BASE) + endpoint);
    url.setQuery(params);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Remus/1.0");
    // MetadataProxy endpoints require a client API key
    if (!m_clientApiKey.isEmpty()) {
        request.setRawHeader("X-Client-API-Key", m_clientApiKey.toUtf8());
    }
    
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
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 404) {
            qDebug() << "Hasheous API 404 (expected miss):" << url.toString();
        } else {
            qWarning() << "Hasheous API error:" << reply->errorString()
                        << "Status:" << statusCode
                        << "URL:" << url.toString();
            emit errorOccurred("Hasheous API error: " + reply->errorString());
        }
    }
    
    reply->deleteLater();
    return result;
}

QJsonObject HasheousProvider::makePostRequest(const QString &endpoint, const QJsonObject &body, const QUrlQuery &params)
{
    m_rateLimiter->waitIfNeeded();
    
    QUrl url(QString(API_BASE) + endpoint);
    if (!params.isEmpty()) {
        url.setQuery(params);
    }
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Remus/1.0");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QByteArray postData = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_network->post(request, postData);
    
    // Synchronous wait for response
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    QJsonObject result;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isObject()) {
            result = doc.object();
        }
    } else {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 404) {
            qDebug() << "Hasheous POST 404 (expected miss):" << url.toString();
        } else {
            qWarning() << "Hasheous POST error:" << reply->errorString()
                        << "Status:" << statusCode
                        << "URL:" << url.toString();
            emit errorOccurred("Hasheous API error: " + reply->errorString());
        }
    }
    
    reply->deleteLater();
    return result;
}

GameMetadata HasheousProvider::parseGameJson(const QJsonObject &json) const
{
    GameMetadata metadata;
    
    if (json.isEmpty()) {
        return metadata;
    }
    
    // Hasheous response has: id, name, metadata[], signatures[], attributes[]
    metadata.title = json["name"].toString();
    metadata.id = QString::number(json["id"].toInt());
    
    // Extract external IDs from metadata array
    QJsonArray metadataArray = json["metadata"].toArray();
    int igdbId = 0;
    for (const QJsonValue &meta : metadataArray) {
        QJsonObject metaObj = meta.toObject();
        QString source = metaObj["source"].toString();
        QString immutableId = metaObj["immutableId"].toString();
        
        if (source == "IGDB" && !immutableId.isEmpty()) {
            igdbId = immutableId.toInt();
            metadata.externalIds["igdb"] = immutableId;
        } else if (source == "TheGamesDB" && !immutableId.isEmpty()) {
            metadata.externalIds["thegamesdb"] = immutableId;
        } else if (source == "RetroAchievements" && !immutableId.isEmpty()) {
            metadata.externalIds["retroachievements"] = immutableId;
        }
    }
    
    // Check DAT signatures (No-Intro, Redump, TOSEC, etc.)
    QJsonArray signatures = json["signatures"].toArray();
    QStringList datSources;
    for (const QJsonValue &sig : signatures) {
        datSources.append(sig.toString());
    }
    if (!datSources.isEmpty()) {
        metadata.externalIds["dat_sources"] = datSources.join(",");
    }
    
    // Extract artwork from attributes
    QJsonArray attributes = json["attributes"].toArray();
    for (const QJsonValue &attr : attributes) {
        QJsonObject attrObj = attr.toObject();
        if (attrObj["attributeName"].toString() == "Logo") {
            QString link = attrObj["link"].toString();
            if (!link.isEmpty()) {
                metadata.boxArtUrl = "https://hasheous.org" + link;
            }
            break;
        }
    }
    
    return metadata;
}

GameMetadata HasheousProvider::fetchIgdbMetadata(int igdbId)
{
    if (igdbId <= 0) {
        return GameMetadata();
    }
    
    qInfo() << "Hasheous: Fetching IGDB metadata via MetadataProxy for ID:" << igdbId;
    
    // MetadataProxy uses query params: ?Id=<igdbId>&expandColumns=...
    QUrlQuery params;
    params.addQueryItem("Id", QString::number(igdbId));
    params.addQueryItem("expandColumns", "age_ratings,alternative_names,collections,cover,dlcs,expanded_games,franchise,franchises,game_modes,genres,involved_companies,platforms,ports,remakes,screenshots,similar_games,videos");
    
    QJsonObject igdbGame = makeRequest("/MetadataProxy/IGDB/Game", params);
    
    if (igdbGame.isEmpty()) {
        qWarning() << "Hasheous: MetadataProxy returned empty for IGDB ID:" << igdbId;
        return GameMetadata();
    }
    
    GameMetadata metadata;
    metadata.title = igdbGame["name"].toString();
    metadata.description = igdbGame["summary"].toString();
    metadata.externalIds["igdb"] = QString::number(igdbId);
    
    // Release date — MetadataProxy returns ISO 8601 string (e.g., "1991-06-23T00:00:00+00:00")
    if (igdbGame.contains("first_release_date")) {
        QJsonValue dateVal = igdbGame["first_release_date"];
        if (dateVal.isString()) {
            QDateTime dt = QDateTime::fromString(dateVal.toString(), Qt::ISODate);
            if (dt.isValid()) {
                metadata.releaseDate = dt.toUTC().date().toString("yyyy-MM-dd");
            }
        } else if (dateVal.isDouble()) {
            // Fallback: direct IGDB uses Unix timestamp
            QDateTime dt = QDateTime::fromSecsSinceEpoch(dateVal.toInteger(), QTimeZone::UTC);
            metadata.releaseDate = dt.toUTC().date().toString("yyyy-MM-dd");
        }
    }
    
    // Genres — MetadataProxy returns object keyed by ID: {"8": {"name": "Platform"}, ...}
    QJsonValue genresVal = igdbGame["genres"];
    if (genresVal.isObject()) {
        QJsonObject genresObj = genresVal.toObject();
        for (auto it = genresObj.begin(); it != genresObj.end(); ++it) {
            QString name = it.value().toObject()["name"].toString();
            if (!name.isEmpty()) {
                metadata.genres.append(name);
            }
        }
    } else if (genresVal.isArray()) {
        // Fallback for array format
        QJsonArray genresArr = genresVal.toArray();
        for (const QJsonValue &genre : genresArr) {
            QString name = genre.toObject()["name"].toString();
            if (!name.isEmpty()) {
                metadata.genres.append(name);
            }
        }
    }
    
    // Rating (IGDB uses 0-100 scale, convert to 0-10)
    if (igdbGame.contains("aggregated_rating")) {
        metadata.rating = igdbGame["aggregated_rating"].toDouble() / 10.0;
    }
    
    // Cover art
    if (igdbGame.contains("cover")) {
        QJsonObject cover = igdbGame["cover"].toObject();
        QString coverUrl = cover["url"].toString();
        if (!coverUrl.isEmpty()) {
            // Upgrade to high-res
            coverUrl.replace("t_thumb", "t_1080p");
            if (coverUrl.startsWith("//")) {
                coverUrl = "https:" + coverUrl;
            }
            metadata.boxArtUrl = coverUrl;
        }
    }

    // Screenshots (object keyed by ID, same pattern as genres)
    auto normalizeShot = [](const QString &url) {
        QString u = url;
        if (u.isEmpty()) return u;
        u.replace("t_thumb", "t_1080p");
        if (u.startsWith("//")) {
            u = "https:" + u;
        }
        return u;
    };

    QJsonValue screenshotsVal = igdbGame["screenshots"];
    if (screenshotsVal.isObject()) {
        QJsonObject shotsObj = screenshotsVal.toObject();
        for (auto it = shotsObj.begin(); it != shotsObj.end(); ++it) {
            QString shotUrl = normalizeShot(it.value().toObject()["url"].toString());
            if (!shotUrl.isEmpty()) {
                metadata.screenshotUrls.append(shotUrl);
            }
        }
    } else if (screenshotsVal.isArray()) {
        QJsonArray shotsArr = screenshotsVal.toArray();
        for (const QJsonValue &shotVal : shotsArr) {
            QString shotUrl = normalizeShot(shotVal.toObject()["url"].toString());
            if (!shotUrl.isEmpty()) {
                metadata.screenshotUrls.append(shotUrl);
            }
        }
    }
    
    // Companies — MetadataProxy returns object keyed by ID, with company as integer
    // The developer/publisher flags are always false in MetadataProxy responses (known bug),
    // so we resolve each company and check its developed/published game ID lists instead.
    QJsonValue companiesVal = igdbGame["involved_companies"];
    if (companiesVal.isObject() || companiesVal.isArray()) {
        // Collect company IDs from either object or array format
        QList<int> companyIds;
        if (companiesVal.isObject()) {
            QJsonObject companiesObj = companiesVal.toObject();
            for (auto it = companiesObj.begin(); it != companiesObj.end(); ++it) {
                QJsonObject comp = it.value().toObject();
                // First check if flags actually work (future MetadataProxy fix)
                bool isDev = comp["developer"].toBool();
                bool isPub = comp["publisher"].toBool();
                QJsonValue companyVal = comp["company"];
                
                if (isDev || isPub) {
                    // Flags are set — resolve name directly
                    QString companyName;
                    if (companyVal.isObject()) {
                        companyName = companyVal.toObject()["name"].toString();
                    } else if (companyVal.isDouble()) {
                        int companyId = companyVal.toInt();
                        if (m_companyCache.contains(companyId)) {
                            companyName = m_companyCache.value(companyId);
                        } else {
                            QUrlQuery compParams;
                            compParams.addQueryItem("Id", QString::number(companyId));
                            QJsonObject compData = makeRequest("/MetadataProxy/IGDB/Company", compParams);
                            companyName = compData["name"].toString();
                            if (!companyName.isEmpty()) {
                                m_companyCache.insert(companyId, companyName);
                            }
                        }
                    }
                    if (!companyName.isEmpty()) {
                        if (isDev && metadata.developer.isEmpty())
                            metadata.developer = companyName;
                        if (isPub && metadata.publisher.isEmpty())
                            metadata.publisher = companyName;
                    }
                } else if (companyVal.isDouble()) {
                    // Flags broken — collect ID for role lookup
                    companyIds.append(companyVal.toInt());
                }
            }
        } else {
            // Array format fallback
            QJsonArray companiesArr = companiesVal.toArray();
            for (const QJsonValue &compVal : companiesArr) {
                QJsonObject comp = compVal.toObject();
                QJsonObject company = comp["company"].toObject();
                QString companyName = company["name"].toString();
                if (!companyName.isEmpty()) {
                    if (comp["developer"].toBool() && metadata.developer.isEmpty())
                        metadata.developer = companyName;
                    if (comp["publisher"].toBool() && metadata.publisher.isEmpty())
                        metadata.publisher = companyName;
                }
            }
        }
        
        // If flags were broken, resolve companies and determine roles from their game lists
        int gameIdInt = igdbId;
        for (int companyId : companyIds) {
            if (!metadata.developer.isEmpty() && !metadata.publisher.isEmpty())
                break; // Both roles filled
            
            QString companyName;
            QJsonObject compData;
            if (m_companyCache.contains(companyId)) {
                companyName = m_companyCache.value(companyId);
                QUrlQuery compParams;
                compParams.addQueryItem("Id", QString::number(companyId));
                compData = makeRequest("/MetadataProxy/IGDB/Company", compParams);
            } else {
                QUrlQuery compParams;
                compParams.addQueryItem("Id", QString::number(companyId));
                compData = makeRequest("/MetadataProxy/IGDB/Company", compParams);
                companyName = compData["name"].toString();
                if (!companyName.isEmpty()) {
                    m_companyCache.insert(companyId, companyName);
                }
            }
            if (companyName.isEmpty())
                continue;
            
            // Check if this game ID appears in the company's developed/published lists
            QJsonArray developed = compData["developed"].toArray();
            QJsonArray published = compData["published"].toArray();
            
            bool isDev = false, isPub = false;
            for (const QJsonValue &gid : developed) {
                if (gid.toInt() == gameIdInt) { isDev = true; break; }
            }
            for (const QJsonValue &gid : published) {
                if (gid.toInt() == gameIdInt) { isPub = true; break; }
            }
            
            if (isDev && metadata.developer.isEmpty()) {
                metadata.developer = companyName;
                qDebug() << "Hasheous: Developer resolved:" << companyName;
            }
            if (isPub && metadata.publisher.isEmpty()) {
                metadata.publisher = companyName;
                qDebug() << "Hasheous: Publisher resolved:" << companyName;
            }
        }
    }

    // Platform → system mapping (IGDB platform slugs to Remus system IDs)
    QSet<QString> platformSlugs;
    QJsonValue platformsVal = igdbGame["platforms"];
    if (platformsVal.isObject()) {
        QJsonObject plats = platformsVal.toObject();
        for (auto it = plats.begin(); it != plats.end(); ++it) {
            QString slug = it.value().toObject()["slug"].toString().toLower();
            if (!slug.isEmpty()) platformSlugs.insert(slug);
        }
    } else if (platformsVal.isArray()) {
        QJsonArray plats = platformsVal.toArray();
        for (const QJsonValue &pv : plats) {
            QString slug = pv.toObject()["slug"].toString().toLower();
            if (!slug.isEmpty()) platformSlugs.insert(slug);
        }
    }

    if (!platformSlugs.isEmpty()) {
        for (auto it = Constants::Systems::SYSTEMS.constBegin(); it != Constants::Systems::SYSTEMS.constEnd(); ++it) {
            int systemId = it.key();
            QString igdbSlug = SystemResolver::providerName(systemId, Constants::Providers::IGDB).toLower();
            if (!igdbSlug.isEmpty() && platformSlugs.contains(igdbSlug)) {
                metadata.system = SystemResolver::internalName(systemId);
                break;
            }
        }
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
        qWarning() << "Hasheous: Invalid hash length, expected CRC32 (8), MD5 (32), or SHA1 (40), got:" << hash.length();
        emit errorOccurred("Invalid hash length for Hasheous");
        return GameMetadata();
    }

    // Delegate to multi-hash path (single hash populated)
    QString crc32 = (hashType == "crc32") ? hash : QString();
    QString md5 = (hashType == "md5") ? hash : QString();
    QString sha1 = (hashType == "sha1") ? hash : QString();
    return getByHashes(crc32, md5, sha1, system);
}

GameMetadata HasheousProvider::getByHashes(const QString &crc32,
                                           const QString &md5,
                                           const QString &sha1,
                                           const QString &system)
{
    Q_UNUSED(system);

    if (crc32.isEmpty() && md5.isEmpty() && sha1.isEmpty()) {
        qWarning() << "Hasheous: No hashes provided";
        emit errorOccurred("No hashes provided for Hasheous");
        return GameMetadata();
    }

    qInfo() << "Hasheous: Looking up hash set"
            << "crc32=" << (crc32.isEmpty() ? "-" : crc32)
            << "md5=" << (md5.isEmpty() ? "-" : md5)
            << "sha1=" << (sha1.isEmpty() ? "-" : sha1);

    QJsonObject body;
    if (!crc32.isEmpty()) body["crc"] = crc32.toLower();
    if (!md5.isEmpty()) body["mD5"] = md5.toLower();
    if (!sha1.isEmpty()) body["shA1"] = sha1.toLower();

    QUrlQuery params;
    params.addQueryItem("returnAllSources", "true");
    params.addQueryItem("returnFields", "Signatures,Metadata,Attributes");

    QJsonObject response = makePostRequest("/Lookup/ByHash", body, params);

    if (response.isEmpty()) {
        qInfo() << "Hasheous: No match found for provided hashes";
        return GameMetadata();
    }

    GameMetadata metadata = parseGameJson(response);

    if (metadata.title.isEmpty()) {
        qInfo() << "Hasheous: No match found for provided hashes";
        return GameMetadata();
    }

    qInfo() << "Hasheous: Found match:" << metadata.title;

    if (metadata.externalIds.contains("igdb")) {
        int igdbId = metadata.externalIds["igdb"].toInt();
        if (igdbId > 0) {
            GameMetadata igdbMetadata = fetchIgdbMetadata(igdbId);
            if (!igdbMetadata.title.isEmpty()) {
                QMap<QString, QString> savedIds = metadata.externalIds;
                QString savedBoxArt = metadata.boxArtUrl;
                metadata = igdbMetadata;
                for (auto it = savedIds.begin(); it != savedIds.end(); ++it) {
                    metadata.externalIds[it.key()] = it.value();
                }
                if (metadata.boxArtUrl.isEmpty() && !savedBoxArt.isEmpty()) {
                    metadata.boxArtUrl = savedBoxArt;
                }
            }
        }
    }

    metadata.providerId = Constants::Providers::HASHEOUS;
    metadata.fetchedAt = QDateTime::currentDateTime();
    emit metadataFetched(metadata);
    return metadata;
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
