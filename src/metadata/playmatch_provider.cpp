#include "playmatch_provider.h"

#include "../core/constants/errors.h"
#include "../core/constants/hash_algorithms.h"
#include "../core/constants/match_methods.h"
#include "../core/constants/network.h"
#include "../core/logging_categories.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QUrl>

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

    QString normalizedHexHash(const QString &hash) {
        return hash.trimmed().toLower();
    }

    bool isHexHash(const QString &hash, int expectedLength) {
        const QString trimmed = hash.trimmed();
        if (trimmed.size() != expectedLength)
            return false;
        for (const QChar ch : trimmed) {
            if (!ch.isDigit() && (ch.toLower() < QChar('a') || ch.toLower() > QChar('f')))
                return false;
        }
        return true;
    }

} // namespace

PlayMatchProvider::PlayMatchProvider(QObject *parent)
    : HttpMetadataProvider(QStringLiteral("playmatch"), Constants::Network::PLAYMATCH_RATE_LIMIT_MS, parent) {
    qInfo() << "PlayMatch provider initialized";
}

QJsonObject PlayMatchProvider::makeRequest(const QString &endpoint, const QUrlQuery &params) {
    m_rateLimiter->waitIfNeeded();

    QUrl url(QString(Constants::API::PLAYMATCH_BASE_URL) + endpoint);
    url.setQuery(params);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);

    QNetworkReply *reply = m_networkManager->get(request);
    ApiResponse apiResp = waitForReply(reply, Constants::Network::PLAYMATCH_TIMEOUT_MS);

    if (!apiResp.success) {
        if (apiResp.error == Constants::Errors::MetadataProvider::REQUEST_TIMEOUT) {
            qWarning() << "PlayMatch GET timeout:" << url.toString();
            emit errorOccurred(QStringLiteral("PlayMatch request timeout"));
        } else if (apiResp.httpStatusCode == Constants::Network::HTTP_NOT_FOUND) {
            qDebug() << "PlayMatch API 404 (expected miss):" << url.toString();
        } else {
            qWarning() << "PlayMatch API error:" << apiResp.error << "Status:" << apiResp.httpStatusCode
                       << "URL:" << url.toString();
            emit errorOccurred(QStringLiteral("PlayMatch API error: ") + apiResp.error);
        }
        return QJsonObject();
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(apiResp.data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "PlayMatch JSON parse error:" << parseError.errorString();
        emit errorOccurred(QStringLiteral("PlayMatch JSON parse error: ") + parseError.errorString());
        return QJsonObject();
    }

    if (doc.isObject())
        return doc.object();
    return QJsonObject();
}

float PlayMatchProvider::matchScoreForType(const QString &gameMatchType) {
    const QString type = gameMatchType.trimmed();
    if (type.compare(QStringLiteral("MD5"), Qt::CaseInsensitive) == 0
        || type.compare(QStringLiteral("SHA1"), Qt::CaseInsensitive) == 0
        || type.compare(QStringLiteral("SHA256"), Qt::CaseInsensitive) == 0
        || type.compare(QStringLiteral("CRC"), Qt::CaseInsensitive) == 0
        || type.compare(QStringLiteral("CRC32"), Qt::CaseInsensitive) == 0) {
        return 1.0f;
    }
    if (type.compare(QStringLiteral("FileNameAndSize"), Qt::CaseInsensitive) == 0)
        return 0.4f;
    return 0.8f;
}

QString PlayMatchProvider::matchMethodForType(const QString &gameMatchType) {
    const float score = matchScoreForType(gameMatchType);
    if (score >= 1.0f)
        return QString::fromLatin1(Constants::MatchMethods::HASH);
    if (gameMatchType.compare(QStringLiteral("FileNameAndSize"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("filename_size");
    return QString::fromLatin1(Constants::MatchMethods::NAME);
}

int PlayMatchProvider::extractIgdbId(const QJsonObject &identifyJson) const {
    const QJsonArray external = identifyJson.value(QStringLiteral("externalMetadata")).toArray();
    for (const QJsonValue &entry : external) {
        const QJsonObject obj = entry.toObject();
        if (obj.value(QStringLiteral("providerName")).toString().compare(QStringLiteral("IGDB"), Qt::CaseInsensitive)
            != 0)
            continue;
        if (obj.value(QStringLiteral("matchType")).toString().compare(QStringLiteral("Failed"), Qt::CaseInsensitive)
            == 0)
            continue;
        bool ok = false;
        const int igdbId = obj.value(QStringLiteral("providerId")).toString().toInt(&ok);
        if (ok && igdbId > 0)
            return igdbId;
    }
    return 0;
}

GameMetadata PlayMatchProvider::parseIdentifyResponse(const QJsonObject &json) const {
    GameMetadata metadata;
    if (json.isEmpty())
        return metadata;

    const QString matchType = json.value(QStringLiteral("gameMatchType")).toString();
    if (matchType.compare(QStringLiteral("NoMatch"), Qt::CaseInsensitive) == 0)
        return metadata;

    metadata.matchScore = matchScoreForType(matchType);
    metadata.matchMethod = matchMethodForType(matchType);
    metadata.id = json.value(QStringLiteral("id")).toString();

    const int igdbId = extractIgdbId(json);
    if (igdbId > 0)
        metadata.externalIds[Constants::Providers::ExternalId::IGDB] = QString::number(igdbId);

    return metadata;
}

GameMetadata PlayMatchProvider::parseIgdbGameJson(const QJsonObject &json, int igdbId) const {
    GameMetadata metadata;
    if (json.isEmpty() || igdbId <= 0)
        return metadata;

    metadata.title = json.value(QStringLiteral("name")).toString();
    metadata.description = json.value(QStringLiteral("summary")).toString();
    if (metadata.description.isEmpty())
        metadata.description = json.value(QStringLiteral("storyline")).toString();

    metadata.externalIds[Constants::Providers::ExternalId::IGDB] = QString::number(igdbId);
    metadata.id = QString::number(igdbId);

    if (json.contains(QStringLiteral("first_release_date"))) {
        const QJsonValue dateVal = json.value(QStringLiteral("first_release_date"));
        if (dateVal.isDouble()) {
            const QDateTime dt = QDateTime::fromSecsSinceEpoch(dateVal.toInteger(), Qt::UTC);
            if (dt.isValid())
                metadata.releaseDate = dt.toUTC().date().toString(QStringLiteral("yyyy-MM-dd"));
        } else if (dateVal.isString()) {
            const QDateTime dt = QDateTime::fromString(dateVal.toString(), Qt::ISODate);
            if (dt.isValid())
                metadata.releaseDate = dt.toUTC().date().toString(QStringLiteral("yyyy-MM-dd"));
        }
    }

    if (json.contains(QStringLiteral("rating"))) {
        const double rating = json.value(QStringLiteral("rating")).toDouble(0.0);
        if (rating > 0.0)
            metadata.rating = static_cast<float>(rating / Constants::API::IGDB_RATING_SCALE);
    }

    return metadata;
}

GameMetadata PlayMatchProvider::fetchIgdbMetadata(int igdbId) {
    if (igdbId <= 0)
        return GameMetadata();

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("id"), QString::number(igdbId));
    const QJsonObject igdbGame = makeRequest(Constants::API::PLAYMATCH_IGDB_GAME_ENDPOINT, params);
    if (igdbGame.isEmpty()) {
        qDebug() << "PlayMatch: IGDB proxy returned empty for ID:" << igdbId;
        return GameMetadata();
    }

    return parseIgdbGameJson(igdbGame, igdbId);
}

GameMetadata PlayMatchProvider::identifyBySignals(const QString &fileName, qint64 fileSize, const QString &crc32,
    const QString &md5, const QString &sha1, const QString &sha256, const QString &system) {
    Q_UNUSED(system);

    const QString trimmedName = fileName.trimmed();
    if (trimmedName.isEmpty() || fileSize <= 0) {
        qDebug() << "PlayMatch: fileName and fileSize are required for identify";
        return GameMetadata();
    }

    const QString crc = normalizedHexHash(crc32);
    const QString md5Hash = normalizedHexHash(md5);
    const QString sha1Hash = normalizedHexHash(sha1);
    const QString sha256Hash = normalizedHexHash(sha256);

    if (crc.isEmpty() && md5Hash.isEmpty() && sha1Hash.isEmpty() && sha256Hash.isEmpty()) {
        qDebug() << "PlayMatch: at least one hash is required for identify";
        return GameMetadata();
    }

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("fileName"), trimmedName);
    params.addQueryItem(QStringLiteral("fileSize"), QString::number(fileSize));
    if (!crc.isEmpty() && isHexHash(crc, 8))
        params.addQueryItem(QStringLiteral("crc"), crc);
    if (!md5Hash.isEmpty() && isHexHash(md5Hash, 32))
        params.addQueryItem(QStringLiteral("md5"), md5Hash);
    if (!sha1Hash.isEmpty() && isHexHash(sha1Hash, 40))
        params.addQueryItem(QStringLiteral("sha1"), sha1Hash);
    if (!sha256Hash.isEmpty() && isHexHash(sha256Hash, 64))
        params.addQueryItem(QStringLiteral("sha256"), sha256Hash);

    qInfo() << "PlayMatch: identifying" << trimmedName << "size=" << fileSize;

    const QJsonObject identifyJson = makeRequest(Constants::API::PLAYMATCH_IDENTIFY_IDS_ENDPOINT, params);
    GameMetadata metadata = parseIdentifyResponse(identifyJson);
    if (metadata.title.isEmpty() && metadata.externalIds.isEmpty())
        return GameMetadata();

    const int igdbId = extractIgdbId(identifyJson);
    if (igdbId > 0) {
        const GameMetadata igdbMetadata = fetchIgdbMetadata(igdbId);
        if (!igdbMetadata.title.isEmpty()) {
            const float savedScore = metadata.matchScore;
            const QString savedMethod = metadata.matchMethod;
            const QString savedId = metadata.id;
            metadata = igdbMetadata;
            metadata.matchScore = savedScore;
            metadata.matchMethod = savedMethod;
            if (!savedId.isEmpty())
                metadata.id = savedId;
        }
    }

    if (metadata.title.isEmpty())
        return GameMetadata();

    metadata.providerId = Constants::Providers::PLAYMATCH;
    metadata.fetchedAt = QDateTime::currentDateTime();
    emit metadataFetched(metadata);
    return metadata;
}

QList<SearchResult> PlayMatchProvider::searchByName(
    const QString &title, const QString &system, const QString &region) {
    Q_UNUSED(title);
    Q_UNUSED(system);
    Q_UNUSED(region);
    qWarning() << "PlayMatch does not support name-based search, use hash/filename matching instead";
    emit errorOccurred(QStringLiteral("PlayMatch only supports hash/filename matching"));
    return QList<SearchResult>();
}

GameMetadata PlayMatchProvider::getByHash(const QString &hash, const QString &system) {
    Q_UNUSED(hash);
    Q_UNUSED(system);
    qDebug() << "PlayMatch: getByHash requires fileName and fileSize — use identifyBySignals instead";
    return GameMetadata();
}

GameMetadata PlayMatchProvider::getById(const QString &id) {
    Q_UNUSED(id);
    qWarning() << "PlayMatch does not support ID-based lookup";
    emit errorOccurred(QStringLiteral("PlayMatch only supports hash/filename matching"));
    return GameMetadata();
}

ArtworkUrls PlayMatchProvider::getArtwork(const QString &id) {
    Q_UNUSED(id);
    return ArtworkUrls();
}

} // namespace Remus
