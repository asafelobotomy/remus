#include "compendium_merge_resolver.h"

#include <QHash>
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
    // ── 1. Load merge policy ──────────────────────────────────────────────────
    QHash<QString, QStringList> policyMap;
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

    // ── 2. Bulk-load all game_facts once ─────────────────────────────────────
    // Group by "game_id\0field_name" — NUL cannot appear in either field.
    struct Candidate {
        int     factId;
        QString fieldValue;
        int     sourcePriority;
        double  confidence;
    };
    // Preserve insertion order so that within each group candidates remain
    // sorted by (source_priority DESC, confidence DESC) as emitted by the query.
    struct Group {
        QString gameId;
        QString fieldName;
        QList<Candidate> candidates;
    };
    QHash<QString, int> keyToGroupIdx;   // groupKey → index into groups
    QList<Group> groups;

    {
        QSqlQuery qFacts(db);
        qFacts.prepare(QStringLiteral(
            "SELECT fact_id, game_id, field_name, field_value, source_priority, confidence "
            "FROM game_facts "
            "ORDER BY game_id, field_name, source_priority DESC, confidence DESC"));
        if (!execQuery(qFacts, error)) {
            return false;
        }
        while (qFacts.next()) {
            const QString gameId    = qFacts.value(1).toString();
            const QString fieldName = qFacts.value(2).toString();
            const QString groupKey  = gameId + QLatin1Char('\0') + fieldName;
            auto it = keyToGroupIdx.constFind(groupKey);
            if (it == keyToGroupIdx.constEnd()) {
                keyToGroupIdx.insert(groupKey, groups.size());
                groups.append({gameId, fieldName, {}});
                it = keyToGroupIdx.constFind(groupKey);
            }
            groups[it.value()].candidates.append({
                qFacts.value(0).toInt(),
                qFacts.value(3).toString(),
                qFacts.value(4).toInt(),
                qFacts.value(5).toDouble(),
            });
        }
    }

    // ── 3. Prepare insert statements once ────────────────────────────────────
    QSqlQuery qResolved(db);
    qResolved.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO canonical_resolution "
        "(game_id, field_name, selected_fact_id, resolved_by_rule) "
        "VALUES (?, ?, ?, ?)"));

    QSqlQuery qConflict(db);
    qConflict.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO merge_conflicts "
        "(game_id, field_name, fact_ids_json, resolution_status) "
        "VALUES (?, ?, ?, 'unresolved')"));

    // ── 4. Resolve each group in memory ──────────────────────────────────────
    for (const Group &grp : std::as_const(groups)) {
        const QList<Candidate> &candidates = grp.candidates;
        if (candidates.isEmpty()) {
            continue;
        }

        if (candidates.size() == 1) {
            // Single source — no conflict possible.
            qResolved.addBindValue(grp.gameId);
            qResolved.addBindValue(grp.fieldName);
            qResolved.addBindValue(candidates.first().factId);
            qResolved.addBindValue(QStringLiteral("single_source"));
            if (!execQuery(qResolved, error)) {
                return false;
            }
            ++stats.resolvedFields;
            continue;
        }

        const QStringList &rules = policyMap.value(
            grp.fieldName, QStringList{QStringLiteral("highest_priority")});
        QString appliedRule;
        int     selectedFactId = -1;

        for (const QString &rule : rules) {
            if (rule == QStringLiteral("highest_priority")) {
                // Sorted by source_priority DESC already.
                selectedFactId = candidates.first().factId;
                appliedRule    = rule;
                break;
            }
            if (rule == QStringLiteral("highest_confidence")) {
                double best = -1.0;
                for (const Candidate &c : candidates) {
                    if (c.confidence > best) {
                        best           = c.confidence;
                        selectedFactId = c.factId;
                    }
                }
                appliedRule = rule;
                break;
            }
            if (rule == QStringLiteral("most_common")) {
                QHash<QString, int> freq;
                for (const Candidate &c : candidates) {
                    ++freq[c.fieldValue];
                }
                int bestCount = 0;
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
                    for (const Candidate &c : candidates) {
                        if (c.fieldValue == leaders.first()) {
                            selectedFactId = c.factId;
                            break;
                        }
                    }
                    appliedRule = rule;
                    break;
                }
                // Tie — fall through to next rule.
            }
            if (rule == QStringLiteral("manual")) {
                break;
            }
        }

        if (selectedFactId > 0) {
            qResolved.addBindValue(grp.gameId);
            qResolved.addBindValue(grp.fieldName);
            qResolved.addBindValue(selectedFactId);
            qResolved.addBindValue(appliedRule);
            if (!execQuery(qResolved, error)) {
                return false;
            }
            ++stats.resolvedFields;
        } else {
            QJsonArray factIdsArr;
            for (const Candidate &c : candidates) {
                factIdsArr.append(c.factId);
            }
            const QString factIdsJson = QString::fromUtf8(
                QJsonDocument(factIdsArr).toJson(QJsonDocument::Compact));
            qConflict.addBindValue(grp.gameId);
            qConflict.addBindValue(grp.fieldName);
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
