#include "retroachievements_enricher.h"

#include "credential_manager.h"
#include "../core/constants/api.h"
#include "../core/constants/network.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace Remus {

// ---------------------------------------------------------------------------
// Credential management
// ---------------------------------------------------------------------------

void RetroAchievementsEnricher::setApiKey(const QString &username,
                                           const QString &apiKey)
{
    m_username = username;
    m_apiKey   = apiKey;
}

bool RetroAchievementsEnricher::hasApiKey() const
{
    return !effectiveApiKey().isEmpty() && !effectiveUsername().isEmpty();
}

QString RetroAchievementsEnricher::effectiveApiKey() const
{
    if (!m_apiKey.isEmpty())
        return m_apiKey;
    return CredentialManager::get(QStringLiteral("retroachievements/api_key"));
}

QString RetroAchievementsEnricher::effectiveUsername() const
{
    if (!m_username.isEmpty())
        return m_username;
    return CredentialManager::get(QStringLiteral("retroachievements/username"));
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QByteArray RetroAchievementsEnricher::makeApiRequest(
        const QString &endpoint,
        const QMap<QString, QString> &params) const
{
    QUrl url(QString::fromLatin1(Constants::API::RA_API_BASE_URL) + endpoint);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("z"), effectiveUsername());
    query.addQueryItem(QStringLiteral("y"), effectiveApiKey());
    for (auto it = params.cbegin(); it != params.cend(); ++it)
        query.addQueryItem(it.key(), it.value());
    url.setQuery(query);

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      Constants::API::USER_AGENT);

    QNetworkReply *reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    // Timeout safety
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(Constants::Network::METADATA_TIMEOUT_MS);

    loop.exec();

    QByteArray data;
    if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
        data = reply->readAll();
    } else {
        m_lastError = reply->errorString();
    }
    reply->deleteLater();

    // Polite rate limiting
    QThread::msleep(Constants::Network::RA_RATE_LIMIT_MS);

    return data;
}

// ---------------------------------------------------------------------------
// fetchSystemList
// ---------------------------------------------------------------------------

QMap<int, QString> RetroAchievementsEnricher::fetchSystemList() const
{
    QMap<int, QString> systems;

    if (!hasApiKey()) {
        m_lastError = QStringLiteral("No RA API key — skipping system list fetch");
        return systems;
    }

    QByteArray data = makeApiRequest(
        QString::fromLatin1(Constants::API::RA_CONSOLE_IDS_ENDPOINT), {});
    if (data.isEmpty())
        return systems;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        m_lastError = QStringLiteral("JSON parse error: ") + parseError.errorString();
        return systems;
    }

    const QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        const QJsonObject obj = val.toObject();
        int id = obj.value(QStringLiteral("ID")).toInt();
        QString name = obj.value(QStringLiteral("Name")).toString();
        if (id > 0 && !name.isEmpty())
            systems.insert(id, name);
    }

    return systems;
}

// ---------------------------------------------------------------------------
// fetchGameHashes
// ---------------------------------------------------------------------------

QList<RetroAchievementsEnricher::HashPatchEntry>
RetroAchievementsEnricher::fetchGameHashes(int gameId) const
{
    QList<HashPatchEntry> entries;

    if (!hasApiKey()) {
        m_lastError = QStringLiteral("No RA API key — skipping hash fetch");
        return entries;
    }

    QMap<QString, QString> params;
    params.insert(QStringLiteral("i"), QString::number(gameId));

    QByteArray data = makeApiRequest(
        QString::fromLatin1(Constants::API::RA_GAME_HASHES_ENDPOINT), params);
    if (data.isEmpty())
        return entries;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        m_lastError = QStringLiteral("JSON parse error: ") + parseError.errorString();
        return entries;
    }

    const QJsonObject root = doc.object();
    const QJsonArray results = root.value(QStringLiteral("Results")).toArray();

    for (const QJsonValue &val : results) {
        const QJsonObject obj = val.toObject();
        HashPatchEntry entry;
        entry.md5      = obj.value(QStringLiteral("MD5")).toString().toLower();
        entry.name     = obj.value(QStringLiteral("Name")).toString();
        entry.patchUrl = obj.value(QStringLiteral("PatchUrl")).toString();

        const QJsonArray labels = obj.value(QStringLiteral("Labels")).toArray();
        for (const QJsonValue &l : labels)
            entry.labels.append(l.toString());

        if (!entry.md5.isEmpty())
            entries.append(entry);
    }

    return entries;
}

// ---------------------------------------------------------------------------
// enrichCatalog
// ---------------------------------------------------------------------------

RetroAchievementsEnricher::EnrichResult
RetroAchievementsEnricher::enrichCatalog(QList<ModEntry> &mods) const
{
    EnrichResult result;

    if (!hasApiKey()) {
        result.skippedNoApiKey = true;
        result.skippedCount = mods.size();
        m_lastError = QStringLiteral("No RA API key — enrichment skipped");
        return result;
    }

    // Build a reverse index: baseMd5 → list of indices in `mods`
    QMap<QString, QList<int>> md5ToIndices;
    for (int i = 0; i < mods.size(); ++i) {
        const QString md5 = mods[i].baseMd5.toLower().trimmed();
        if (!md5.isEmpty())
            md5ToIndices[md5].append(i);
    }

    if (md5ToIndices.isEmpty()) {
        result.skippedCount = mods.size();
        return result;
    }

    // Fetch the system list so we can iterate by console
    QMap<int, QString> systems = fetchSystemList();
    if (systems.isEmpty()) {
        result.error = m_lastError;
        return result;
    }

    // For each system, fetch game list to get gameId→hashes mapping
    for (auto sysIt = systems.cbegin(); sysIt != systems.cend(); ++sysIt) {
        if (md5ToIndices.isEmpty())
            break;  // all matched

        QMap<QString, QString> params;
        params.insert(QStringLiteral("i"), QString::number(sysIt.key()));
        params.insert(QStringLiteral("h"), QStringLiteral("1")); // include hashes
        params.insert(QStringLiteral("f"), QStringLiteral("1")); // only games with hashes

        QByteArray data = makeApiRequest(
            QString::fromLatin1(Constants::API::RA_GAME_LIST_ENDPOINT), params);
        if (data.isEmpty())
            continue;

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError)
            continue;

        const QJsonArray games = doc.array();
        for (const QJsonValue &gv : games) {
            const QJsonObject game = gv.toObject();
            int gameId = game.value(QStringLiteral("ID")).toInt();
            const QString hashes = game.value(QStringLiteral("Hashes")).toString();

            if (gameId <= 0 || hashes.isEmpty())
                continue;

            // Hashes is a tilde-delimited string of MD5s
            const QStringList hashList = hashes.split(QLatin1Char('~'),
                                                       Qt::SkipEmptyParts);
            for (const QString &h : hashList) {
                const QString md5 = h.toLower().trimmed();
                if (!md5ToIndices.contains(md5))
                    continue;

                // Match found — fetch detailed hashes for PatchUrl
                QList<HashPatchEntry> patchEntries = fetchGameHashes(gameId);

                for (const HashPatchEntry &pe : patchEntries) {
                    const QString peMd5 = pe.md5.toLower().trimmed();
                    if (!md5ToIndices.contains(peMd5))
                        continue;

                    const QList<int> &indices = md5ToIndices[peMd5];
                    for (int idx : indices) {
                        if (mods[idx].patchUrl.isEmpty() && !pe.patchUrl.isEmpty()) {
                            mods[idx].patchUrl = pe.patchUrl;
                            ++result.enrichedCount;
                        }
                    }
                }

                // Remove matched MD5s to avoid re-fetching
                md5ToIndices.remove(md5);
            }
        }
    }

    result.skippedCount = mods.size() - result.enrichedCount;
    return result;
}

} // namespace Remus
