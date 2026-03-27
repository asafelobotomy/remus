#include "cli_mod_support.h"

#include "cli_commands.h"

#include <algorithm>
#include <QJsonDocument>
#include <QTextStream>
#include <QUrl>

#include "../core/constants/constants.h"

namespace {

bool useDescendingSort(const QString &sortBy)
{
    return sortBy == QStringLiteral("rating") || sortBy == QStringLiteral("downloads");
}

} // namespace

bool parseModQueryOptions(const QCommandLineParser &parser,
                          ModQueryOptions          &options,
                          QString                  &error)
{
    options.systemFilter = parser.isSet("mod-system") ? parser.value("mod-system").trimmed() : QString();
    options.authorFilter = parser.isSet("mod-author") ? parser.value("mod-author").trimmed() : QString();
    options.typeFilter = parser.isSet("mod-type") ? parser.value("mod-type").trimmed() : QString();
    options.formatFilter = parser.isSet("mod-format") ? parser.value("mod-format").trimmed() : QString();
    options.sourceUrlFilter = parser.isSet("mod-source-url") ? parser.value("mod-source-url").trimmed() : QString();
    options.sortBy = parser.isSet("mod-sort") ? parser.value("mod-sort").trimmed().toLower() : QString();
    options.jsonOutput = parser.isSet(Remus::Constants::Cli::Options::JSON) || parser.isSet(Remus::Constants::Cli::Options::MOD_JSON);
    options.allowSystemFallback = !parser.isSet("mod-no-system-fallback");

    if (parser.isSet("mod-system") && options.systemFilter.isEmpty()) {
        error = QStringLiteral("Invalid system name for --mod-system");
        return false;
    }
    if (parser.isSet("mod-author") && options.authorFilter.isEmpty()) {
        error = QStringLiteral("Invalid author text for --mod-author");
        return false;
    }
    if (parser.isSet("mod-type") && options.typeFilter.isEmpty()) {
        error = QStringLiteral("Invalid type for --mod-type");
        return false;
    }
    if (parser.isSet("mod-format") && options.formatFilter.isEmpty()) {
        error = QStringLiteral("Invalid format for --mod-format");
        return false;
    }
    if (parser.isSet("mod-source-url") && options.sourceUrlFilter.isEmpty()) {
        error = QStringLiteral("Invalid text for --mod-source-url");
        return false;
    }

    if (!options.sortBy.isEmpty()) {
        static const QStringList validSorts = {
            QStringLiteral("title"),
            QStringLiteral("author"),
            QStringLiteral("system"),
            QStringLiteral("type"),
            QStringLiteral("format"),
            QStringLiteral("rating"),
            QStringLiteral("downloads")
        };
        if (!validSorts.contains(options.sortBy)) {
            error = QStringLiteral("Invalid value for --mod-sort (expected title, author, system, type, format, rating, downloads)");
            return false;
        }
    }

    if (parser.isSet("mod-min-rating")) {
        bool ok = false;
        options.minRating = parser.value("mod-min-rating").toDouble(&ok);
        if (!ok || options.minRating < 0.0 || options.minRating > 5.0) {
            error = QStringLiteral("Invalid rating for --mod-min-rating (expected 0.0 to 5.0)");
            return false;
        }
    }

    if (parser.isSet("mod-min-downloads")) {
        bool ok = false;
        options.minDownloads = parser.value("mod-min-downloads").toInt(&ok);
        if (!ok || options.minDownloads < 0) {
            error = QStringLiteral("Invalid value for --mod-min-downloads (expected integer >= 0)");
            return false;
        }
    }

    return true;
}

QList<Remus::ModEntry> filterCatalogMods(const QList<Remus::ModEntry> &mods,
                                         const ModQueryOptions         &options)
{
    QList<Remus::ModEntry> filtered;
    for (const auto &mod : mods) {
        if (!options.systemFilter.isEmpty()) {
            // Support display names with aliases like "Sega Genesis / Mega Drive":
            // match if either side contains the other, or any slash-delimited alias matches.
            bool systemMatch = (mod.system.compare(options.systemFilter, Qt::CaseInsensitive) == 0);
            if (!systemMatch) {
                const QStringList aliases = options.systemFilter.split(QStringLiteral(" / "));
                for (const QString &alias : aliases) {
                    if (mod.system.compare(alias.trimmed(), Qt::CaseInsensitive) == 0
                        || mod.system.contains(alias.trimmed(), Qt::CaseInsensitive)) {
                        systemMatch = true;
                        break;
                    }
                }
            }
            if (!systemMatch) continue;
        }
        if (!options.authorFilter.isEmpty() && !mod.author.contains(options.authorFilter, Qt::CaseInsensitive)) {
            continue;
        }
        if (!options.typeFilter.isEmpty() && mod.type.compare(options.typeFilter, Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (!options.formatFilter.isEmpty() && mod.format.compare(options.formatFilter, Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (!options.sourceUrlFilter.isEmpty() && !mod.sourceUrl.contains(options.sourceUrlFilter, Qt::CaseInsensitive)) {
            continue;
        }
        if (options.minRating >= 0.0 && mod.rating < options.minRating) {
            continue;
        }
        if (options.minDownloads >= 0 && mod.downloads < options.minDownloads) {
            continue;
        }
        filtered.append(mod);
    }
    return filtered;
}

QList<ListedMod> withScope(const QList<Remus::ModEntry> &mods,
                           const QString                &scope)
{
    QList<ListedMod> rows;
    rows.reserve(mods.size());
    for (const auto &mod : mods) {
        rows.append({mod, scope});
    }
    return rows;
}

QList<ListedMod> sortListedMods(const QList<ListedMod> &mods,
                                const ModQueryOptions  &options)
{
    QList<ListedMod> sorted = mods;
    if (options.sortBy.isEmpty()) {
        return sorted;
    }

    const bool descending = useDescendingSort(options.sortBy);
    std::sort(sorted.begin(), sorted.end(), [&](const ListedMod &left, const ListedMod &right) {
        auto compareStrings = [&](const QString &lhs, const QString &rhs) {
            return QString::compare(lhs, rhs, Qt::CaseInsensitive) < 0;
        };

        bool less = false;
        bool equal = false;

        if (options.sortBy == QStringLiteral("title")) {
            less = compareStrings(left.mod.title, right.mod.title);
            equal = QString::compare(left.mod.title, right.mod.title, Qt::CaseInsensitive) == 0;
        } else if (options.sortBy == QStringLiteral("author")) {
            less = compareStrings(left.mod.author, right.mod.author);
            equal = QString::compare(left.mod.author, right.mod.author, Qt::CaseInsensitive) == 0;
        } else if (options.sortBy == QStringLiteral("system")) {
            less = compareStrings(left.mod.system, right.mod.system);
            equal = QString::compare(left.mod.system, right.mod.system, Qt::CaseInsensitive) == 0;
        } else if (options.sortBy == QStringLiteral("type")) {
            less = compareStrings(left.mod.type, right.mod.type);
            equal = QString::compare(left.mod.type, right.mod.type, Qt::CaseInsensitive) == 0;
        } else if (options.sortBy == QStringLiteral("format")) {
            less = compareStrings(left.mod.format, right.mod.format);
            equal = QString::compare(left.mod.format, right.mod.format, Qt::CaseInsensitive) == 0;
        } else if (options.sortBy == QStringLiteral("rating")) {
            less = left.mod.rating < right.mod.rating;
            equal = qFuzzyCompare(left.mod.rating + 1.0, right.mod.rating + 1.0);
        } else if (options.sortBy == QStringLiteral("downloads")) {
            less = left.mod.downloads < right.mod.downloads;
            equal = left.mod.downloads == right.mod.downloads;
        }

        if (equal) {
            return QString::compare(left.mod.title, right.mod.title, Qt::CaseInsensitive) < 0;
        }
        return descending ? !less : less;
    });

    return sorted;
}

QString describeActiveFilters(const ModQueryOptions &options)
{
    QStringList parts;
    if (!options.systemFilter.isEmpty()) {
        parts << QString("system=\"%1\"").arg(options.systemFilter);
    }
    if (!options.authorFilter.isEmpty()) {
        parts << QString("author~\"%1\"").arg(options.authorFilter);
    }
    if (!options.typeFilter.isEmpty()) {
        parts << QString("type=\"%1\"").arg(options.typeFilter);
    }
    if (!options.formatFilter.isEmpty()) {
        parts << QString("format=\"%1\"").arg(options.formatFilter);
    }
    if (!options.sourceUrlFilter.isEmpty()) {
        parts << QString("source~\"%1\"").arg(options.sourceUrlFilter);
    }
    if (options.minRating >= 0.0) {
        parts << QString("rating>=%1").arg(QString::number(options.minRating, 'f', 1));
    }
    if (options.minDownloads >= 0) {
        parts << QString("downloads>=%1").arg(options.minDownloads);
    }
    if (!options.sortBy.isEmpty()) {
        parts << QString("sort=%1").arg(options.sortBy);
    }
    return parts.join(QStringLiteral(", "));
}

QJsonObject listedModToJson(const ListedMod &row)
{
    const auto &mod = row.mod;

    QJsonObject object;
    object["id"] = mod.id;
    object["title"] = mod.title;
    object["author"] = mod.author;
    object["version"] = mod.version;
    object["description"] = mod.description;
    object["type"] = mod.type;
    object["system"] = mod.system;
    object["format"] = mod.format;
    object["patchUrl"] = mod.patchUrl;
    object["patchSha1"] = mod.patchSha1;
    object["patchSize"] = static_cast<qint64>(mod.patchSize);
    object["baseCrc32"] = mod.baseCrc32;
    object["baseMd5"] = mod.baseMd5;
    object["baseSha1"] = mod.baseSha1;
    object["sourceUrl"] = mod.sourceUrl;
    object["rating"] = mod.rating;
    object["downloads"] = mod.downloads;
    if (!row.matchScope.isEmpty()) {
        object["matchScope"] = row.matchScope;
    }
    return object;
}

QJsonObject installedModToJson(const Remus::Database::ModInstallationRecord &record,
                              const QString                                &baseFilename)
{
    QJsonObject object;
    object["id"] = record.id;
    object["baseFileId"] = record.baseFileId;
    object["patchedFileId"] = record.patchedFileId;
    object["catalogModId"] = record.catalogModId;
    object["modTitle"] = record.modTitle;
    object["modAuthor"] = record.modAuthor;
    object["modVersion"] = record.modVersion;
    object["modType"] = record.modType;
    object["patchFormat"] = record.patchFormat;
    object["patchUrl"] = record.patchUrl;
    object["patchSha1"] = record.patchSha1;
    object["sourceUrl"] = record.sourceUrl;
    object["installedAt"] = record.installedAt.toString(Qt::ISODate);
    object["baseFilename"] = baseFilename;
    return object;
}

QJsonObject systemCountToJson(const QString &system, int count)
{
    QJsonObject object;
    object["system"] = system;
    object["count"] = count;
    return object;
}

void printJsonArray(const QJsonArray &array)
{
    QTextStream stream(stdout);
    stream << QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Indented)).trimmed() << Qt::endl;
}

void printJsonObject(const QJsonObject &object)
{
    QTextStream stream(stdout);
    stream << QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented)).trimmed() << Qt::endl;
}

void printModList(const QList<ListedMod> &mods)
{
    bool showMatchScope = false;
    for (const auto &row : mods) {
        if (!row.matchScope.isEmpty()) {
            showMatchScope = true;
            break;
        }
    }

    if (showMatchScope) {
        qInfo().noquote() << QString("%1  %2  %3  %4  %5  %6")
            .arg("Scope", -8)
            .arg("ID", -20)
            .arg("Title", -40)
            .arg("Type", -14)
            .arg("Format", -8)
            .arg("Rating");
    } else {
        qInfo().noquote() << QString("%1  %2  %3  %4  %5")
            .arg("ID", -20)
            .arg("Title", -40)
            .arg("Type", -14)
            .arg("Format", -8)
            .arg("Rating");
    }

    for (const auto &row : mods) {
        const auto &mod = row.mod;
        if (showMatchScope) {
            qInfo().noquote() << QString("%1  %2  %3  %4  %5  %6")
                .arg(row.matchScope.left(8), -8)
                .arg(mod.id.left(20), -20)
                .arg(mod.title.left(40), -40)
                .arg(mod.type.left(14), -14)
                .arg(mod.format.left(8), -8)
                .arg(QString::number(mod.rating, 'f', 1));
        } else {
            qInfo().noquote() << QString("%1  %2  %3  %4  %5")
                .arg(mod.id.left(20), -20)
                .arg(mod.title.left(40), -40)
                .arg(mod.type.left(14), -14)
                .arg(mod.format.left(8), -8)
                .arg(QString::number(mod.rating, 'f', 1));
        }
    }
}

void printModDetails(const Remus::ModEntry &mod)
{
    qInfo().noquote() << QString("ID:          %1").arg(mod.id);
    qInfo().noquote() << QString("Title:       %1").arg(mod.title);
    qInfo().noquote() << QString("Author:      %1").arg(mod.author);
    qInfo().noquote() << QString("Version:     %1").arg(mod.version);
    qInfo().noquote() << QString("Type:        %1").arg(mod.type);
    qInfo().noquote() << QString("System:      %1").arg(mod.system);
    qInfo().noquote() << QString("Format:      %1").arg(mod.format);
    qInfo().noquote() << QString("Patch URL:   %1").arg(mod.patchUrl);
    qInfo().noquote() << QString("Patch SHA1:  %1").arg(mod.patchSha1.isEmpty() ? "(none)" : mod.patchSha1);
    qInfo().noquote() << QString("Downloads:   %1").arg(mod.downloads);
    qInfo().noquote() << QString("Rating:      %1").arg(QString::number(mod.rating, 'f', 1));
    qInfo().noquote() << QString("Source URL:  %1").arg(mod.sourceUrl.isEmpty() ? "(none)" : mod.sourceUrl);
    if (!mod.description.isEmpty()) {
        qInfo().noquote() << QString("Description: %1").arg(mod.description);
    }
}

bool loadModCatalog(CliContext &ctx, Remus::ModCatalogProvider &catalog, QString &error)
{
    const bool hasCatalog = ctx.parser.isSet("mod-catalog");
    const bool hasCatalogUrl = ctx.parser.isSet("mod-catalog-url");

    if (!hasCatalog && !hasCatalogUrl) {
        error = QStringLiteral("Mod commands require --mod-catalog <path> or --mod-catalog-url <url>");
        return false;
    }

    if (hasCatalog) {
        if (!catalog.loadFromFile(ctx.parser.value("mod-catalog"))) {
            error = QStringLiteral("Failed to load mod catalog: %1").arg(catalog.lastError());
            return false;
        }
        return true;
    }

    const QUrl url(ctx.parser.value("mod-catalog-url"));
    if (!url.isValid() || url.scheme().isEmpty()) {
        error = QStringLiteral("Invalid URL for --mod-catalog-url");
        return false;
    }

    const bool forceRefresh = ctx.parser.isSet("mod-catalog-refresh");
    if (!catalog.loadFromUrl(url, forceRefresh)) {
        error = QStringLiteral("Failed to load mod catalog: %1").arg(catalog.lastError());
        return false;
    }

    Remus::Database::ModCatalogCacheRecord cacheRec;
    cacheRec.sourceUrl = url.toString();
    cacheRec.modCount = catalog.allMods().size();
    ctx.db.upsertCatalogCache(cacheRec);
    return true;
}