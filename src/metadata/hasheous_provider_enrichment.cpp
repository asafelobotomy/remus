#include "hasheous_provider.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QTimeZone>

#include "../core/constants/providers.h"
#include "../core/constants/systems.h"
#include "../core/system_resolver.h"

namespace Remus {

GameMetadata HasheousProvider::parseGameJson(const QJsonObject &json) const
{
    GameMetadata metadata;

    if (json.isEmpty()) {
        return metadata;
    }

    metadata.title = json["name"].toString();
    metadata.id = QString::number(json["id"].toInt());

    const QJsonArray metadataArray = json["metadata"].toArray();
    for (const QJsonValue &meta : metadataArray) {
        const QJsonObject metaObj = meta.toObject();
        const QString source = metaObj["source"].toString();
        const QString immutableId = metaObj["immutableId"].toString();

        if (source == "IGDB" && !immutableId.isEmpty()) {
            metadata.externalIds[Constants::Providers::ExternalId::IGDB] = immutableId;
        } else if (source == "TheGamesDB" && !immutableId.isEmpty()) {
            metadata.externalIds[Constants::Providers::ExternalId::THEGAMESDB] = immutableId;
        } else if (source == "RetroAchievements" && !immutableId.isEmpty()) {
            metadata.externalIds[Constants::Providers::ExternalId::RETROACHIEVEMENTS] = immutableId;
        }
    }

    QStringList datSources;
    const QJsonArray signatures = json["signatures"].toArray();
    for (const QJsonValue &sig : signatures) {
        datSources.append(sig.toString());
    }
    if (!datSources.isEmpty()) {
        metadata.externalIds[Constants::Providers::ExternalId::DAT_SOURCES] = datSources.join(',');
    }

    const QJsonArray attributes = json["attributes"].toArray();
    for (const QJsonValue &attr : attributes) {
        const QJsonObject attrObj = attr.toObject();
        if (attrObj["attributeName"].toString() == "Logo") {
            const QString link = attrObj["link"].toString();
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

    if (!metadataProxyEnabled()) {
        qDebug() << "Hasheous: MetadataProxy enrichment skipped for IGDB ID:" << igdbId;
        return GameMetadata();
    }

    qInfo() << "Hasheous: Fetching IGDB metadata via MetadataProxy for ID:" << igdbId;

    QUrlQuery params;
    params.addQueryItem("Id", QString::number(igdbId));
    params.addQueryItem("expandColumns", "age_ratings,alternative_names,collections,cover,dlcs,expanded_games,franchise,franchises,game_modes,genres,involved_companies,platforms,ports,remakes,screenshots,similar_games,videos");

    const QJsonObject igdbGame = makeRequest(Constants::API::HASHEOUS_PROXY_IGDB_GAME, params);
    if (igdbGame.isEmpty()) {
        qWarning() << "Hasheous: MetadataProxy returned empty for IGDB ID:" << igdbId;
        return GameMetadata();
    }

    GameMetadata metadata;
    metadata.title = igdbGame["name"].toString();
    metadata.description = igdbGame["summary"].toString();
    metadata.externalIds[Constants::Providers::ExternalId::IGDB] = QString::number(igdbId);

    if (igdbGame.contains("first_release_date")) {
        const QJsonValue dateVal = igdbGame["first_release_date"];
        if (dateVal.isString()) {
            const QDateTime dt = QDateTime::fromString(dateVal.toString(), Qt::ISODate);
            if (dt.isValid()) {
                metadata.releaseDate = dt.toUTC().date().toString("yyyy-MM-dd");
            }
        } else if (dateVal.isDouble()) {
            const QDateTime dt = QDateTime::fromSecsSinceEpoch(dateVal.toInteger(), QTimeZone::UTC);
            metadata.releaseDate = dt.toUTC().date().toString("yyyy-MM-dd");
        }
    }

    const QJsonValue genresVal = igdbGame["genres"];
    if (genresVal.isObject()) {
        const QJsonObject genresObj = genresVal.toObject();
        for (auto it = genresObj.begin(); it != genresObj.end(); ++it) {
            const QString name = it.value().toObject()["name"].toString();
            if (!name.isEmpty()) {
                metadata.genres.append(name);
            }
        }
    } else if (genresVal.isArray()) {
        const QJsonArray genresArr = genresVal.toArray();
        for (const QJsonValue &genre : genresArr) {
            const QString name = genre.toObject()["name"].toString();
            if (!name.isEmpty()) {
                metadata.genres.append(name);
            }
        }
    }

    if (igdbGame.contains("aggregated_rating")) {
        metadata.rating = igdbGame["aggregated_rating"].toDouble() / Constants::API::IGDB_RATING_SCALE;
    }

    if (igdbGame.contains("cover")) {
        const QJsonObject cover = igdbGame["cover"].toObject();
        QString coverUrl = cover["url"].toString();
        if (!coverUrl.isEmpty()) {
            coverUrl.replace(Constants::API::IGDB_IMG_THUMB, Constants::API::IGDB_IMG_1080P);
            if (coverUrl.startsWith("//")) {
                coverUrl = "https:" + coverUrl;
            }
            metadata.boxArtUrl = coverUrl;
        }
    }

    auto normalizeShot = [](const QString &url) {
        QString normalized = url;
        if (normalized.isEmpty()) {
            return normalized;
        }
        normalized.replace(Constants::API::IGDB_IMG_THUMB, Constants::API::IGDB_IMG_1080P);
        if (normalized.startsWith("//")) {
            normalized = "https:" + normalized;
        }
        return normalized;
    };

    const QJsonValue screenshotsVal = igdbGame["screenshots"];
    if (screenshotsVal.isObject()) {
        const QJsonObject shotsObj = screenshotsVal.toObject();
        for (auto it = shotsObj.begin(); it != shotsObj.end(); ++it) {
            const QString shotUrl = normalizeShot(it.value().toObject()["url"].toString());
            if (!shotUrl.isEmpty()) {
                metadata.screenshotUrls.append(shotUrl);
            }
        }
    } else if (screenshotsVal.isArray()) {
        const QJsonArray shotsArr = screenshotsVal.toArray();
        for (const QJsonValue &shotVal : shotsArr) {
            const QString shotUrl = normalizeShot(shotVal.toObject()["url"].toString());
            if (!shotUrl.isEmpty()) {
                metadata.screenshotUrls.append(shotUrl);
            }
        }
    }

    const QJsonValue companiesVal = igdbGame["involved_companies"];
    if (companiesVal.isObject() || companiesVal.isArray()) {
        QList<int> companyIds;
        if (companiesVal.isObject()) {
            const QJsonObject companiesObj = companiesVal.toObject();
            for (auto it = companiesObj.begin(); it != companiesObj.end(); ++it) {
                const QJsonObject comp = it.value().toObject();
                const bool isDev = comp["developer"].toBool();
                const bool isPub = comp["publisher"].toBool();
                const QJsonValue companyVal = comp["company"];

                if (isDev || isPub) {
                    QString companyName;
                    if (companyVal.isObject()) {
                        companyName = companyVal.toObject()["name"].toString();
                    } else if (companyVal.isDouble()) {
                        const int companyId = companyVal.toInt();
                        if (m_companyCache.contains(companyId)) {
                            companyName = m_companyCache.value(companyId);
                        } else {
                            QUrlQuery compParams;
                            compParams.addQueryItem("Id", QString::number(companyId));
                            const QJsonObject compData = makeRequest(Constants::API::HASHEOUS_PROXY_IGDB_COMPANY, compParams);
                            companyName = compData["name"].toString();
                            if (!companyName.isEmpty()) {
                                m_companyCache.insert(companyId, companyName);
                            }
                        }
                    }
                    if (!companyName.isEmpty()) {
                        if (isDev && metadata.developer.isEmpty()) {
                            metadata.developer = companyName;
                        }
                        if (isPub && metadata.publisher.isEmpty()) {
                            metadata.publisher = companyName;
                        }
                    }
                } else if (companyVal.isDouble()) {
                    companyIds.append(companyVal.toInt());
                }
            }
        } else {
            const QJsonArray companiesArr = companiesVal.toArray();
            for (const QJsonValue &compVal : companiesArr) {
                const QJsonObject comp = compVal.toObject();
                const QJsonObject company = comp["company"].toObject();
                const QString companyName = company["name"].toString();
                if (!companyName.isEmpty()) {
                    if (comp["developer"].toBool() && metadata.developer.isEmpty()) {
                        metadata.developer = companyName;
                    }
                    if (comp["publisher"].toBool() && metadata.publisher.isEmpty()) {
                        metadata.publisher = companyName;
                    }
                }
            }
        }

        for (const int companyId : companyIds) {
            if (!metadata.developer.isEmpty() && !metadata.publisher.isEmpty()) {
                break;
            }

            QString companyName;
            QJsonObject compData;
            QUrlQuery compParams;
            compParams.addQueryItem("Id", QString::number(companyId));
            compData = makeRequest(Constants::API::HASHEOUS_PROXY_IGDB_COMPANY, compParams);

            if (m_companyCache.contains(companyId)) {
                companyName = m_companyCache.value(companyId);
            } else {
                companyName = compData["name"].toString();
                if (!companyName.isEmpty()) {
                    m_companyCache.insert(companyId, companyName);
                }
            }
            if (companyName.isEmpty()) {
                continue;
            }

            const QJsonArray developed = compData["developed"].toArray();
            const QJsonArray published = compData["published"].toArray();

            bool isDev = false;
            bool isPub = false;
            for (const QJsonValue &gid : developed) {
                if (gid.toInt() == igdbId) {
                    isDev = true;
                    break;
                }
            }
            for (const QJsonValue &gid : published) {
                if (gid.toInt() == igdbId) {
                    isPub = true;
                    break;
                }
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

    QSet<QString> platformSlugs;
    const QJsonValue platformsVal = igdbGame["platforms"];
    if (platformsVal.isObject()) {
        const QJsonObject plats = platformsVal.toObject();
        for (auto it = plats.begin(); it != plats.end(); ++it) {
            const QString slug = it.value().toObject()["slug"].toString().toLower();
            if (!slug.isEmpty()) {
                platformSlugs.insert(slug);
            }
        }
    } else if (platformsVal.isArray()) {
        const QJsonArray plats = platformsVal.toArray();
        for (const QJsonValue &pv : plats) {
            const QString slug = pv.toObject()["slug"].toString().toLower();
            if (!slug.isEmpty()) {
                platformSlugs.insert(slug);
            }
        }
    }

    if (!platformSlugs.isEmpty()) {
        for (auto it = Constants::Systems::SYSTEMS.constBegin(); it != Constants::Systems::SYSTEMS.constEnd(); ++it) {
            const int systemId = it.key();
            const QString igdbSlug = SystemResolver::providerName(systemId, Constants::Providers::IGDB).toLower();
            if (!igdbSlug.isEmpty() && platformSlugs.contains(igdbSlug)) {
                metadata.system = SystemResolver::internalName(systemId);
                break;
            }
        }
    }

    return metadata;
}

} // namespace Remus