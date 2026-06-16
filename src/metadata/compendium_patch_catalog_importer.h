#pragma once

#include <QSqlDatabase>
#include <QString>

namespace Remus {
namespace CompendiumPatchCatalog {

    struct ImportStats {
        int sourcesImported = 0;
        int entriesImported = 0;
        int filesSkipped = 0;
    };

    /// Import ClrMamePro patch/hack DAT files from @p patchDir into compendium patch tables.
    /// Resolves libretro-style DAT basenames to systems.internal_name via systems.libretro_name.
    bool importDirectory(QSqlDatabase &database, const QString &patchDir, ImportStats &stats, QString &error);

    bool importDatFile(
        QSqlDatabase &database, const QString &datPath, const QString &catalogName, ImportStats &stats, QString &error);

} // namespace CompendiumPatchCatalog
} // namespace Remus
