#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace Remus {

enum class CompendiumBuildPreset {
    Default,
    OfflineOnly,
    StrictOffline,
    DeveloperFull,
};

struct CompendiumFullBuildOptions {
    bool skipDatUpdate = false;
    bool offlineOnly = false;
    bool strictOffline = false;
    bool forceFullRebuild = false;
    bool onlineEnrichmentAll = false;
    bool recover = false;
    bool forceEnrichment = false;
    bool allowUnresolvedConflicts = false;
    bool skipValidation = false;
    bool pruneAcquisition = false;
    bool thumbnailSnapLossless = false;
    bool skipConsolidate = false;
    bool detached = true;
    QString outputDbPath;
};

struct CompendiumExtendBuildOptions {
    QStringList enrichSources;
    bool forceEnrichment = false;
    bool offlineOnly = false;
    bool onlineEnrichmentAll = false;
    bool consolidateArtwork = false;
    bool detached = true;
    QString outputDbPath;
};

CompendiumFullBuildOptions applyBuildPreset(CompendiumBuildPreset preset);
void normalizeFullBuildOptions(CompendiumFullBuildOptions &options);
void normalizeExtendBuildOptions(CompendiumExtendBuildOptions &options);
QStringList fullBuildFlagArgs(const QString &outputDb, const CompendiumFullBuildOptions &options);
QStringList extendBuildCommandArgs(const QString &repoRoot, const QString &cliBinary, const QString &outputDb,
    const CompendiumExtendBuildOptions &options);

} // namespace Remus
