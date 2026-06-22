#include "wikidata_provider.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace Remus {

WikidataProvider::WikidataProvider(QObject *parent)
    : HttpMetadataProvider(1100, parent) // ~1 req/sec to be polite
{ }

QList<SearchResult> WikidataProvider::searchByName(const QString &title, const QString &system, const QString &region) {
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
        const QString uri = b.value(QStringLiteral("item")).toObject().value(QStringLiteral("value")).toString();
        result.id = uri.mid(uri.lastIndexOf(QLatin1Char('/')) + 1);
        result.title = b.value(QStringLiteral("itemLabel")).toObject().value(QStringLiteral("value")).toString();
        result.provider = Constants::Providers::WIKIDATA;
        result.matchScore = 0.6f; // Name match baseline

        // Exact title match gets a boost
        if (result.title.compare(title, Qt::CaseInsensitive) == 0)
            result.matchScore = 0.85f;

        if (!result.id.isEmpty() && !result.title.isEmpty())
            results.append(result);
    }

    return results;
}

GameMetadata WikidataProvider::getByHash(const QString &hash, const QString &system) {
    Q_UNUSED(hash);
    Q_UNUSED(system);
    return { }; // Wikidata has no hash support
}

GameMetadata WikidataProvider::getById(const QString &id) {
    const QString query = buildDetailQuery(id);
    QJsonObject response = executeSparql(query);

    const QJsonObject resultsObj = response.value(QStringLiteral("results")).toObject();
    const QJsonArray bindings = resultsObj.value(QStringLiteral("bindings")).toArray();

    if (bindings.isEmpty())
        return { };

    return parseDetailBindings(bindings, id);
}

ArtworkUrls WikidataProvider::getArtwork(const QString &id) {
    ArtworkUrls urls;

    // Query Wikidata for P18 (image) property
    const QString query = QStringLiteral("SELECT ?image WHERE {\n"
                                         "  wd:%1 wdt:P18 ?image .\n"
                                         "}\n"
                                         "LIMIT 1\n")
                              .arg(id);

    QJsonObject response = executeSparql(query);
    const QJsonArray bindings
        = response.value(QStringLiteral("results")).toObject().value(QStringLiteral("bindings")).toArray();

    if (bindings.isEmpty())
        return urls;

    // P18 returns a Wikimedia Commons filename URL like:
    // http://commons.wikimedia.org/wiki/Special:FilePath/Example.jpg
    const QString imageUrl = bindings.first()
                                 .toObject()
                                 .value(QStringLiteral("image"))
                                 .toObject()
                                 .value(QStringLiteral("value"))
                                 .toString();

    if (!imageUrl.isEmpty())
        urls.boxFront = QUrl(imageUrl);

    return urls;
}

QJsonObject WikidataProvider::executeSparql(const QString &query) {
    throttle();

    QUrl url(QString::fromLatin1(SPARQL_ENDPOINT));
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("query"), query);
    urlQuery.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    url.setQuery(urlQuery);

    QNetworkRequest request(url);
    request.setHeader(
        QNetworkRequest::UserAgentHeader, QStringLiteral("Remus/1.0 (https://github.com/asafelobotomy/remus)"));
    request.setRawHeader("Accept", "application/sparql-results+json");

    QNetworkReply *reply = m_networkManager->get(request);
    ApiResponse apiResponse = waitForReply(reply, REQUEST_TIMEOUT_MS);

    if (!apiResponse.success || apiResponse.data.isEmpty())
        return { };

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(apiResponse.data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return { };

    return doc.object();
}

QString WikidataProvider::buildSearchQuery(const QString &title, const QString &system) const {
    // Escape quotes in title for SPARQL string literal
    QString escapedTitle = title;
    escapedTitle.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escapedTitle.replace(QLatin1Char('"'), QStringLiteral("\\\""));

    const QString filter = platformFilterClause(system);

    return QStringLiteral("SELECT DISTINCT ?item ?itemLabel WHERE {\n"
                          "  ?item wdt:P31/wdt:P279* wd:Q7889 .\n"
                          "  ?item rdfs:label ?itemLabel .\n"
                          "  FILTER(LANG(?itemLabel) = \"en\")\n"
                          "  FILTER(CONTAINS(LCASE(?itemLabel), LCASE(\"%1\")))\n"
                          "%2"
                          "}\n"
                          "LIMIT 10\n")
        .arg(escapedTitle, filter);
}

QString WikidataProvider::platformFilterClause(const QString &system) const {
    if (system.isEmpty())
        return QString();

    QStringList variants;
    const QStringList parts = system.split(QStringLiteral(" / "));
    for (const QString &part : parts) {
        QString escaped = part.trimmed();
        escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        if (!escaped.isEmpty())
            variants.append(QStringLiteral("CONTAINS(LCASE(?platLabel), LCASE(\"%1\"))").arg(escaped));
    }
    if (variants.isEmpty()) {
        QString escapedSystem = system;
        escapedSystem.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        escapedSystem.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        variants.append(QStringLiteral("CONTAINS(LCASE(?platLabel), LCASE(\"%1\"))").arg(escapedSystem));
    }

    return QStringLiteral("  ?item wdt:P400 ?platform .\n"
                          "  ?platform rdfs:label ?platLabel .\n"
                          "  FILTER(LANG(?platLabel) = \"en\")\n"
                          "  FILTER(%1)\n")
        .arg(variants.join(QStringLiteral(" || ")));
}

QString WikidataProvider::buildPlatformBulkQuery(const QString &system, int limit, int offset) const {
    const QString filter = platformFilterClause(system);
    if (filter.isEmpty())
        return QString();

    return QStringLiteral("SELECT ?item ?itemLabel ?itemDescription ?genreLabel ?developerLabel "
                          "?publisherLabel ?pubDate ?platformLabel WHERE {\n"
                          "  ?item wdt:P31/wdt:P279* wd:Q7889 .\n"
                          "%1"
                          "  OPTIONAL { ?item wdt:P136 ?genre . ?genre rdfs:label ?genreLabel . "
                          "FILTER(LANG(?genreLabel) = \"en\") }\n"
                          "  OPTIONAL { ?item wdt:P178 ?developer . ?developer rdfs:label ?developerLabel . "
                          "FILTER(LANG(?developerLabel) = \"en\") }\n"
                          "  OPTIONAL { ?item wdt:P123 ?publisher . ?publisher rdfs:label ?publisherLabel . "
                          "FILTER(LANG(?publisherLabel) = \"en\") }\n"
                          "  OPTIONAL { ?item wdt:P577 ?pubDate }\n"
                          "  OPTIONAL { ?item wdt:P400 ?platform . ?platform rdfs:label ?platformLabel . "
                          "FILTER(LANG(?platformLabel) = \"en\") }\n"
                          "  SERVICE wikibase:label { bd:serviceParam wikibase:language \"en\" }\n"
                          "}\n"
                          "LIMIT %2 OFFSET %3\n")
        .arg(filter)
        .arg(limit)
        .arg(offset);
}

QList<GameMetadata> WikidataProvider::fetchGamesForPlatform(const QString &systemDisplayName, int limit, int offset) {
    QList<GameMetadata> results;
    if (systemDisplayName.trimmed().isEmpty())
        return results;

    const QString query = buildPlatformBulkQuery(systemDisplayName, limit, offset);
    if (query.isEmpty())
        return results;

    const QJsonObject response = executeSparql(query);
    const QJsonArray bindings
        = response.value(QStringLiteral("results")).toObject().value(QStringLiteral("bindings")).toArray();
    if (bindings.isEmpty())
        return results;

    QHash<QString, QJsonArray> grouped;
    grouped.reserve(bindings.size());
    for (const QJsonValue &bindingVal : bindings) {
        const QJsonObject binding = bindingVal.toObject();
        const QString uri = binding.value(QStringLiteral("item")).toObject().value(QStringLiteral("value")).toString();
        const QString entityId = uri.mid(uri.lastIndexOf(QLatin1Char('/')) + 1);
        if (entityId.isEmpty())
            continue;
        grouped[entityId].append(binding);
    }

    results.reserve(grouped.size());
    for (auto it = grouped.constBegin(); it != grouped.constEnd(); ++it)
        results.append(parseDetailBindings(it.value(), it.key()));

    return results;
}

QString WikidataProvider::buildDetailQuery(const QString &entityId) const {
    return QStringLiteral(
        "SELECT ?itemLabel ?itemDescription ?genreLabel ?developerLabel "
        "?publisherLabel ?pubDate ?platformLabel WHERE {\n"
        "  BIND(wd:%1 AS ?item)\n"
        "  OPTIONAL { ?item wdt:P136 ?genre . ?genre rdfs:label ?genreLabel . FILTER(LANG(?genreLabel) = \"en\") }\n"
        "  OPTIONAL { ?item wdt:P178 ?developer . ?developer rdfs:label ?developerLabel . FILTER(LANG(?developerLabel) "
        "= \"en\") }\n"
        "  OPTIONAL { ?item wdt:P123 ?publisher . ?publisher rdfs:label ?publisherLabel . FILTER(LANG(?publisherLabel) "
        "= \"en\") }\n"
        "  OPTIONAL { ?item wdt:P577 ?pubDate }\n"
        "  OPTIONAL { ?item wdt:P400 ?platform . ?platform rdfs:label ?platformLabel . FILTER(LANG(?platformLabel) = "
        "\"en\") }\n"
        "  SERVICE wikibase:label { bd:serviceParam wikibase:language \"en\" }\n"
        "}\n")
        .arg(entityId);
}

GameMetadata WikidataProvider::parseDetailBindings(const QJsonArray &bindings, const QString &entityId) const {
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
            metadata.title = b.value(QStringLiteral("itemLabel")).toObject().value(QStringLiteral("value")).toString();
        }

        // Description — itemDescription is a short Wikidata entity disambiguator
        // (e.g. "1991 beat 'em up video game"). Accept any non-trivial value (≥10
        // chars); a richer provider like ScreenScraper will replace it if available.
        if (metadata.description.isEmpty()) {
            const QString candidate
                = b.value(QStringLiteral("itemDescription")).toObject().value(QStringLiteral("value")).toString();
            if (candidate.length() >= 10) {
                metadata.description = candidate;
            }
        }

        // Genre (collect unique)
        const QString genre
            = b.value(QStringLiteral("genreLabel")).toObject().value(QStringLiteral("value")).toString();
        if (!genre.isEmpty() && !seenGenres.contains(genre)) {
            seenGenres.insert(genre);
            metadata.genres.append(genre);
        }

        // Developer (take first)
        if (metadata.developer.isEmpty()) {
            metadata.developer
                = b.value(QStringLiteral("developerLabel")).toObject().value(QStringLiteral("value")).toString();
        }

        // Publisher (take first)
        if (metadata.publisher.isEmpty()) {
            metadata.publisher
                = b.value(QStringLiteral("publisherLabel")).toObject().value(QStringLiteral("value")).toString();
        }

        // Release date (take earliest)
        const QString dateStr = b.value(QStringLiteral("pubDate")).toObject().value(QStringLiteral("value")).toString();
        if (!dateStr.isEmpty()) {
            // Wikidata returns ISO 8601 dates like "1996-03-22T00:00:00Z"
            const QString isoDate = dateStr.left(10);
            if (metadata.releaseDate.isEmpty() || isoDate < metadata.releaseDate)
                metadata.releaseDate = isoDate;
        }

        // Platform (take first for system field)
        const QString platform
            = b.value(QStringLiteral("platformLabel")).toObject().value(QStringLiteral("value")).toString();
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
