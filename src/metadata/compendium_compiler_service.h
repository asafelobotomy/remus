#pragma once
// Phase 1 compendium compiler: service layer.
// Orchestrates extraction → normalization → identity linking → persistence →
// merge resolution for one compendium build invocation.

#include "compendium_types.h"

#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <functional>

namespace Remus {
namespace Compendium {

    // Describes one source to process (mirrors the manifest descriptor).
    struct CompendiumSourceConfig {
        QString sourceId;
        QString displayName;
        QString sourceType; // "dat", "libretro_metadata", …
        QString snapshotId;
        QString filePath;
        int priority;
        bool enabled;
        QString licenseId;
        QString licenseUrl;
        bool attributionRequired;
    };

    // Full build configuration (built from the manifest by the CLI adapter).
    struct CompendiumBuildConfig {
        QString buildId;
        int schemaVersion;
        QString manifestJson; // raw JSON string for storage in compendium_builds
        QList<CompendiumSourceConfig> sources;
    };

    // Progress callback fired after each source is fully processed.
    // current: 1-based index of the source just completed.
    // total:   number of enabled sources in this build.
    using ProgressCallback
        = std::function<void(int current, int total, const QString &sourceId, const CompilerStats &stats)>;

    class CompendiumCompilerService {
    public:
        // Run a full compendium build.  The database must already have the schema
        // applied (migration + seeds) and the sources/source_snapshots rows
        // pre-inserted by the CLI adapter before calling run().
        //
        // Returns populated stats on success; sets error and returns {} on failure.
        // onProgress (optional) is called after each enabled source is processed.
        CompilerStats run(const CompendiumBuildConfig &config, QSqlDatabase &db, QString &error,
            ProgressCallback onProgress = nullptr);
    };

    // Merge game rows that share the same (system_id, canonical_title), keeping
    // the row with the most signatures and reassigning all child rows to the winner.
    // Called automatically by CompendiumCompilerService::run() and also exposed
    // here for incremental-ingest callers that run their own pipeline.
    // Returns the number of merged rows, or -1 on fatal DB error.
    int deduplicateGames(QSqlDatabase &db, QString &error);

} // namespace Compendium
} // namespace Remus
