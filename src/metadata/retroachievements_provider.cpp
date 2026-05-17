#include "retroachievements_provider.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace Remus {

RetroAchievementsProvider::RetroAchievementsProvider(QObject *parent)
    : HttpMetadataProvider(500, parent)  // 2 req/sec max — be polite
{
}

void RetroAchievementsProvider::setCredentials(const QString &username,
                                                const QString &apiKey)
{
    m_username = username;
    m_apiKey = apiKey;
    m_authenticated = !username.isEmpty() && !apiKey.isEmpty();
}

QList<SearchResult> RetroAchievementsProvider::searchByName(const QString &title,
                                                             const QString &system,
                                                             const QString &region)
{
    Q_UNUSED(title);
    Q_UNUSED(system);
    Q_UNUSED(region);
    // RA API doesn't have a name search endpoint — hash-only provider
    return {};
}

GameMetadata RetroAchievementsProvider::getByHash(const QString &hash,
                                                    const QString &system)
{
    Q_UNUSED(system);

    if (!m_authenticated || hash.isEmpty())
        return {};

    // RA hash lookup uses MD5 — if this isn't an MD5 hash (32 hex chars), skip
    const QString cleanHash = hash.trimmed().toLower();
    if (cleanHash.length() != 32)
        return {};

    int gameId = resolveHashToGameId(cleanHash);
    if (gameId <= 0)
        return {};

    QJsonObject gameJson = fetchGameJson(gameId);
    if (gameJson.isEmpty())
        return {};

    return parseGameJson(gameJson, gameId);
}

GameMetadata RetroAchievementsProvider::getById(const QString &id)
{
    if (!m_authenticated)
        return {};

    bool ok = false;
    int gameId = id.toInt(&ok);
    if (!ok || gameId <= 0)
        return {};

    QJsonObject gameJson = fetchGameJson(gameId);
    if (gameJson.isEmpty())
        return {};

    return parseGameJson(gameJson, gameId);
}

ArtworkUrls RetroAchievementsProvider::getArtwork(const QString &id)
{
    if (!m_authenticated)
        return {};

    bool ok = false;
    int gameId = id.toInt(&ok);
    if (!ok || gameId <= 0)
        return {};

    QJsonObject json = fetchGameJson(gameId);
    if (json.isEmpty())
        return {};

    ArtworkUrls artwork;
    const QString mediaBase = QString::fromLatin1(MEDIA_BASE);

    const QString boxArt = json.value(QStringLiteral("ImageBoxArt")).toString();
    if (!boxArt.isEmpty())
        artwork.boxFront = QUrl(mediaBase + boxArt);

    const QString titleImg = json.value(QStringLiteral("ImageTitle")).toString();
    if (!titleImg.isEmpty())
        artwork.titleScreen = QUrl(mediaBase + titleImg);

    const QString ingame = json.value(QStringLiteral("ImageIngame")).toString();
    if (!ingame.isEmpty())
        artwork.screenshot = QUrl(mediaBase + ingame);

    const QString icon = json.value(QStringLiteral("ImageIcon")).toString();
    if (!icon.isEmpty())
        artwork.logo = QUrl(mediaBase + icon);

    return artwork;
}

int RetroAchievementsProvider::resolveHashToGameId(const QString &md5Hash)
{
    throttle();

    // The standard emulator hash→gameID resolution endpoint
    QUrl url(QStringLiteral("https://retroachievements.org/dorequest.php"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("r"), QStringLiteral("gameid"));
    query.addQueryItem(QStringLiteral("m"), md5Hash);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Remus/1.0 (https://github.com/asafelobotomy/remus)"));

    QNetworkReply *reply = m_networkManager->get(request);
    ApiResponse response = waitForReply(reply, REQUEST_TIMEOUT_MS);

    if (!response.success || response.data.isEmpty())
        return 0;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response.data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return 0;

    const QJsonObject obj = doc.object();
    if (!obj.value(QStringLiteral("Success")).toBool())
        return 0;

    int gameId = obj.value(QStringLiteral("GameID")).toInt(0);
    return gameId;
}

QJsonObject RetroAchievementsProvider::fetchGameJson(int gameId)
{
    throttle();

    QUrl url(QString::fromLatin1(API_BASE) + QStringLiteral("/API_GetGame.php"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("i"), QString::number(gameId));
    query.addQueryItem(QStringLiteral("y"), m_apiKey);
    url.setQuery(query);
    // NOTE: RetroAchievements API requires the key as a query parameter.
    // Do not log or print `url` after this point — it contains a plaintext API key.

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Remus/1.0 (https://github.com/asafelobotomy/remus)"));

    QNetworkReply *reply = m_networkManager->get(request);
    ApiResponse response = waitForReply(reply, REQUEST_TIMEOUT_MS);

    if (!response.success || response.data.isEmpty())
        return {};

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(response.data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return {};

    return doc.object();
}

GameMetadata RetroAchievementsProvider::parseGameJson(const QJsonObject &json,
                                                       int gameId) const
{
    GameMetadata metadata;
    metadata.id = QString::number(gameId);
    metadata.providerId = Constants::Providers::RETROACHIEVEMENTS;
    metadata.matchMethod = QStringLiteral("hash");
    metadata.matchScore = 1.0f;
    metadata.fetchedAt = QDateTime::currentDateTimeUtc();

    metadata.title = json.value(QStringLiteral("Title")).toString();
    if (metadata.title.isEmpty())
        metadata.title = json.value(QStringLiteral("GameTitle")).toString();

    metadata.system = json.value(QStringLiteral("ConsoleName")).toString();
    metadata.developer = json.value(QStringLiteral("Developer")).toString();
    metadata.publisher = json.value(QStringLiteral("Publisher")).toString();

    const QString genre = json.value(QStringLiteral("Genre")).toString();
    if (!genre.isEmpty())
        metadata.genres = genre.split(QStringLiteral(", "), Qt::SkipEmptyParts);

    // Release date: "1992-06-02 00:00:00" → "1992-06-02"
    const QString released = json.value(QStringLiteral("Released")).toString();
    if (!released.isEmpty())
        metadata.releaseDate = released.left(10);

    // Box art URL
    const QString boxArt = json.value(QStringLiteral("ImageBoxArt")).toString();
    if (!boxArt.isEmpty())
        metadata.boxArtUrl = QString::fromLatin1(MEDIA_BASE) + boxArt;

    // Screenshots
    const QString titleImg = json.value(QStringLiteral("ImageTitle")).toString();
    if (!titleImg.isEmpty())
        metadata.screenshotUrls.append(QString::fromLatin1(MEDIA_BASE) + titleImg);

    const QString ingame = json.value(QStringLiteral("ImageIngame")).toString();
    if (!ingame.isEmpty())
        metadata.screenshotUrls.append(QString::fromLatin1(MEDIA_BASE) + ingame);

    // External ID cross-reference
    metadata.externalIds.insert(Constants::Providers::ExternalId::RETROACHIEVEMENTS,
                                QString::number(gameId));

    return metadata;
}

} // namespace Remus
