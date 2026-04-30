#include "compendium_compiler_service.h"

#include "compendium_dat_extractor.h"
#include "compendium_normalizer.h"
#include "compendium_identity_linker.h"
#include "compendium_fact_inserter.h"
#include "compendium_merge_resolver.h"

#include <QDebug>

namespace Remus {
namespace Compendium {

CompilerStats CompendiumCompilerService::run(const CompendiumBuildConfig &config,
                                              QSqlDatabase &db,
                                              QString &error,
                                              ProgressCallback onProgress)
{
    CompilerStats stats;
    const CompendiumNormalizer normalizer;
    const IdentityLinker       linker;
    const FactInserter         inserter;
    const MergeResolver        resolver;

    // Count enabled sources once for accurate progress reporting.
    int totalEnabled = 0;
    for (const auto &s : config.sources) { if (s.enabled) ++totalEnabled; }
    int processed = 0;

    for (const CompendiumSourceConfig &src : config.sources) {
        if (!src.enabled) {
            ++stats.skippedDisabled;
            continue;
        }

        if (src.sourceType != QStringLiteral("dat")) {
            qWarning() << "[CompendiumCompilerService] Unsupported source type:"
                       << src.sourceType << "— skipped";
            continue;
        }

        // ── Extract ───────────────────────────────────────────────────────────
        QString extractError;
        QList<SourceRecordEnvelope> records = DatExtractor::extract(
            src.filePath, src.sourceId, src.snapshotId, extractError);

        if (records.isEmpty()) {
            if (!extractError.isEmpty()) {
                qWarning() << "[CompendiumCompilerService] Extraction failed for"
                           << src.sourceId << ":" << extractError;
            }
            continue;
        }

        // ── Normalize ─────────────────────────────────────────────────────────
        for (SourceRecordEnvelope &rec : records) {
            normalizer.normalize(rec);
        }

        // ── Link identities ───────────────────────────────────────────────────
        linker.link(records);

        // ── Persist ───────────────────────────────────────────────────────────
        if (!inserter.insert(records, db, stats, error)) {
            return stats; // error is set
        }

        ++processed;
        if (onProgress) {
            onProgress(processed, totalEnabled, src.sourceId, stats);
        }
    }

    // ── Merge resolution (single pass over all accumulated facts) ─────────────
    if (!resolver.resolve(db, stats, error)) {
        return stats;
    }

    return stats;
}

} // namespace Compendium
} // namespace Remus
