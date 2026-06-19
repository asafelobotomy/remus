#include "compendium_source_purge.h"

#include <QSqlError>
#include <QSqlQuery>

namespace Remus {
namespace Compendium {

    namespace {

        bool execBoundDelete(
            QSqlDatabase &db, const QString &sql, const QString &sourceId, QString &error, const char *label) {
            QSqlQuery q(db);
            if (!q.prepare(sql)) {
                error = QStringLiteral("%1 prepare: %2").arg(QLatin1String(label), q.lastError().text());
                return false;
            }
            q.addBindValue(sourceId);
            if (!q.exec()) {
                error = QStringLiteral("%1: %2").arg(QLatin1String(label), q.lastError().text());
                return false;
            }
            return true;
        }

    } // namespace

    bool purgeSourceIngestData(QSqlDatabase &db, const QString &sourceId, QString &error) {
        if (sourceId.isEmpty()) {
            error = QStringLiteral("purgeSourceIngestData: empty source_id");
            return false;
        }

        if (!execBoundDelete(db,
                QStringLiteral("DELETE FROM game_disc_tracks WHERE disc_set_id IN "
                               "(SELECT disc_set_id FROM game_disc_sets WHERE source_id = ?)"),
                sourceId, error, "purge disc tracks")) {
            return false;
        }

        if (!execBoundDelete(db, QStringLiteral("DELETE FROM game_disc_sets WHERE source_id = ?"), sourceId, error,
                "purge disc sets")) {
            return false;
        }

        if (!execBoundDelete(db,
                QStringLiteral("DELETE FROM game_disc_tracks WHERE signature_id IN "
                               "(SELECT signature_id FROM game_signatures WHERE source_id = ?)"),
                sourceId, error, "purge disc tracks by signature")) {
            return false;
        }

        if (!execBoundDelete(db,
                QStringLiteral("DELETE FROM canonical_resolution WHERE selected_fact_id IN "
                               "(SELECT fact_id FROM game_facts WHERE source_id = ?)"),
                sourceId, error, "purge canonical resolution rows")) {
            return false;
        }

        QSqlQuery clearConflictQ(db);
        clearConflictQ.prepare(QStringLiteral("UPDATE merge_conflicts SET chosen_fact_id = NULL "
                                              "WHERE chosen_fact_id IN "
                                              "(SELECT fact_id FROM game_facts WHERE source_id = ?)"));
        clearConflictQ.addBindValue(sourceId);
        if (!clearConflictQ.exec()) {
            error = QStringLiteral("purge merge conflict fact refs: %1").arg(clearConflictQ.lastError().text());
            return false;
        }

        const QStringList deleteSql = {
            QStringLiteral("DELETE FROM game_facts WHERE source_id = ?"),
            QStringLiteral("DELETE FROM game_signatures WHERE source_id = ?"),
            QStringLiteral("DELETE FROM game_serials WHERE source_id = ?"),
            QStringLiteral("DELETE FROM game_names WHERE source_id = ?"),
            QStringLiteral("DELETE FROM source_items WHERE source_id = ?"),
        };
        for (const QString &sql : deleteSql) {
            if (!execBoundDelete(db, sql, sourceId, error, "purge source ingest rows")) {
                return false;
            }
        }

        return true;
    }

} // namespace Compendium
} // namespace Remus
