#pragma once
// Phase 1 compendium compiler: service layer.
// Orchestrates extraction → normalization → identity linking → persistence →
// merge resolution for one compendium build invocation.

#include "compendium_types.h"

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

namespace Remus {
namespace Compendium {

// Describes one source to process (mirrors the manifest descriptor).
struct CompendiumSourceConfig {
    QString sourceId;
    QString displayName;
    QString sourceType;   // "dat", "libretro_metadata", …
    QString snapshotId;
    QString filePath;
    int     priority;
    bool    enabled;
    QString licenseId;
    QString licenseUrl;
    bool    attributionRequired;
};

// Full build configuration (built from the manifest by the CLI adapter).
struct CompendiumBuildConfig {
    QString buildId;
    int     schemaVersion;
    QString manifestJson;  // raw JSON string for storage in compendium_builds
    QList<CompendiumSourceConfig> sources;
};

class CompendiumCompilerService
{
public:
    // Run a full compendium build.  The database must already have the schema
    // applied (migration + seeds) and the sources/source_snapshots rows
    // pre-inserted by the CLI adapter before calling run().
    //
    // Returns populated stats on success; sets error and returns {} on failure.
    CompilerStats run(const CompendiumBuildConfig &config,
                      QSqlDatabase &db,
                      QString &error);
};

} // namespace Compendium
} // namespace Remus
