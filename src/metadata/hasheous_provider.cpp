#include "hasheous_provider.h"
#include "../core/hasheous_config.h"
#include "../core/constants/providers.h"
#include "../core/constants/settings.h"
#include "../core/constants/api.h"
#include "../core/constants/errors.h"
#include "../core/system_resolver.h"
#include "../core/constants/systems.h"
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QSet>
#include <QTimeZone>
#include "../core/logging_categories.h"
#include "../core/constants/network.h"

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

    bool isMetadataProxyEndpoint(const QString &path) {
        return path.contains("/MetadataProxy/", Qt::CaseInsensitive);
    }

    QString normalizedHashValue(const QString &hash) {
        return hash.trimmed().toLower();
    }

    QJsonObject hashEntryToPayloadObject(const HasheousHashEntry &entry) {
        QJsonObject obj;
        const QString discSha1 = !entry.chdSha1.isEmpty() ? entry.chdSha1 : entry.rvzSha1;
        if (!discSha1.isEmpty()) {
            obj.insert(QStringLiteral("shA1"), normalizedHashValue(discSha1));
            return obj;
        }

        if (!entry.crc32.isEmpty())
            obj.insert(QStringLiteral("crc"), normalizedHashValue(entry.crc32));
        if (!entry.md5.isEmpty())
            obj.insert(QStringLiteral("mD5"), normalizedHashValue(entry.md5));
        if (!entry.sha1.isEmpty())
            obj.insert(QStringLiteral("shA1"), normalizedHashValue(entry.sha1));
        if (!entry.sha256.isEmpty())
            obj.insert(QStringLiteral("sha256"), normalizedHashValue(entry.sha256));
        return obj;
    }

    QJsonArray buildLookupPayload(const QList<HasheousHashEntry> &entries) {
        QJsonArray payload;
        for (const HasheousHashEntry &entry : entries) {
            const QJsonObject obj = hashEntryToPayloadObject(entry);
            if (!obj.isEmpty())
                payload.append(obj);
        }
        return payload;
    }

    QJsonObject responseObjectFromDocument(const QJsonDocument &doc) {
        if (doc.isObject())
            return doc.object();
        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            for (const QJsonValue &value : arr) {
                if (value.isObject() && !value.toObject().value(QStringLiteral("name")).toString().isEmpty())
                    return value.toObject();
            }
            if (!arr.isEmpty() && arr.first().isObject())
                return arr.first().toObject();
        }
        return { };
    }

} // namespace

HasheousProvider::HasheousProvider(QObject *parent)
    : HttpMetadataProvider(QStringLiteral("hasheous"), Constants::Network::HASHEOUS_RATE_LIMIT_MS, parent) {
    m_baseUrl = resolveHasheousBaseUrl();
    qInfo() << "Hasheous provider initialized (base URL:" << m_baseUrl << "; hash lookup enabled; MetadataProxy"
            << (metadataProxyEnabled() ? "enabled" : "disabled") << ")";
}

void HasheousProvider::setBaseUrl(const QString &url) {
    const QString resolved = resolveHasheousBaseUrl(url);
    m_baseUrl = resolved.isEmpty() ? QString::fromLatin1(Constants::API::HASHEOUS_BASE_URL) : resolved;
}

QString HasheousProvider::detectHashType(const QString &hash) const {
    return Constants::HashAlgorithms::detectFromLength(hash.trimmed().length());
}

QJsonObject HasheousProvider::makeRequest(const QString &endpoint, const QUrlQuery &params) {
    m_rateLimiter->waitIfNeeded();

    if (isMetadataProxyEndpoint(endpoint) && !metadataProxyEnabled()) {
        qDebug() << "Hasheous: Skipping MetadataProxy request without configured authorization:" << endpoint;
        return QJsonObject();
    }

    QUrl url(m_baseUrl + endpoint);
    url.setQuery(params);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);
    if (isMetadataProxyEndpoint(endpoint) && !m_clientApiKey.isEmpty()) {
        request.setRawHeader("X-Client-API-Key", m_clientApiKey.toUtf8());
    }

    QNetworkReply *reply = m_networkManager->get(request);
    ApiResponse apiResp = waitForReply(reply, Constants::Network::HASHEOUS_TIMEOUT_MS);

    if (!apiResp.success) {
        if (apiResp.error == Constants::Errors::MetadataProvider::REQUEST_TIMEOUT) {
            qWarning() << "Hasheous GET timeout:" << url.toString();
            emit errorOccurred("Hasheous request timeout");
        } else {
            const bool metadataProxyRequest = isMetadataProxyEndpoint(url.path());
            if (apiResp.httpStatusCode == Constants::Network::HTTP_NOT_FOUND) {
                qDebug() << "Hasheous API 404 (expected miss):" << url.toString();
            } else if (metadataProxyRequest
                && (apiResp.httpStatusCode == Constants::Network::HTTP_UNAUTHORIZED
                    || apiResp.httpStatusCode == Constants::Network::HTTP_FORBIDDEN)) {
                if (!m_metadataProxyDisabled) {
                    qInfo() << "Hasheous: MetadataProxy disabled after authorization failure"
                            << "Status:" << apiResp.httpStatusCode;
                }
                m_metadataProxyDisabled = true;
            } else {
                qWarning() << "Hasheous API error:" << apiResp.error << "Status:" << apiResp.httpStatusCode
                           << "URL:" << url.toString();
                emit errorOccurred("Hasheous API error: " + apiResp.error);
            }
        }
        return QJsonObject();
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(apiResp.data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Hasheous JSON parse error:" << parseError.errorString();
        emit errorOccurred("Hasheous JSON parse error: " + parseError.errorString());
        return QJsonObject();
    }
    return doc.object();
}

bool HasheousProvider::metadataProxyEnabled() const {
    return !m_clientApiKey.isEmpty() && !m_metadataProxyDisabled;
}

QJsonObject HasheousProvider::makePostRequest(
    const QString &endpoint, const QJsonDocument &bodyDoc, const QUrlQuery &params) {
    m_rateLimiter->waitIfNeeded();

    QUrl url(m_baseUrl + endpoint);
    if (!params.isEmpty()) {
        url.setQuery(params);
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    const QByteArray postData = bodyDoc.toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_networkManager->post(request, postData);
    ApiResponse apiResp = waitForReply(reply, Constants::Network::HASHEOUS_TIMEOUT_MS);

    if (!apiResp.success) {
        if (apiResp.error == Constants::Errors::MetadataProvider::REQUEST_TIMEOUT) {
            qWarning() << "Hasheous POST timeout:" << url.toString();
            emit errorOccurred("Hasheous request timeout");
        } else if (apiResp.httpStatusCode == Constants::Network::HTTP_NOT_FOUND) {
            qDebug() << "Hasheous POST 404 (expected miss):" << url.toString();
        } else {
            qWarning() << "Hasheous POST error:" << apiResp.error << "Status:" << apiResp.httpStatusCode
                       << "URL:" << url.toString();
            emit errorOccurred("Hasheous API error: " + apiResp.error);
        }
        return QJsonObject();
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(apiResp.data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Hasheous JSON parse error (POST):" << parseError.errorString();
        emit errorOccurred("Hasheous JSON parse error: " + parseError.errorString());
        return QJsonObject();
    }

    return responseObjectFromDocument(doc);
}

QList<SearchResult> HasheousProvider::searchByName(const QString &title, const QString &system, const QString &region) {
    Q_UNUSED(title);
    Q_UNUSED(system);
    Q_UNUSED(region);

    qWarning() << "Hasheous does not support name-based search, use hash matching instead";
    emit errorOccurred("Hasheous only supports hash-based matching");

    return QList<SearchResult>();
}

GameMetadata HasheousProvider::getByHash(const QString &hash, const QString &system) {
    Q_UNUSED(system);

    QString hashType = detectHashType(hash);
    if (hashType.isEmpty()) {
        qWarning() << "Hasheous: Invalid hash length, expected CRC32 (8), MD5 (32), SHA1 (40), or SHA256 (64), got:"
                   << hash.length();
        emit errorOccurred("Invalid hash length for Hasheous");
        return GameMetadata();
    }

    QString crc32 = (hashType == QLatin1String("crc32")) ? hash : QString();
    QString md5 = (hashType == QLatin1String("md5")) ? hash : QString();
    QString sha1 = (hashType == QLatin1String("sha1")) ? hash : QString();
    QString sha256 = (hashType == QLatin1String("sha256")) ? hash : QString();
    return getByHashes(crc32, md5, sha1, system, sha256);
}

GameMetadata HasheousProvider::getByHashes(
    const QString &crc32, const QString &md5, const QString &sha1, const QString &system, const QString &sha256) {
    HasheousHashEntry entry;
    entry.crc32 = crc32;
    entry.md5 = md5;
    entry.sha1 = sha1;
    entry.sha256 = sha256;
    return getByHashEntries({ entry }, system);
}

GameMetadata HasheousProvider::getByHashEntries(const QList<HasheousHashEntry> &entries, const QString &system) {
    Q_UNUSED(system);

    const QJsonArray payload = buildLookupPayload(entries);
    if (payload.isEmpty()) {
        qWarning() << "Hasheous: No hashes provided";
        emit errorOccurred("No hashes provided for Hasheous");
        return GameMetadata();
    }

    qInfo() << "Hasheous: Looking up" << payload.size() << "hash set(s) via array payload";

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("returnAllSources"), QStringLiteral("true"));
    params.addQueryItem(QStringLiteral("returnFields"), QStringLiteral("Signatures,Metadata,Attributes"));

    const QJsonObject response
        = makePostRequest(Constants::API::HASHEOUS_LOOKUP_ENDPOINT, QJsonDocument(payload), params);

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

    if (metadata.externalIds.contains("igdb") && metadataProxyEnabled()) {
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
    } else if (metadata.externalIds.contains("igdb") && !metadataProxyEnabled()) {
        qInfo() << "Hasheous: IGDB enrichment available (ID:" << metadata.externalIds["igdb"]
                << ") but MetadataProxy is disabled."
                << "Set hasheous_client_api_key in settings for richer metadata.";
        m_igdbSkippedCount++;
    }

    metadata.providerId = Constants::Providers::HASHEOUS;
    metadata.fetchedAt = QDateTime::currentDateTime();
    emit metadataFetched(metadata);
    return metadata;
}

GameMetadata HasheousProvider::getById(const QString &id) {
    Q_UNUSED(id);

    qWarning() << "Hasheous does not support ID-based lookup";
    emit errorOccurred("Hasheous only supports hash-based matching");

    return GameMetadata();
}

ArtworkUrls HasheousProvider::getArtwork(const QString &id) {
    Q_UNUSED(id);
    return ArtworkUrls();
}

} // namespace Remus
