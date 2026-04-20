#pragma once
// Phase 1 compendium compiler: merge resolver.
// Reads merge_policy rows from the compendium DB and materialises
// canonical_resolution rows (and merge_conflicts where policy is ambiguous).
// Fully data-driven — no hardcoded merge rules in C++.

#include "compendium_types.h"
#include <QSqlDatabase>
#include <QString>

namespace Remus {
namespace Compendium {

class MergeResolver
{
public:
    // Run the full merge pass over all game_facts in the open DB.
    // Updates stats.resolvedFields and stats.unresolvedConflicts.
    // Returns false on fatal DB error.
    bool resolve(QSqlDatabase &db,
                 CompilerStats &stats,
                 QString &error) const;
};

} // namespace Compendium
} // namespace Remus
