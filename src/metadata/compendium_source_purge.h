#pragma once

#include <QSqlDatabase>
#include <QString>

namespace Remus {
namespace Compendium {

    /// Remove ingest rows for @p sourceId so a changed DAT can be re-ingested cleanly.
    /// Does not delete @c games rows (post-ingest dedup handles orphans where possible).
    bool purgeSourceIngestData(QSqlDatabase &db, const QString &sourceId, QString &error);

} // namespace Compendium
} // namespace Remus
