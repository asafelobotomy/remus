#include "cli_commands.h"
#include "cli_compendium_build_phases.h"
#include "cli_helpers.h"
#include "compendium_sql_utilities.h"

#include "../metadata/compendium_compiler_service.h"
#include "../metadata/compendium_dat_extractor.h"
#include "../metadata/compendium_fact_inserter.h"
#include "../metadata/compendium_identity_linker.h"
#include "../metadata/compendium_merge_resolver.h"
#include "../metadata/compendium_normalizer.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

using namespace CompendiumSqlUtilities;
using namespace Remus;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Compute SHA-256 of a file in 1 MiB chunks. Returns hex string or empty.
static QString fileChecksum(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    while (!f.atEnd()) {
        const QByteArray chunk = f.read(1024 * 1024);
        if (chunk.isEmpty()) break;
        h.addData(chunk);
    }
    return QString::fromLatin1(h.result().toHex());
}

// Derive a sanitised source-id from a DAT base name (no extension).
static QString deriveSourceId(const QString &baseName)
{
    QString name = baseName.toLower();
    static const QRegularExpression reInvalid(QStringLiteral("[^a-z0-9]+"));
    name.replace(reInvalid, QStringLiteral("-"));
    while (name.startsWith(QLatin1Char('-'))) name.remove(0, 1);
    while (name.endsWith(QLatin1Char('-')))   name.chop(1);
    return name.left(64);
}

// ── Command handler ───────────────────────────────────────────────────────────

int handleIngestSourceCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("ingest-source")) return 0;

    const QString datPath    = ctx.parser.value("ingest-source").trimmed();
    const QString outputPath = ctx.parser.value("compendium-output").trimmed();

    const QFileInfo datInfo(datPath);
    if (!datInfo.exists() || !datInfo.isFile()) {
        qCritical() << "✗ DAT file not found:" << datPath;
        return 1;
    }

    const QFileInfo dbInfo(outputPath);
    if (!dbInfo.exists()) {
        qCritical() << "✗ Compendium database not found (run --build-compendium first):"
                    << outputPath;
        return 1;
    }

    const QString sourceId = ctx.parser.isSet("source-id")
                                 ? ctx.parser.value("source-id").trimmed()
                                 : deriveSourceId(datInfo.completeBaseName());
    if (sourceId.isEmpty()) {
        qCritical() << "✗ Could not derive a source-id from:" << datPath;
        return 1;
    }

    const int priority = ctx.parser.value("source-priority").toInt();

    qInfo().noquote() << QStringLiteral("[ingest-source] DAT:       %1").arg(datInfo.absoluteFilePath());
    qInfo().noquote() << QStringLiteral("[ingest-source] DB:        %1").arg(dbInfo.absoluteFilePath());
    qInfo().noquote() << QStringLiteral("[ingest-source] source-id: %1  priority: %2")
                             .arg(sourceId).arg(priority);

    qInfo() << "[ingest-source] Computing DAT checksum...";
    const QString datChecksum = fileChecksum(datInfo.absoluteFilePath());
    if (datChecksum.isEmpty()) {
        qCritical() << "✗ Could not read DAT file:" << datPath;
        return 1;
    }

    const QString connectionName = QStringLiteral("ingest-")
                                   + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(dbInfo.absoluteFilePath());
    if (!database.open()) {
        qCritical() << "✗ Failed to open compendium database:" << database.lastError().text();
        QSqlDatabase::removeDatabase(connectionName);
        return 1;
    }

    // Single cleanup point — closes and removes the DB connection on any early exit.
    const auto cleanup = [&]() {
        database.close();
        QSqlDatabase::removeDatabase(connectionName);
    };

    QSqlQuery pragma(database);
    for (const QString &p : {
             QStringLiteral("PRAGMA foreign_keys = ON"),
             QStringLiteral("PRAGMA journal_mode = WAL"),
             QStringLiteral("PRAGMA synchronous = NORMAL"),
             QStringLiteral("PRAGMA temp_store = MEMORY"),
             QStringLiteral("PRAGMA cache_size = -65536"),
         }) {
        pragma.exec(p);  // non-fatal; WAL already set by --build-compendium
    }

    // Idempotency: skip if this exact DAT (by checksum) was already ingested
    {
        QSqlQuery q(database);
        q.prepare(QStringLiteral(
            "SELECT snapshot_id FROM source_snapshots "
            "WHERE source_id = ? AND checksum_sha256 = ? LIMIT 1"));
        q.addBindValue(sourceId);
        q.addBindValue(datChecksum);
        if (q.exec() && q.next()) {
            qInfo().noquote() << QStringLiteral(
                "[ingest-source] ✔ Already ingested (snapshot: %1) — nothing to do.")
                                     .arg(q.value(0).toString());
            cleanup();
            return 0;
        }
    }

    // Snapshot ID is content-addressable (checksum prefix) for stable re-runs.
    // 16 hex chars = 64 bits of checksum, keeping collision risk negligible even
    // across large DAT libraries from the same source.
    const QString snapshotId = sourceId + QLatin1Char('-') + datChecksum.left(16);

    // Single transaction covering source/snapshot inserts, fact insertion,
    // post-ingest dedup, and merge resolution so the snapshot row is only
    // committed once the compendium is in a fully-integrated state.  This
    // prevents the idempotency guard (checksum lookup above) from treating a
    // partially-integrated ingest as complete on retry.
    // FTS rebuild is excluded: it manages its own transaction and is a derived
    // index that can always be rebuilt idempotently.
    if (!database.transaction()) {
        qCritical() << "✗ Failed to start ingestion transaction:" << database.lastError().text();
        cleanup();
        return 1;
    }

    // Upsert source row (OR IGNORE: may already exist from a prior ingest of the same source)
    {
        QSqlQuery q(database);
        q.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO sources "
            "    (source_id, display_name, source_type, priority, enabled) "
            "VALUES (?, ?, 'dat', ?, 1)"));
        q.addBindValue(sourceId);
        q.addBindValue(datInfo.completeBaseName());
        q.addBindValue(priority);
        QString err;
        if (!execPrepared(q, err, QStringLiteral("Upsert source"))) {
            qCritical() << "✗" << err;
            database.rollback();
            cleanup();
            return 1;
        }
    }

    // Insert snapshot row (new snapshot for this DAT version)
    {
        QSqlQuery q(database);
        q.prepare(QStringLiteral(
            "INSERT INTO source_snapshots "
            "    (snapshot_id, source_id, snapshot_label, fetched_at, checksum_sha256) "
            "VALUES (?, ?, ?, ?, ?)"));
        q.addBindValue(snapshotId);
        q.addBindValue(sourceId);
        q.addBindValue(datInfo.fileName());
        q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        q.addBindValue(datChecksum);
        QString err;
        if (!execPrepared(q, err, QStringLiteral("Insert snapshot"))) {
            qCritical() << "✗" << err;
            database.rollback();
            cleanup();
            return 1;
        }
    }

    // Bootstrap identity linker from existing DB so new records link to
    // games already in the compendium instead of minting fresh IDs.
    qInfo() << "[ingest-source] Bootstrapping identity linker...";
    Remus::Compendium::IdentityLinker linker;
    {
        QString linkErr;
        if (!linker.loadFromDatabase(database, linkErr)) {
            qCritical() << "✗ Failed to bootstrap identity linker:" << linkErr;
            database.rollback();
            cleanup();
            return 1;
        }
    }

    // Extract DAT
    qInfo() << "[ingest-source] Extracting DAT records...";
    QString extractError;
    QList<Remus::Compendium::SourceRecordEnvelope> records =
        Remus::Compendium::DatExtractor::extract(
            datInfo.absoluteFilePath(), sourceId, snapshotId, extractError);
    if (records.isEmpty()) {
        qCritical() << "✗ Extraction produced no records:" << extractError;
        database.rollback();
        cleanup();
        return 1;
    }
    qInfo().noquote() << QStringLiteral("[ingest-source] Extracted %1 records.").arg(records.size());

    // Normalize
    const Remus::Compendium::CompendiumNormalizer normalizer;
    for (Remus::Compendium::SourceRecordEnvelope &rec : records) {
        normalizer.normalize(rec);
    }

    // Link identities (new records link to existing games where hashes/serials/titles match)
    const int newGames = linker.link(records);
    qInfo().noquote()
        << QStringLiteral("[ingest-source] Linked: %1 new games, %2 merged to existing.")
               .arg(newGames).arg(records.size() - newGames);

    // Persist linked records
    Remus::Compendium::CompilerStats stats;
    QString insertError;
    const Remus::Compendium::FactInserter inserter;
    if (!inserter.insert(records, database, stats, insertError)) {
        database.rollback();
        qCritical() << "✗ Fact insertion failed:" << insertError;
        cleanup();
        return 1;
    }
    // Post-ingest dedup — runs inside the main transaction so a failure rolls
    // back all inserted facts and the snapshot row together.
    {
        QString dedupError;
        const int merged = Remus::Compendium::deduplicateGames(database, dedupError);
        if (merged < 0) {
            qCritical() << "✗ Post-ingest dedup failed:" << dedupError;
            database.rollback();
            cleanup();
            return 1;
        }
        stats.deduplicatedGames = merged;
        if (merged > 0) {
            qInfo().noquote()
                << QStringLiteral("[ingest-source] Deduped %1 game rows.").arg(merged);
        }
    }

    // Merge resolution — also inside the main transaction.
    {
        const Remus::Compendium::MergeResolver resolver;
        QString resolveError;
        if (!resolver.resolve(database, stats, resolveError)) {
            qCritical() << "✗ Merge resolution failed:" << resolveError;
            database.rollback();
            cleanup();
            return 1;
        }
    }

    // Commit the main transaction: source row, snapshot row, game facts, dedup,
    // and merge resolution are now an atomic unit.  The idempotency guard
    // (checksum lookup) will only find this snapshot after a clean commit.
    if (!database.commit()) {
        qCritical() << "✗ Failed to commit ingestion transaction:" << database.lastError().text();
        cleanup();
        return 1;
    }

    // Rebuild FTS index (idempotent: clears previous content before repopulating)
    {
        int ftsRowsIndexed = 0;
        QString ftsError;
        if (!populateCompendiumFtsIndex(database, ftsRowsIndexed, ftsError)) {
            qCritical() << "✗ FTS rebuild failed:" << ftsError;
            cleanup();
            return 1;
        }
    }

    qInfo() << "";
    qInfo() << "=== Ingest Source Complete ===";
    qInfo().noquote() << QStringLiteral("Source ID:       %1").arg(sourceId);
    qInfo().noquote() << QStringLiteral("Snapshot ID:     %1").arg(snapshotId);
    qInfo().noquote() << QStringLiteral("Records:         %1").arg(stats.recordsIngested);
    qInfo().noquote() << QStringLiteral("New games:       %1").arg(newGames);
    qInfo().noquote() << QStringLiteral("Merged to exist: %1").arg(records.size() - newGames);
    qInfo().noquote() << QStringLiteral("Signatures:      %1").arg(stats.signaturesCreated);
    qInfo().noquote() << QStringLiteral("Serials:         %1").arg(stats.serialsCreated);
    qInfo().noquote() << QStringLiteral("Facts:           %1").arg(stats.factsCreated);
    qInfo().noquote() << QStringLiteral("Deduped rows:    %1").arg(stats.deduplicatedGames);
    qInfo().noquote() << QStringLiteral("Resolved fields: %1").arg(stats.resolvedFields);

    database.close();
    QSqlDatabase::removeDatabase(connectionName);
    return 0;
}
