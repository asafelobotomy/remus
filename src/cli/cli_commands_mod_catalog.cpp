#include "cli_commands.h"
#include "cli_logging.h"

#include "../services/rapatches_catalog_builder.h"
#include "../services/retroachievements_enricher.h"
#include "../services/mod_catalog_provider.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

int handleModCatalogBuildCommand(CliContext &ctx)
{
    const bool hasBuild   = ctx.parser.isSet("mod-catalog-build");
    const bool hasEnrich  = ctx.parser.isSet("mod-enrich-ra");

    if (!hasBuild && !hasEnrich)
        return 0;

    const bool wantsJson = ctx.parser.isSet("json") || ctx.parser.isSet("mod-json");

    // ── Build from RAPatches ──────────────────────────────────────────────
    if (hasBuild) {
        const QString repoPath = ctx.parser.value("mod-catalog-build");
        if (!QDir(repoPath).exists()) {
            qCritical() << "RAPatches directory does not exist:" << repoPath;
            return 1;
        }

        qInfo() << "Building mod catalog from RAPatches clone:" << repoPath;

        Remus::RAPatchesCatalogBuilder builder;
        auto result = builder.buildFromDirectory(repoPath);

        if (!result.error.isEmpty()) {
            qCritical().noquote() << "Build failed:" << result.error;
            return 1;
        }

        qInfo().noquote() << QString("Scanned %1 files (%2 skipped), found %3 mod entries")
            .arg(result.filesScanned)
            .arg(result.filesSkipped)
            .arg(result.mods.size());

        // Determine output path
        QString outputPath = ctx.parser.value("mod-catalog-output");
        if (outputPath.isEmpty())
            outputPath = QDir::currentPath() + QStringLiteral("/rapatches-catalog.json");

        // Optionally enrich before writing
        if (hasEnrich) {
            Remus::RetroAchievementsEnricher enricher;
            if (ctx.parser.isSet("ra-api-key"))
                enricher.setApiKey(ctx.parser.value("ra-user"),
                                    ctx.parser.value("ra-api-key"));

            if (enricher.hasApiKey()) {
                qInfo() << "Enriching catalog with RetroAchievements data...";
                auto enrichResult = enricher.enrichCatalog(result.mods);
                qInfo().noquote() << QString("Enriched %1 entries (%2 skipped)")
                    .arg(enrichResult.enrichedCount)
                    .arg(enrichResult.skippedCount);
            } else {
                qInfo() << "No RA API key available — skipping enrichment";
            }
        }

        QString writeErr = builder.writeCatalogJson(result.mods, outputPath);
        if (!writeErr.isEmpty()) {
            qCritical().noquote() << "Failed to write catalog:" << writeErr;
            return 1;
        }

        qInfo() << "Catalog written to" << outputPath;
        return 0;
    }

    // ── Enrich existing catalog ───────────────────────────────────────────
    if (hasEnrich) {
        const QString catalogPath = ctx.parser.value("mod-catalog");
        if (catalogPath.isEmpty()) {
            qCritical() << "--mod-enrich-ra requires --mod-catalog <path> to identify the catalog to enrich";
            return 1;
        }

        Remus::ModCatalogProvider catalog;
        if (!catalog.loadFromFile(catalogPath)) {
            qCritical() << "Failed to load catalog:" << catalog.lastError();
            return 1;
        }

        Remus::RetroAchievementsEnricher enricher;
        if (ctx.parser.isSet("ra-api-key"))
            enricher.setApiKey(ctx.parser.value("ra-user"),
                                ctx.parser.value("ra-api-key"));

        if (!enricher.hasApiKey()) {
            qInfo() << "No RA API key available — enrichment skipped";
            qInfo() << "Set REMUS_RA_API_KEY and REMUS_RA_USERNAME env vars, or pass --ra-api-key and --ra-user";
            return 0;
        }

        qInfo() << "Enriching catalog with RetroAchievements data...";
        QList<Remus::ModEntry> mods = catalog.allMods();
        auto enrichResult = enricher.enrichCatalog(mods);

        if (enrichResult.skippedNoApiKey) {
            qInfo() << "Enrichment skipped — no API key";
            return 0;
        }

        qInfo().noquote() << QString("Enriched %1 entries (%2 skipped)")
            .arg(enrichResult.enrichedCount)
            .arg(enrichResult.skippedCount);

        // Write enriched catalog back
        QString outputPath = ctx.parser.value("mod-catalog-output");
        if (outputPath.isEmpty())
            outputPath = catalogPath;  // overwrite in-place

        Remus::RAPatchesCatalogBuilder builder;
        QString writeErr = builder.writeCatalogJson(mods, outputPath);
        if (!writeErr.isEmpty()) {
            qCritical().noquote() << "Failed to write enriched catalog:" << writeErr;
            return 1;
        }

        qInfo() << "Enriched catalog written to" << outputPath;
        return 0;
    }

    return 0;
}
