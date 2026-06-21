#include "steamgriddb_provider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>

#include "../core/constants/constants.h"

namespace Remus {

namespace {

    QString pickBestAssetUrl(const QJsonArray &items) {
        QString bestUrl;
        int bestScore = -1;

        for (const QJsonValue &itemVal : items) {
            const QJsonObject item = itemVal.toObject();
            const QJsonArray tags = item.value(QStringLiteral("tags")).toArray();
            bool skip = false;
            for (const QJsonValue &tagVal : tags) {
                const QString tag = tagVal.toString().toLower();
                if (tag == QStringLiteral("nsfw") || tag == QStringLiteral("humor")
                    || tag == QStringLiteral("epilepsy")) {
                    skip = true;
                    break;
                }
            }
            if (skip)
                continue;

            const QString url = item.value(QStringLiteral("url")).toString();
            if (url.isEmpty())
                continue;

            const int score = item.value(QStringLiteral("score")).toInt(0);
            if (score > bestScore) {
                bestScore = score;
                bestUrl = url;
            }
        }

        if (!bestUrl.isEmpty())
            return bestUrl;

        // Fall back to the first item if every candidate was tagged.
        if (!items.isEmpty()) {
            return items.first().toObject().value(QStringLiteral("url")).toString();
        }

        return { };
    }

    bool isDigitsOnly(const QString &value) {
        if (value.isEmpty())
            return false;
        for (const QChar ch : value) {
            if (!ch.isDigit())
                return false;
        }
        return true;
    }

} // namespace

SteamGridDBProvider::SteamGridDBProvider(QObject *parent)
    : HttpMetadataProvider(QStringLiteral("steamgriddb"), Constants::Network::STEAMGRIDDB_RATE_LIMIT_MS, parent) { }

void SteamGridDBProvider::setApiKey(const QString &apiKey) {
    m_apiKey = apiKey.trimmed();
}

QString SteamGridDBProvider::resolveArtworkLookupId(
    const QString &id, const QMap<QString, QString> &externalIds) {
    const QString steamId = externalIds.value(Constants::Providers::ExternalId::STEAM).trimmed();
    if (!steamId.isEmpty())
        return QStringLiteral("steam:") + steamId;

    const QString trimmed = id.trimmed();
    if (trimmed.startsWith(QStringLiteral("steam:"), Qt::CaseInsensitive)
        || trimmed.startsWith(QStringLiteral("sgdb:"), Qt::CaseInsensitive)) {
        return trimmed;
    }

    return trimmed;
}

SteamGridDBProvider::LookupTarget SteamGridDBProvider::parseLookupId(const QString &id) const {
    LookupTarget target;
    const QString trimmed = id.trimmed();
    if (trimmed.isEmpty())
        return target;

    static const QRegularExpression steamPrefix(QStringLiteral("^steam:(\\d+)$"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression sgdbPrefix(QStringLiteral("^sgdb:(\\d+)$"), QRegularExpression::CaseInsensitiveOption);

    if (const QRegularExpressionMatch steamMatch = steamPrefix.match(trimmed); steamMatch.hasMatch()) {
        target.kind = LookupTarget::Kind::SteamAppId;
        target.value = steamMatch.captured(1);
        return target;
    }

    if (const QRegularExpressionMatch sgdbMatch = sgdbPrefix.match(trimmed); sgdbMatch.hasMatch()) {
        target.kind = LookupTarget::Kind::GameId;
        target.value = sgdbMatch.captured(1);
        return target;
    }

    if (isDigitsOnly(trimmed)) {
        target.kind = LookupTarget::Kind::GameId;
        target.value = trimmed;
    }

    return target;
}

QList<SearchResult> SteamGridDBProvider::searchByName(
    const QString &title, const QString &system, const QString &region) {
    Q_UNUSED(title);
    Q_UNUSED(system);
    Q_UNUSED(region);
    return { };
}

GameMetadata SteamGridDBProvider::getByHash(const QString &hash, const QString &system) {
    Q_UNUSED(hash);
    Q_UNUSED(system);
    return { };
}

GameMetadata SteamGridDBProvider::getById(const QString &id) {
    Q_UNUSED(id);
    return { };
}

ArtworkUrls SteamGridDBProvider::getArtwork(const QString &id) {
    ArtworkUrls artwork;

    if (m_apiKey.isEmpty()) {
        emit errorOccurred(QStringLiteral("SteamGridDB API key not configured"));
        return artwork;
    }

    const LookupTarget target = parseLookupId(id);
    if (target.kind == LookupTarget::Kind::None)
        return artwork;

    QUrlQuery gridQuery;
    gridQuery.addQueryItem(QStringLiteral("dimensions"), QStringLiteral("600x900,660x930"));
    gridQuery.addQueryItem(QStringLiteral("types"), QStringLiteral("static"));
    gridQuery.addQueryItem(QStringLiteral("nsfw"), QStringLiteral("false"));
    gridQuery.addQueryItem(QStringLiteral("humor"), QStringLiteral("false"));
    gridQuery.addQueryItem(QStringLiteral("limit"), QStringLiteral("50"));

    QUrlQuery assetQuery;
    assetQuery.addQueryItem(QStringLiteral("types"), QStringLiteral("static"));
    assetQuery.addQueryItem(QStringLiteral("nsfw"), QStringLiteral("false"));
    assetQuery.addQueryItem(QStringLiteral("humor"), QStringLiteral("false"));
    assetQuery.addQueryItem(QStringLiteral("limit"), QStringLiteral("20"));

    const QString resourcePrefix = target.kind == LookupTarget::Kind::SteamAppId
        ? QStringLiteral("/grids/steam/") + target.value
        : QStringLiteral("/grids/game/") + target.value;

    const QUrl gridUrl = bestAssetUrl(resourcePrefix, gridQuery);
    if (!gridUrl.isEmpty())
        artwork.boxFront = gridUrl;

    const QString heroPrefix = target.kind == LookupTarget::Kind::SteamAppId
        ? QStringLiteral("/heroes/steam/") + target.value
        : QStringLiteral("/heroes/game/") + target.value;
    const QUrl heroUrl = bestAssetUrl(heroPrefix, assetQuery);
    if (!heroUrl.isEmpty())
        artwork.banner = heroUrl;

    const QString logoPrefix = target.kind == LookupTarget::Kind::SteamAppId
        ? QStringLiteral("/logos/steam/") + target.value
        : QStringLiteral("/logos/game/") + target.value;
    const QUrl logoUrl = bestAssetUrl(logoPrefix, assetQuery);
    if (!logoUrl.isEmpty()) {
        artwork.logo = logoUrl;
        artwork.clearLogo = logoUrl;
    }

    // If a plain numeric id failed as a SGDB game id, retry as a Steam app id.
    if (artwork.boxFront.isEmpty() && target.kind == LookupTarget::Kind::GameId && isDigitsOnly(target.value)) {
        return getArtwork(QStringLiteral("steam:") + target.value);
    }

    return artwork;
}

QUrl SteamGridDBProvider::bestAssetUrl(const QString &resourcePath, const QUrlQuery &query) {
    QUrl url(QString(Constants::API::STEAMGRIDDB_BASE_URL) + resourcePath);
    url.setQuery(query);

    const ApiResponse response = makeRequest(url);
    if (!response.success)
        return { };

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(response.data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "SteamGridDB JSON parse error:" << parseError.errorString();
        return { };
    }

    const QJsonObject root = doc.object();
    if (!root.value(QStringLiteral("success")).toBool(false))
        return { };

    const QString best = pickBestAssetUrl(root.value(QStringLiteral("data")).toArray());
    return best.isEmpty() ? QUrl() : QUrl(best);
}

SteamGridDBProvider::ApiResponse SteamGridDBProvider::makeRequest(const QUrl &url) {
    m_rateLimiter->waitIfNeeded();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);
    request.setRawHeader("Authorization", QByteArray("Bearer ") + m_apiKey.toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    return waitForReply(reply, Constants::Network::STEAMGRIDDB_TIMEOUT_MS);
}

bool SteamGridDBProvider::isAvailable() {
    return !m_apiKey.isEmpty();
}

} // namespace Remus
