#include "cli_mod_support.h"

#include <algorithm>

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
    options.jsonOutput = parser.isSet("json") || parser.isSet("mod-json");
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
        if (!options.systemFilter.isEmpty() && mod.system.compare(options.systemFilter, Qt::CaseInsensitive) != 0) {
            continue;
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