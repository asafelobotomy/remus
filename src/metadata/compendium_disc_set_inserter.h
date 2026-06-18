#pragma once

#include "compendium_types.h"

#include <QHash>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

namespace Remus {
namespace Compendium {

    class DiscSetInserter {
    public:
        /// Effective @c disc_count per canonical @c set_key for a normalized batch.
        static QHash<QString, int> effectiveDiscCounts(const QList<SourceRecordEnvelope> &records);

        /// Insert or fetch @c game_disc_sets row for a DAT game block.
        static qint64 ensureDiscSet(const SourceRecordEnvelope &blockRepresentative, int effectiveDiscCount,
            qint64 sourceItemId, QSqlDatabase &db, CompilerStats &stats, QString &error);

        /// Resolve the primary signature row for a persisted track envelope.
        static qint64 lookupPrimarySignatureId(
            const SourceRecordEnvelope &rec, QSqlDatabase &db, QString &error);

        /// Insert one @c game_disc_tracks row linked to @p discSetId .
        static bool insertTrack(const SourceRecordEnvelope &rec, qint64 discSetId, qint64 signatureId, QSqlQuery &q,
            CompilerStats &stats, QString &error);

        /// Insert disc sets and tracks for a normalized, linked DAT batch.
        static bool insertDiscTopologyForRecords(
            const QList<SourceRecordEnvelope> &records, QSqlDatabase &db, CompilerStats &stats, QString &error);
    };

} // namespace Compendium
} // namespace Remus
