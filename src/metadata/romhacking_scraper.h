#pragma once

#include <QList>
#include <QString>

#include "../services/mod_catalog_provider.h"

namespace Remus {

class RomhackingScraper {
public:
    struct SearchOptions {
        QString query;
        QString system;
        int maxResults = 50;
    };

    QList<ModEntry> search(const SearchOptions &options, QString *error = nullptr);

    bool writeCatalogJson(const QList<ModEntry> &mods, const QString &outputPath, QString *error = nullptr);

private:
    static QString modTypeFromUrl(const QString &url);
    static QString slugId(const QString &title, const QString &url);
};

} // namespace Remus
