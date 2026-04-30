#pragma once
// Phase 1 compendium compiler: fact inserter.
// Persists linked SourceRecordEnvelopes to the compendium SQLite database:
//   games, game_names, game_signatures, game_serials, source_items, game_facts.

#include "compendium_types.h"

#include <QList>
#include <QSqlDatabase>
#include <QSqlQuery>

namespace Remus {
namespace Compendium {

class FactInserter
{
public:
    // Insert all linked records into the open compendium database.
    // All records must belong to the same source (single-source batch).
    // stats is updated in-place; error is set on fatal failure.
    // Returns false on fatal DB error.
    bool insert(const QList<SourceRecordEnvelope> &records,
                QSqlDatabase &db,
                CompilerStats &stats,
                QString &error) const;

private:
    // Helpers accept pre-prepared queries to avoid sqlite3_prepare_v2() per record.
    bool ensureGame(const SourceRecordEnvelope &rec,
                    QSqlQuery &qGame, QSqlQuery &qName,
                    CompilerStats &stats, QString &error) const;

    bool insertSourceItem(const SourceRecordEnvelope &rec,
                          QSqlQuery &q,
                          CompilerStats &stats, QString &error) const;

    bool insertSignatures(const SourceRecordEnvelope &rec,
                          QSqlQuery &q,
                          CompilerStats &stats, QString &error) const;

    bool insertSerials(const SourceRecordEnvelope &rec,
                       QSqlQuery &q,
                       CompilerStats &stats, QString &error) const;

    bool insertFacts(const SourceRecordEnvelope &rec,
                     QSqlQuery &q,
                     CompilerStats &stats, QString &error,
                     int sourcePriority) const;
};

} // namespace Compendium
} // namespace Remus
