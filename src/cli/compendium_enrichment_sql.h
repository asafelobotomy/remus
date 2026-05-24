#pragma once

#include <QString>
#include <QVariant>

class QSqlDatabase;
class QSqlQuery;

/**
 * @file compendium_enrichment_sql.h
 * @brief Shared SQL helpers for compendium enrichment passes.
 *
 * Centralises the boilerplate that every enricher needs:
 *  - execPrepared          — execute a prepared query with consistent error reporting
 *  - upsertEnrichmentSource — INSERT OR IGNORE for sources + source_snapshots
 *  - normalizeMetadataTitle — fuzzy-match title normalisation (shared across providers)
 *
 * Include this header instead of re-implementing these in each enricher's
 * anonymous namespace.
 */

namespace CompendiumEnrichmentSql {

struct SourceSpec {
    QString sourceId;
    QString displayName;
    QString sourceType;
    QString licenseUrl;
    bool attributionRequired = false;
    int priority = 0;
    QString licenseId;
};

struct SnapshotSpec {
    QString snapshotId;
    QString snapshotLabel;
};

struct FactInsertSpec {
    QString sourceId;
    QString snapshotId;
    int sourcePriority = 0;
    double confidence = 0.0;
};

/**
 * @brief Execute a prepared QSqlQuery and populate @p error on failure.
 */
bool execPrepared(QSqlQuery &query, QString &error, const QString &context);

/**
 * @brief INSERT OR IGNORE a source row and its corresponding snapshot row.
 *
 * Safe to call multiple times; duplicate rows are silently ignored.
 *
 * @param db                  Open SQLite connection (inside a transaction).
 * @param sourceId            sources.source_id value (e.g. "igdb").
 * @param displayName         sources.display_name value (e.g. "IGDB").
 * @param sourceType          "online-api" or "static-file".
 * @param licenseUrl          sources.license_url value (may be empty → NULL).
 * @param attributionRequired true → 1, false → 0.
 * @param priority            sources.priority value.
 * @param snapshotId          source_snapshots.snapshot_id value.
 * @param snapshotLabel       source_snapshots.snapshot_label value.
 * @param error               [out] Human-readable error message on failure.
 * @return true on success, false on error.
 */
bool upsertEnrichmentSource(QSqlDatabase &db,
                            const SourceSpec &source,
                            const SnapshotSpec &snapshot,
                            QString &error);

/**
 * @brief Insert a single game_facts row using the shared enrichment metadata.
 *
 * Empty @p fieldValue is treated as a no-op and returns true.
 * When @p inserted is provided it is set to true if a new row was inserted.
 */
bool insertGameFact(QSqlQuery &factQuery,
                    const FactInsertSpec &spec,
                    const QString &gameId,
                    const QString &fieldName,
                    const QString &fieldValue,
                    const QString &valueType,
                    QString &error,
                    const QString &contextPrefix,
                    bool *inserted = nullptr);

/**
 * @brief Helpers for binding nullable values in UPDATE statements.
 */
QVariant nullableText(const QString &value);
QVariant nullableInt(int value);
QVariant nullableDouble(double value);

/**
 * @brief Normalise a game title for fuzzy provider matching.
 *
 * Strips a trailing parenthetical suffix (e.g. "(USA)", "(128K)"),
 * lowercases, removes leading articles (the/a/an), and keeps only
 * letters and digits. Examples:
 *   "Super Mario 64 (USA)"     → "supermario64"
 *   "The Legend of Zelda (EU)" → "legendofzelda"
 *   "F-Zero (GX) (Japan)"      → "fzerogx"
 */
QString normalizeMetadataTitle(const QString &title);

} // namespace CompendiumEnrichmentSql
