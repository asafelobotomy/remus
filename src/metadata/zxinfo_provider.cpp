#include "zxinfo_provider.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace Remus {

static constexpr int ZXINFO_RATE_MS = 250;
static constexpr int ZXINFO_TIMEOUT_MS = 15'000;
static const char *ZXINFO_BASE_URL = "https://api.zxinfo.dk/v3";

ZXInfoProvider::ZXInfoProvider(QObject *parent)
    : HttpMetadataProvider(ZXINFO_RATE_MS, parent) { }

QList<SearchResult> ZXInfoProvider::searchByName(
    const QString &title, const QString & /*system*/, const QString & /*region*/) {
    QList<SearchResult> results;
    const QList<GameMetadata> mds = searchAndFetch(title, 5);
    for (const GameMetadata &gm : mds) {
        SearchResult sr;
        sr.id = gm.id;
        sr.title = gm.title;
        sr.matchScore = 0.75f;
        results.append(sr);
    }
    return results;
}

GameMetadata ZXInfoProvider::getByHash(const QString &, const QString &) {
    GameMetadata m;
    emit errorOccurred(QStringLiteral("ZXInfo does not support hash-based lookups"));
    return m;
}

GameMetadata ZXInfoProvider::getById(const QString &id) {
    GameMetadata m;
    if (id.isEmpty())
        return m;

    const QUrl url(QStringLiteral("%1/games/%2").arg(QString::fromLatin1(ZXINFO_BASE_URL), id));
    const ApiResponse resp = makeRequest(url);
    if (!resp.success)
        return m;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(resp.data, &err);
    if (err.error != QJsonParseError::NoError)
        return m;

    return parseEntry(doc.object().value(QStringLiteral("_source")).toObject());
}

ArtworkUrls ZXInfoProvider::getArtwork(const QString &) {
    return { };
}

QList<GameMetadata> ZXInfoProvider::searchAndFetch(const QString &title, int limit) {
    QList<GameMetadata> results;
    if (title.isEmpty())
        return results;

    QUrl url(QStringLiteral("%1/search").arg(QString::fromLatin1(ZXINFO_BASE_URL)));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("query"), title);
    q.addQueryItem(QStringLiteral("size"), QString::number(limit));
    q.addQueryItem(QStringLiteral("contenttype"), QStringLiteral("SOFTWARE"));
    url.setQuery(q);

    const ApiResponse resp = makeRequest(url);
    if (!resp.success)
        return results;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(resp.data, &err);
    if (err.error != QJsonParseError::NoError)
        return results;

    const QJsonArray hits
        = doc.object().value(QStringLiteral("hits")).toObject().value(QStringLiteral("hits")).toArray();

    results.reserve(hits.size());
    for (const QJsonValue &hit : hits) {
        const QJsonObject h = hit.toObject();
        GameMetadata gm = parseEntry(h.value(QStringLiteral("_source")).toObject());
        if (!gm.title.isEmpty()) {
            gm.id = h.value(QStringLiteral("_id")).toString();
            results.append(gm);
        }
    }

    return results;
}

// ── private helpers ───────────────────────────────────────────────────────────

ZXInfoProvider::ApiResponse ZXInfoProvider::makeRequest(const QUrl &url) {
    m_rateLimiter->waitIfNeeded();
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("remus-cli/1.0"));
    QNetworkReply *reply = m_networkManager->get(request);
    return waitForReply(reply, ZXINFO_TIMEOUT_MS);
}

GameMetadata ZXInfoProvider::parseEntry(const QJsonObject &source) const {
    GameMetadata m;
    m.providerId = QStringLiteral("zxinfo");
    m.title = source.value(QStringLiteral("title")).toString();

    // Genre: prefer genreSubType ("Platform"), fall back to genreType ("Arcade Game")
    const QString sub = source.value(QStringLiteral("genreSubType")).toString().trimmed();
    const QString type = source.value(QStringLiteral("genreType")).toString().trimmed();
    if (!sub.isEmpty())
        m.genres.append(sub);
    else if (!type.isEmpty())
        m.genres.append(type);

    // Release year
    const int year = source.value(QStringLiteral("originalYearOfRelease")).toInt();
    if (year > 1970 && year < 2030)
        m.releaseDate = QString::number(year);

    // Publisher: first entry in publishers[]
    const QJsonArray pubs = source.value(QStringLiteral("publishers")).toArray();
    if (!pubs.isEmpty())
        m.publisher = pubs.first().toObject().value(QStringLiteral("name")).toString().trimmed();

    // Developer: look for authors with labelType starting with "Company" (e.g.
    // "Company: Publisher/Manager", "Company: User group") and type "Creator"
    const QJsonArray authors = source.value(QStringLiteral("authors")).toArray();
    for (const QJsonValue &av : authors) {
        const QJsonObject a = av.toObject();
        if (a.value(QStringLiteral("labelType")).toString().startsWith(QStringLiteral("Company"))
            && a.value(QStringLiteral("type")).toString() == QStringLiteral("Creator")) {
            m.developer = a.value(QStringLiteral("name")).toString().trimmed();
            break;
        }
    }

    // Description: the remarks field (may be absent or null for many entries)
    m.description = source.value(QStringLiteral("remarks")).toString().trimmed();

    return m;
}

} // namespace Remus
