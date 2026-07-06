#include "compendium_build_options.h"

namespace Remus {

CompendiumFullBuildOptions applyBuildPreset(CompendiumBuildPreset preset) {
    CompendiumFullBuildOptions options;
    switch (preset) {
    case CompendiumBuildPreset::OfflineOnly:
        options.offlineOnly = true;
        break;
    case CompendiumBuildPreset::StrictOffline:
        options.strictOffline = true;
        options.offlineOnly = true;
        break;
    case CompendiumBuildPreset::DeveloperFull:
        options.forceFullRebuild = true;
        options.onlineEnrichmentAll = true;
        break;
    case CompendiumBuildPreset::Default:
    default:
        break;
    }
    normalizeFullBuildOptions(options);
    return options;
}

void normalizeFullBuildOptions(CompendiumFullBuildOptions &options) {
    if (options.strictOffline) {
        options.offlineOnly = true;
        options.onlineEnrichmentAll = false;
    }
    if (options.offlineOnly) {
        options.onlineEnrichmentAll = false;
    }
    if (options.onlineEnrichmentAll) {
        options.offlineOnly = false;
        options.strictOffline = false;
    }
}

void normalizeExtendBuildOptions(CompendiumExtendBuildOptions &options) {
    if (options.offlineOnly) {
        options.onlineEnrichmentAll = false;
    }
    if (options.onlineEnrichmentAll) {
        options.offlineOnly = false;
    }
}

QStringList fullBuildFlagArgs(const QString &outputDb, const CompendiumFullBuildOptions &options) {
    QStringList args;
    args << QStringLiteral("--output-db") << outputDb;
    if (options.skipDatUpdate) {
        args << QStringLiteral("--skip-update");
    }
    if (options.offlineOnly) {
        args << QStringLiteral("--offline-only");
    }
    if (options.strictOffline) {
        args << QStringLiteral("--strict-offline");
    }
    if (options.forceFullRebuild) {
        args << QStringLiteral("--force-full-rebuild");
    }
    if (options.onlineEnrichmentAll) {
        args << QStringLiteral("--online-enrichment-all");
    }
    if (options.recover) {
        args << QStringLiteral("--recover");
    }
    if (options.forceEnrichment) {
        args << QStringLiteral("--force-enrichment");
    }
    if (options.allowUnresolvedConflicts) {
        args << QStringLiteral("--allow-unresolved-conflicts");
    }
    if (options.skipValidation) {
        args << QStringLiteral("--skip-validation");
    }
    if (options.pruneAcquisition) {
        args << QStringLiteral("--prune-acquisition-sources");
    }
    if (options.thumbnailSnapLossless) {
        args << QStringLiteral("--thumbnail-snap-lossless");
    }
    if (options.skipConsolidate) {
        args << QStringLiteral("--skip-consolidate");
    }
    return args;
}

QStringList extendBuildCommandArgs(const QString &repoRoot, const QString &cliBinary, const QString &outputDb,
    const CompendiumExtendBuildOptions &options) {
    Q_UNUSED(repoRoot)
    QStringList cliArgs;
    cliArgs << cliBinary;
    cliArgs << QStringLiteral("--enrich-compendium");
    cliArgs << QStringLiteral("--compendium-output") << outputDb;
    for (const QString &source : options.enrichSources) {
        const QString trimmed = source.trimmed();
        if (!trimmed.isEmpty()) {
            cliArgs << QStringLiteral("--enrich-source") << trimmed;
        }
    }
    if (options.forceEnrichment) {
        cliArgs << QStringLiteral("--force-enrichment");
    }
    if (options.offlineOnly) {
        cliArgs << QStringLiteral("--offline-only-enrichment");
    }
    if (options.onlineEnrichmentAll) {
        cliArgs << QStringLiteral("--online-enrichment-all");
    }
    return cliArgs;
}

} // namespace Remus
