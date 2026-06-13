#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "database.h"

namespace Remus {

struct LibraryExportRow {
    FileRecord file;
    Database::MatchResult match;
};

class LibraryExporter {
public:
    static QList<LibraryExportRow> buildRows(Database &db, const QStringList &systemFilters = {});

    static QString defaultFilename(const QString &format);
    static QString resolveOutputPath(const QString &format, const QString &outputPath);

    static bool exportToFile(
        Database &db, const QString &format, const QString &outputPath, const QStringList &systemFilters = {},
        QString *error = nullptr);
};

} // namespace Remus
