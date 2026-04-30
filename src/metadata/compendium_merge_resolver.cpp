#include "compendium_merge_resolver.h"

#include <QDebug>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>

namespace Remus {
namespace Compendium {

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool execQuery(QSqlQuery &q, QString &error)
{
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    return true;
}

// ── Merge resolver implementation ─────────────────────────────────────────────
//
// For each (game_id, field_name) group in game_facts the highest-priority
// candidate wins (source_priority DESC, confidence DESC, fact_id ASC tiebreak).
// Single-source groups are labelled "single_source"; multi-source "highest_priority".
//
// A single SQL window-function INSERT replaces the old per-group C++ loop,
// running entirely in SQLite's native C without loading rows into Qt memory.
// Requires SQLite ≥ 3.25 (window functions); Qt 6 bundles SQLite ≥ 3.39.
//
// Non-default merge_policy rules (most_common, manual, highest_confidence)
// are not yet implemented — they fall back to highest_priority and emit a
// qWarning. The merge_policy table is currently unseeded, so this does not
// affect current builds.

bool MergeResolver::resolve(QSqlDatabase &db,
                             CompilerStats &stats,
                             QString &error) const
{
    // ── 1. Load merge policy ──────────────────────────────────────────────────
    QHash<QString, QStringList> policyMap;
    {
        QSqlQuery qPolicy(db);
        qPolicy.prepare(QStringLiteral(
            "SELECT field_name, rule_key FROM merge_policy "
            "WHERE active = 1 ORDER BY field_name, rule_order"));
        if (!execQuery(qPolicy, error)) return false;
        while (qPolicy.next())
            policyMap[qPolicy.value(0).toString()] << qPolicy.value(1).toString();
    }

    // Warn about non-default policies — SQL path only supports highest_priority.
    for (auto it = policyMap.constBegin(); it != policyMap.constEnd(); ++it) {
        const QStringList &rules = it.value();
        if (!rules.isEmpty() && rules.first() != QLatin1String("highest_priority")) {
            qWarning().noquote()
                << QStringLiteral("[MergeResolver] Field '%1' uses policy '%2'"
                                  " — non-default policies fall back to highest_priority.")
                       .arg(it.key(), rules.first());
        }
    }

    // ── 2. Resolve all (game_id, field_name) groups with one SQL statement ────
    //
    // ROW_NUMBER() picks the top-priority candidate per group.
    // COUNT() distinguishes single-source vs multi-source groups.
    // OR REPLACE handles incremental re-runs over an existing compendium.
    {
        QSqlQuery qResolve(db);
        const bool ok = qResolve.exec(QStringLiteral(
            "INSERT OR REPLACE INTO canonical_resolution "
            "    (game_id, field_name, selected_fact_id, resolved_by_rule) "
            "SELECT game_id, field_name, fact_id, "
            "       CASE WHEN cnt = 1 THEN 'single_source' ELSE 'highest_priority' END "
            "FROM ("
            "    SELECT game_id, field_name, fact_id, "
            "           COUNT(*) OVER (PARTITION BY game_id, field_name) AS cnt, "
            "           ROW_NUMBER() OVER ("
            "               PARTITION BY game_id, field_name "
            "               ORDER BY source_priority DESC, confidence DESC, fact_id ASC"
            "           ) AS rn "
            "    FROM game_facts"
            ") WHERE rn = 1"));

        if (!ok) {
            error = QStringLiteral("Merge resolve failed: %1")
                        .arg(qResolve.lastError().text());
            return false;
        }

        const int affected = qResolve.numRowsAffected();
        stats.resolvedFields = (affected >= 0) ? affected : 0;
    }

    return true;
}

} // namespace Compendium
} // namespace Remus
