#include "wikidata_provider.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace Remus {

WikidataProvider::WikidataProvider(QObject *parent)
    : HttpMetadataProvider(1100, parent)  // ~1 req/sec to be polite
{
}

QList<SearchResult> WikidataProvider::searchByName(const QString &title,
                                                    const QString &system,
                                                    const QString &region)
{
    Q_UNUSED(region);
    QList<SearchResult> results;

    const QString query = buildSearchQuery(title, system);
    QJsonObject response = executeSparql(query);

    const QJsonObject resultsObj = response.value(QStringLiteral("results")).toObject();
    const QJsonArray bindings = resultsObj.value(QStringLiteral("bindings")).toArray();

    for (const QJsonValue &binding : bindings) {
        const QJsonObject b = binding.toObject();

        SearchResult result;
        // Extract entity ID from URI: http://www.wikidata.org/entity/Q12345 → Q12345
        const QString uri = b.value(QStringLiteral("item")).toObject()
                             .value(QStringLiteral("value")).toString();
        result.id = uri.mid(uri.lastIndexOf(QLatin1Char('/')) + 1);
        result.title = b.value(QStringLiteral("itemLabel")).toObject()
                        .value(QStringLiteral("value")).toString();
        result.provider = Constants::Providers::WIKIDATA;
        result.matchScore = 0.6f;  // Name match baseline

        // Exact title match gets a boost
        if (result.title.compare(title, Qt::CaseInsensitive) == 0)
            result.matchScore = 0.85f;

        if (!result.id.isEmpty() && !result.title.isEmpty())
            results.append(result);
    }

    return results;
}

GameMetadata WikidataProvider::getByHash(const QString &hash, const QString &system)
{
    Q_UNUSED(hash);
    Q_UNUSED(system);
    return {};  // Wikidata has no hash support
}

GameMetadata WikidataProvider::getById(const QString &id)
{
    const QString query = buildDetailQuery(id);
    QJsonObject response = executeSparql(query);

    const QJsonObject resultsObj = response.value(QStringLiteral("results")).toObject();
    const QJsonArray bindings = resultsObj.value(QStringLiteral("bindings")).toArray();

    if (bindings.isEmpty())
        return {};

    return parseDetailBindings(bindings, id);
}

ArtworkUrls WikidataProvider::getArtwork(const QString &id)
{
    Q_UNUSED(id);
    return {};  // Wikidata images require Wikimedia Commons URL construction — skip for now
}

QJsonObject WikidataProvider::executeSparql(const QString &query)
{
    throttle();

    QUrl url(QString::fromLatin1(SPARQL_ENDPOINT));
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("query"), query);
    urlQuery.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    url.setQuery(urlQuery);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Remus/1.0 (https://github.com/asafelobotomy/remus)"));
    request.setRawHeader("Accept", "application/sparql-results+json");

    QNetworkReply *reply = m_networkManager->get(request);
    ApiResponse apiResponse = waitForReply(reply, REQUEST_TIMEOUT_MS);

    if (!apiResponse.success || apiResponse.data.isEmpty())
        return {};

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(apiResponse.data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return {};

    return doc.object();
}

QString WikidataProvider::buildSearchQuery(const QString &title, const QString &system) const
{
    // Escape quotes in title for SPARQL string literal
    QString escapedTitle = title;
    escapedTitle.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escapedTitle.replace(QLatin1Char('"'), QStringLiteral("\\\""));

    // Base: find items that are instances of video game (Q7889) or subclasses
    QString filter;
    if (!system.isEmpty()) {
        // Add platform filter if system provided
        QString escapedSystem = system;
        escapedSystem.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        escapedSystem.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        filter = QStringLiteral(
            "  ?item wdt:P400 ?platform .\n"
            "  ?platform rdfs:label ?platLabel .\n"
            "  FILTER(LANG(?platLabel) = \"en\")\n"
            "  FILTER(CONTAINS(LCASE(?platLabel), LCASE(\"%1\")))\n"
        ).arg(escapedSystem);
    }

    return QStringLiteral(
        "SELECT DISTINCT ?item ?itemLabel WHERE {\n"
        "  ?item wdt:P31/wdt:P279* wd:Q7889 .\n"
        "  ?item rdfs:label ?itemLabel .\n"
        "  FILTER(LANG(?itemLabel) = \"en\")\n"
        "  FILTER(CONTAINS(LCASE(?itemLabel), LCASE(\"%1\")))\n"
        "%2"
        "}\n"
        "LIMIT 10\n"
    ).arg(escapedTitle, filter);
}

QString WikidataProvider::buildDetailQuery(const QString &entityId) const
{
    return QStringLiteral(
        "SELECT ?itemLabel ?itemDescription ?genreLabel ?developerLabel "
        "?publisherLabel ?pubDate ?platformLabel WHERE {\n"
        "  BIND(wd:%1 AS ?item)\n"
        "  OPTIONAL { ?item wdt:P136 ?genre . ?genre rdfs:label ?genreLabel . FILTER(LANG(?genreLabel) = \"en\") }\n"
        "  OPTIONAL { ?item wdt:P178 ?developer . ?developer rdfs:label ?developerLabel . FILTER(LANG(?developerLabel) = \"en\") }\n"
        "  OPTIONAL { ?item wdt:P123 ?publisher . ?publisher rdfs:label ?publisherLabel . FILTER(LANG(?publisherLabel) = \"en\") }\n"
        "  OPTIONAL { ?item wdt:P577 ?pubDate }\n"
        "  OPTIONAL { ?item wdt:P400 ?platform . ?platform rdfs:label ?platformLabel . FILTER(LANG(?platformLabel) = \"en\") }\n"
        "  SERVICE wikibase:label { bd:serviceParam wikibase:language \"en\" }\n"
        "}\n"
    ).arg(entityId);
}

GameMetadata WikidataProvider::parseDetailBindings(const QJsonArray &bindings,
                                                    const QString &entityId) const
{
    GameMetadata metadata;
    metadata.id = entityId;
    metadata.providerId = Constants::Providers::WIKIDATA;
    metadata.matchMethod = QStringLiteral("name");
    metadata.fetchedAt = QDateTime::currentDateTimeUtc();

    QSet<QString> seenGenres;
    QSet<QString> seenPlatforms;

    for (const QJsonValue &bindingVal : bindings) {
        const QJsonObject b = bindingVal.toObject();

        // Title (take first non-empty)
        if (metadata.title.isEmpty()) {
            metadata.title = b.value(QStringLiteral("itemLabel")).toObject()
                              .value(QStringLiteral("value")).toString();
        }

        // Description
        if (metadata.description.isEmpty()) {
            metadata.description = b.value(QStringLiteral("itemDescription")).toObject()
                                    .value(QStringLiteral("value")).toString();
        }

        // Genre (collect unique)
        const QString genre = b.value(QStringLiteral("genreLabel")).toObject()
                               .value(QStringLiteral("value")).toString();
        if (!genre.isEmpty() && !seenGenres.contains(genre)) {
            seenGenres.insert(genre);
            metadata.genres.append(genre);
        }

        // Developer (take first)
        if (metadata.developer.isEmpty()) {
            metadata.developer = b.value(QStringLiteral("developerLabel")).toObject()
                                  .value(QStringLiteral("value")).toString();
        }

        // Publisher (take first)
        if (metadata.publisher.isEmpty()) {
            metadata.publisher = b.value(QStringLiteral("publisherLabel")).toObject()
                                  .value(QStringLiteral("value")).toString();
        }

        // Release date (take earliest)
        const QString dateStr = b.value(QStringLiteral("pubDate")).toObject()
                                 .value(QStringLiteral("value")).toString();
        if (!dateStr.isEmpty()) {
            // Wikidata returns ISO 8601 dates like "1996-03-22T00:00:00Z"
            const QString isoDate = dateStr.left(10);
            if (metadata.releaseDate.isEmpty() || isoDate < metadata.releaseDate)
                metadata.releaseDate = isoDate;
        }

        // Platform (take first for system field)
        const QString platform = b.value(QStringLiteral("platformLabel")).toObject()
                                  .value(QStringLiteral("value")).toString();
        if (!platform.isEmpty() && !seenPlatforms.contains(platform)) {
            seenPlatforms.insert(platform);
            if (metadata.system.isEmpty())
                metadata.system = platform;
        }
    }

    // Set external ID
    metadata.externalIds.insert(QStringLiteral("wikidata"), entityId);

    return metadata;
}

} // namespace Remus
