#include "compendium_merge_resolver.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QJsonArray>
#include <QJsonDocument>
#include <QVariant>

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
// Strategy (data-driven from merge_policy table):
//   For each (game_id, field_name) group in game_facts:
//   1. Collect all candidate fact_ids with their source_priority and confidence.
//   2. Look up merge_policy rows for this field_name, ordered by rule_order.
//   3. Apply the first matching rule:
//      - "highest_priority"  → pick the fact from the highest source_priority
//      - "highest_confidence"→ pick the fact with the highest confidence value
//      - "most_common"       → pick the fact value that appears most frequently
//      - "manual"            → record as conflict (requires human resolution)
//   4. Insert a canonical_resolution row for resolved fields.
//   5. Insert a merge_conflicts row for fields that cannot be auto-resolved.

bool MergeResolver::resolve(QSqlDatabase &db,
                             CompilerStats &stats,
                             QString &error) const
{
    // Load the merge policy into memory for fast per-field lookup.
    // Map: field_name → ordered list of rule_key strings
    QMap<QString, QStringList> policyMap;
    {
        QSqlQuery qPolicy(db);
        qPolicy.prepare(QStringLiteral(
            "SELECT field_name, rule_key FROM merge_policy "
            "WHERE active = 1 ORDER BY field_name, rule_order"));
        if (!execQuery(qPolicy, error)) {
            return false;
        }
        while (qPolicy.next()) {
            policyMap[qPolicy.value(0).toString()] << qPolicy.value(1).toString();
        }
    }

    // Collect distinct (game_id, field_name) combinations that have >1 source fact.
    QSqlQuery qGroups(db);
    qGroups.prepare(QStringLiteral(
        "SELECT game_id, field_name "
        "FROM game_facts "
        "GROUP BY game_id, field_name "
        "HAVING COUNT(DISTINCT source_id) > 0"));
    if (!execQuery(qGroups, error)) {
        return false;
    }

    while (qGroups.next()) {
        const QString gameId    = qGroups.value(0).toString();
        const QString fieldName = qGroups.value(1).toString();

        // Collect candidates for this (game_id, field_name).
        QSqlQuery qCandidates(db);
        qCandidates.prepare(QStringLiteral(
            "SELECT fact_id, field_value, source_priority, confidence "
            "FROM game_facts "
            "WHERE game_id = ? AND field_name = ? "
            "ORDER BY source_priority DESC, confidence DESC"));
        qCandidates.addBindValue(gameId);
        qCandidates.addBindValue(fieldName);
        if (!execQuery(qCandidates, error)) {
            return false;
        }

        struct Candidate {
            int     factId;
            QString fieldValue;
            int     sourcePriority;
            double  confidence;
        };
        QList<Candidate> candidates;
        while (qCandidates.next()) {
            candidates.append({
                qCandidates.value(0).toInt(),
                qCandidates.value(1).toString(),
                qCandidates.value(2).toInt(),
                qCandidates.value(3).toDouble(),
            });
        }

        if (candidates.isEmpty()) {
            continue;
        }
        if (candidates.size() == 1) {
            // Single source — resolve directly, no conflict possible.
            QSqlQuery qRes(db);
            qRes.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO canonical_resolution "
                "(game_id, field_name, selected_fact_id, resolved_by_rule) "
                "VALUES (?, ?, ?, 'single_source')"));
            qRes.addBindValue(gameId);
            qRes.addBindValue(fieldName);
            qRes.addBindValue(candidates.first().factId);
            if (!execQuery(qRes, error)) {
                return false;
            }
            ++stats.resolvedFields;
            continue;
        }

        // Determine the rule to apply for this field.
        const QStringList &rules = policyMap.value(fieldName,
                                                    QStringList{QStringLiteral("highest_priority")});
        QString appliedRule;
        int     selectedFactId = -1;

        for (const QString &rule : rules) {
            if (rule == QStringLiteral("highest_priority")) {
                // Already sorted by source_priority DESC.
                selectedFactId = candidates.first().factId;
                appliedRule    = rule;
                break;
            }
            if (rule == QStringLiteral("highest_confidence")) {
                // Find the highest confidence.
                double best = -1.0;
                for (const Candidate &c : std::as_const(candidates)) {
                    if (c.confidence > best) {
                        best           = c.confidence;
                        selectedFactId = c.factId;
                    }
                }
                appliedRule = rule;
                break;
            }
            if (rule == QStringLiteral("most_common")) {
                // Count occurrences of each field_value.
                QMap<QString, int> freq;
                for (const Candidate &c : std::as_const(candidates)) {
                    ++freq[c.fieldValue];
                }
                int    bestCount = 0;
                QList<QString> leaders;
                for (auto it = freq.constBegin(); it != freq.constEnd(); ++it) {
                    if (it.value() > bestCount) {
                        bestCount = it.value();
                        leaders.clear();
                        leaders << it.key();
                    } else if (it.value() == bestCount) {
                        leaders << it.key();
                    }
                }
                if (leaders.size() == 1) {
                    // Unambiguous majority.
                    for (const Candidate &c : std::as_const(candidates)) {
                        if (c.fieldValue == leaders.first()) {
                            selectedFactId = c.factId;
                            break;
                        }
                    }
                    appliedRule = rule;
                    break;
                }
                // Tie — continue to next rule.
            }
            if (rule == QStringLiteral("manual")) {
                // Fall through to conflict recording.
                break;
            }
        }

        if (selectedFactId > 0) {
            QSqlQuery qRes(db);
            qRes.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO canonical_resolution "
                "(game_id, field_name, selected_fact_id, resolved_by_rule) "
                "VALUES (?, ?, ?, ?)"));
            qRes.addBindValue(gameId);
            qRes.addBindValue(fieldName);
            qRes.addBindValue(selectedFactId);
            qRes.addBindValue(appliedRule);
            if (!execQuery(qRes, error)) {
                return false;
            }
            ++stats.resolvedFields;
        } else {
            // Record as unresolved conflict.
            QJsonArray factIdsArr;
            for (const Candidate &c : std::as_const(candidates)) {
                factIdsArr.append(c.factId);
            }
            const QString factIdsJson = QString::fromUtf8(
                QJsonDocument(factIdsArr).toJson(QJsonDocument::Compact));

            QSqlQuery qConflict(db);
            qConflict.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO merge_conflicts "
                "(game_id, field_name, fact_ids_json, resolution_status) "
                "VALUES (?, ?, ?, 'unresolved')"));
            qConflict.addBindValue(gameId);
            qConflict.addBindValue(fieldName);
            qConflict.addBindValue(factIdsJson);
            if (!execQuery(qConflict, error)) {
                return false;
            }
            if (qConflict.numRowsAffected() > 0) {
                ++stats.unresolvedConflicts;
            }
        }
    }

    return true;
}

} // namespace Compendium
} // namespace Remus
