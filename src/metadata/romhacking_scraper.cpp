#include "romhacking_scraper.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>

namespace Remus {

namespace {

    QByteArray fetchUrl(const QUrl &url, QString *error) {
        QNetworkAccessManager manager;
        QNetworkRequest request(url);
        request.setHeader(
            QNetworkRequest::UserAgentHeader, QStringLiteral("Remus/0.13 (+https://github.com/asafelobotomy/remus)"));

        QNetworkReply *reply = manager.get(request);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            if (error)
                *error = QStringLiteral("Network error fetching %1: %2").arg(url.toString(), reply->errorString());
            reply->deleteLater();
            return { };
        }

        const QByteArray body = reply->readAll();
        reply->deleteLater();
        return body;
    }

} // namespace

QString RomhackingScraper::modTypeFromUrl(const QString &url) {
    if (url.contains(QStringLiteral("/translations/")))
        return QStringLiteral("translation");
    if (url.contains(QStringLiteral("/utilities/")))
        return QStringLiteral("improvement");
    return QStringLiteral("hack");
}

QString RomhackingScraper::slugId(const QString &title, const QString &url) {
    static const QRegularExpression idRegex(QStringLiteral("/(hacks|translations|utilities)/(\\d+)/"));
    const QRegularExpressionMatch match = idRegex.match(url);
    if (match.hasMatch())
        return match.captured(2);
    return QString(title).replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9]+")), QStringLiteral("-")).toLower();
}

QList<ModEntry> RomhackingScraper::search(const SearchOptions &options, QString *error) {
    QList<ModEntry> mods;
    if (options.query.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("Search query is required");
        return mods;
    }

    QUrl url(QStringLiteral("https://www.romhacking.net/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page"), QStringLiteral("hacks"));
    query.addQueryItem(QStringLiteral("q"), options.query.trimmed());
    url.setQuery(query);

    const QByteArray html = fetchUrl(url, error);
    if (html.isEmpty())
        return mods;

    static const QRegularExpression linkRegex(QStringLiteral("<a\\s+href=\"(/(?:hacks|translations|utilities)/\\d+/)\""
                                                             "[^>]*>([^<]+)</a>"),
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatchIterator it = linkRegex.globalMatch(QString::fromUtf8(html));
    QSet<QString> seen;
    while (it.hasNext() && mods.size() < options.maxResults) {
        const QRegularExpressionMatch match = it.next();
        const QString path = match.captured(1);
        const QString title
            = match.captured(2).trimmed().replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
        if (title.isEmpty() || seen.contains(path))
            continue;
        seen.insert(path);

        ModEntry entry;
        entry.id = slugId(title, path);
        entry.title = title;
        entry.type = modTypeFromUrl(path);
        entry.system = options.system;
        entry.sourceUrl = QStringLiteral("https://www.romhacking.net") + path;
        entry.description
            = QStringLiteral("Discovered via romhacking.net search for \"%1\"").arg(options.query.trimmed());
        mods.append(entry);
    }

    if (mods.isEmpty() && error && error->isEmpty())
        *error = QStringLiteral("No romhacking.net results found for \"%1\"").arg(options.query.trimmed());
    return mods;
}

bool RomhackingScraper::writeCatalogJson(const QList<ModEntry> &mods, const QString &outputPath, QString *error) {
    QJsonArray arr;
    for (const ModEntry &mod : mods) {
        QJsonObject obj;
        obj[QStringLiteral("id")] = mod.id;
        obj[QStringLiteral("title")] = mod.title;
        obj[QStringLiteral("author")] = mod.author;
        obj[QStringLiteral("version")] = mod.version;
        obj[QStringLiteral("description")] = mod.description;
        obj[QStringLiteral("type")] = mod.type;
        obj[QStringLiteral("system")] = mod.system;
        obj[QStringLiteral("format")] = mod.format;
        obj[QStringLiteral("patch_url")] = mod.patchUrl;
        obj[QStringLiteral("source_url")] = mod.sourceUrl;
        obj[QStringLiteral("rating")] = mod.rating;
        obj[QStringLiteral("downloads")] = mod.downloads;
        arr.append(obj);
    }

    QJsonObject root;
    root[QStringLiteral("mods")] = arr;

    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("Failed to write catalog: %1").arg(outputPath);
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace Remus
