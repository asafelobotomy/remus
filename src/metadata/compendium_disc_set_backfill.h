#pragma once

#include "compendium_types.h"

#include <QSqlDatabase>
#include <QString>

namespace Remus {
namespace Compendium {

    class DiscSetBackfill {
    public:
        /// Rebuild disc topology from persisted @c source_items + @c game_signatures rows.
        /// Idempotent when topology already exists unless @p clearExisting is true.
        static bool backfillDiscSets(
            QSqlDatabase &db, bool clearExisting, CompilerStats &stats, QString &error);
    };

} // namespace Compendium
} // namespace Remus
