#include "cli_commands.h"
#include "cli_mod_support.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QTextStream>
#include <QUrl>

#include "../services/mod_catalog_provider.h"
#include "../services/mod_workflow_service.h"
#include "../services/patch_service.h"
#include "cli_logging.h"

namespace {

void printJsonArray(const QJsonArray &array)
{
    QTextStream stream(stdout);
    stream << QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Indented)).trimmed() << Qt::endl;
}

void printJsonObject(const QJsonObject &object)
{
    QTextStream stream(stdout);
    stream << QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented)).trimmed() << Qt::endl;
}

QJsonObject toJson(const ListedMod &row)
{
    const auto &mod = row.mod;

    QJsonObject object;
    object["id"] = mod.id;
    object["title"] = mod.title;
    object["author"] = mod.author;
    object["version"] = mod.version;
    object["description"] = mod.description;
    object["type"] = mod.type;
    object["system"] = mod.system;
    object["format"] = mod.format;
    object["patchUrl"] = mod.patchUrl;
    object["patchSha1"] = mod.patchSha1;
    object["patchSize"] = static_cast<qint64>(mod.patchSize);
    object["baseCrc32"] = mod.baseCrc32;
    object["baseMd5"] = mod.baseMd5;
    object["baseSha1"] = mod.baseSha1;
    object["sourceUrl"] = mod.sourceUrl;
    object["rating"] = mod.rating;
    object["downloads"] = mod.downloads;
    if (!row.matchScope.isEmpty()) {
        object["matchScope"] = row.matchScope;
    }
    return object;
}

QJsonObject toJson(const Database::ModInstallationRecord &record, const QString &baseFilename)
{
    QJsonObject object;
    object["id"] = record.id;
    object["baseFileId"] = record.baseFileId;
    object["patchedFileId"] = record.patchedFileId;
    object["catalogModId"] = record.catalogModId;
    object["modTitle"] = record.modTitle;
    object["modAuthor"] = record.modAuthor;
    object["modVersion"] = record.modVersion;
    object["modType"] = record.modType;
    object["patchFormat"] = record.patchFormat;
    object["patchUrl"] = record.patchUrl;
    object["patchSha1"] = record.patchSha1;
    object["sourceUrl"] = record.sourceUrl;
    object["installedAt"] = record.installedAt.toString(Qt::ISODate);
    object["baseFilename"] = baseFilename;
    return object;
}

QJsonObject toJson(const QString &system, int count)
{
    QJsonObject object;
    object["system"] = system;
    object["count"] = count;
    return object;
}

void printModList(const QList<ListedMod> &mods)
{
    bool showMatchScope = false;
    for (const auto &row : mods) {
        if (!row.matchScope.isEmpty()) {
            showMatchScope = true;
            break;
        }
    }

    if (showMatchScope) {
        qInfo().noquote() << QString("%1  %2  %3  %4  %5  %6")
            .arg("Scope", -8)
            .arg("ID", -20)
            .arg("Title", -40)
            .arg("Type", -14)
            .arg("Format", -8)
            .arg("Rating");
    } else {
        qInfo().noquote() << QString("%1  %2  %3  %4  %5")
            .arg("ID", -20)
            .arg("Title", -40)
            .arg("Type", -14)
            .arg("Format", -8)
            .arg("Rating");
    }

    for (const auto &row : mods) {
        const auto &mod = row.mod;
        if (showMatchScope) {
            qInfo().noquote() << QString("%1  %2  %3  %4  %5  %6")
                .arg(row.matchScope.left(8), -8)
                .arg(mod.id.left(20), -20)
                .arg(mod.title.left(40), -40)
                .arg(mod.type.left(14), -14)
                .arg(mod.format.left(8), -8)
                .arg(QString::number(mod.rating, 'f', 1));
        } else {
            qInfo().noquote() << QString("%1  %2  %3  %4  %5")
                .arg(mod.id.left(20), -20)
                .arg(mod.title.left(40), -40)
                .arg(mod.type.left(14), -14)
                .arg(mod.format.left(8), -8)
                .arg(QString::number(mod.rating, 'f', 1));
        }
    }
}

void printModDetails(const ModEntry &mod)
{
    qInfo().noquote() << QString("ID:          %1").arg(mod.id);
    qInfo().noquote() << QString("Title:       %1").arg(mod.title);
    qInfo().noquote() << QString("Author:      %1").arg(mod.author);
    qInfo().noquote() << QString("Version:     %1").arg(mod.version);
    qInfo().noquote() << QString("Type:        %1").arg(mod.type);
    qInfo().noquote() << QString("System:      %1").arg(mod.system);
    qInfo().noquote() << QString("Format:      %1").arg(mod.format);
    qInfo().noquote() << QString("Patch URL:   %1").arg(mod.patchUrl);
    qInfo().noquote() << QString("Patch SHA1:  %1").arg(mod.patchSha1.isEmpty() ? "(none)" : mod.patchSha1);
    qInfo().noquote() << QString("Downloads:   %1").arg(mod.downloads);
    qInfo().noquote() << QString("Rating:      %1").arg(QString::number(mod.rating, 'f', 1));
    qInfo().noquote() << QString("Source URL:  %1").arg(mod.sourceUrl.isEmpty() ? "(none)" : mod.sourceUrl);
    if (!mod.description.isEmpty()) {
        qInfo().noquote() << QString("Description: %1").arg(mod.description);
    }
}

bool loadCatalog(CliContext &ctx, ModCatalogProvider &catalog, QString &error)
{
    const bool hasCatalog = ctx.parser.isSet("mod-catalog");
    const bool hasCatalogUrl = ctx.parser.isSet("mod-catalog-url");

    if (!hasCatalog && !hasCatalogUrl) {
        error = QStringLiteral("Mod commands require --mod-catalog <path> or --mod-catalog-url <url>");
        return false;
    }

    if (hasCatalog) {
        if (!catalog.loadFromFile(ctx.parser.value("mod-catalog"))) {
            error = QStringLiteral("Failed to load mod catalog: %1").arg(catalog.lastError());
            return false;
        }
        return true;
    }

    const QUrl url(ctx.parser.value("mod-catalog-url"));
    if (!url.isValid() || url.scheme().isEmpty()) {
        error = QStringLiteral("Invalid URL for --mod-catalog-url");
        return false;
    }

    const bool forceRefresh = ctx.parser.isSet("mod-catalog-refresh");
    if (!catalog.loadFromUrl(url, forceRefresh)) {
        error = QStringLiteral("Failed to load mod catalog: %1").arg(catalog.lastError());
        return false;
    }

    Database::ModCatalogCacheRecord cacheRec;
    cacheRec.sourceUrl = url.toString();
    cacheRec.modCount = catalog.allMods().size();
    ctx.db.upsertCatalogCache(cacheRec);
    return true;
}

} // namespace

int handleModCommands(CliContext &ctx)
{
    const bool hasList         = ctx.parser.isSet("mod-list");
    const bool hasShow         = ctx.parser.isSet("mod-show");
    const bool hasSystems      = ctx.parser.isSet("mod-systems");
    const bool hasCatalogQuery = ctx.parser.isSet("mod-system")
                              || ctx.parser.isSet("mod-author")
                              || ctx.parser.isSet("mod-type")
                              || ctx.parser.isSet("mod-format")
                              || ctx.parser.isSet("mod-source-url")
                              || ctx.parser.isSet("mod-min-rating")
                              || ctx.parser.isSet("mod-min-downloads")
                              || ctx.parser.isSet("mod-sort");
    const bool hasInstall      = ctx.parser.isSet("mod-install");
    const bool hasInstalled    = ctx.parser.isSet("mod-installed");
    const bool hasUninstall    = ctx.parser.isSet("mod-uninstall");
    const bool wantsJson       = ctx.parser.isSet("json") || ctx.parser.isSet("mod-json");

    if (!hasList && !hasShow && !hasSystems && !hasCatalogQuery
        && !hasInstall && !hasInstalled && !hasUninstall) {
        return 0;
    }

    if (hasInstalled) {
        const auto files = ctx.db.getExistingFiles();
        bool found = false;
        QJsonArray results;
        for (const auto &file : files) {
            const auto installs = ctx.db.getModInstallations(file.id);
            for (const auto &inst : installs) {
                if (wantsJson) {
                    results.append(toJson(inst, file.filename));
                    found = true;
                    continue;
                }

                if (!found) {
                    qInfo() << "=== Installed Mods ===";
                    qInfo().noquote() << QString("%1  %2  %3  %4  %5")
                        .arg("ID", -6)
                        .arg("Title", -30)
                        .arg("Type", -14)
                        .arg("Version", -10)
                        .arg("Base File");
                    found = true;
                }

                qInfo().noquote() << QString("%1  %2  %3  %4  %5")
                    .arg(inst.id, -6)
                    .arg(inst.modTitle.left(30), -30)
                    .arg(inst.modType.left(14), -14)
                    .arg(inst.modVersion.left(10), -10)
                    .arg(file.filename);
            }
        }

        if (wantsJson) {
            printJsonArray(results);
        } else if (!found) {
            qInfo() << "No mods installed.";
        }
        return 0;
    }

    if (hasUninstall) {
        bool ok = false;
        const int installId = ctx.parser.value("mod-uninstall").toInt(&ok);
        if (!ok || installId <= 0) {
            qCritical() << "Invalid mod installation ID";
            return 1;
        }

        PatchService patchSvc;
        ModWorkflowService workflow(ctx.db, patchSvc);
        if (workflow.uninstall(installId)) {
            qInfo() << "✓ Mod uninstalled (installation" << installId << ")";
        } else {
            qCritical() << "Failed to uninstall mod" << installId;
            return 1;
        }
        return 0;
    }

    ModCatalogProvider catalog;
    QString error;
    if (!loadCatalog(ctx, catalog, error)) {
        qCritical().noquote() << error;
        return 1;
    }

    ModQueryOptions queryOptions;
    if (!parseModQueryOptions(ctx.parser, queryOptions, error)) {
        qCritical().noquote() << error;
        return 1;
    }

    const auto emitRows = [&](const QList<ListedMod> &rows) {
        if (queryOptions.jsonOutput) {
            QJsonArray array;
            for (const auto &row : rows) {
                array.append(toJson(row));
            }
            printJsonArray(array);
        } else {
            printModList(rows);
        }
    };

    if (hasSystems) {
        QMap<QString, int> counts;
        for (const auto &mod : catalog.allMods()) {
            counts[mod.system] += 1;
        }

        if (queryOptions.jsonOutput) {
            QJsonArray array;
            for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
                array.append(toJson(it.key(), it.value()));
            }
            printJsonArray(array);
            return 0;
        }

        qInfo() << "=== Catalog Systems ===";
        for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
            qInfo().noquote() << QString("%-24s %1 mods").arg(it.key(), -24).arg(it.value());
        }
        return 0;
    }

    if (hasShow) {
        const QString modId = ctx.parser.value("mod-show");
        const auto modOpt = catalog.getModById(modId);
        if (!modOpt) {
            qCritical() << "Mod not found in catalog:" << modId;
            return 1;
        }

        if (queryOptions.jsonOutput) {
            printJsonObject(toJson({*modOpt, {}}));
        } else {
            qInfo() << "=== Mod Details ===";
            printModDetails(*modOpt);
        }
        return 0;
    }

    if (hasCatalogQuery && !hasList && !hasInstall) {
        const auto mods = filterCatalogMods(catalog.allMods(), queryOptions);
        if (mods.isEmpty()) {
            if (queryOptions.jsonOutput) {
                printJsonArray(QJsonArray{});
            } else {
                qInfo() << "No catalog mods matched the requested filters.";
            }
            return 0;
        }

        if (!queryOptions.jsonOutput) {
            const QString filterLabel = describeActiveFilters(queryOptions);
            qInfo().noquote() << QString("=== Catalog Mods (%1) ===")
                .arg(filterLabel.isEmpty() ? QStringLiteral("all") : filterLabel);
        }
        emitRows(sortListedMods(withScope(mods, {}), queryOptions));
        return 0;
    }

    if (hasList) {
        bool ok = false;
        const int fileId = ctx.parser.value("mod-list").toInt(&ok);
        if (!ok || fileId <= 0) {
            qCritical() << "Invalid file ID for --mod-list";
            return 1;
        }

        FileRecord file = ctx.db.getFileById(fileId);
        if (file.id == 0) {
            qCritical() << "File not found:" << fileId;
            return 1;
        }

        QList<ListedMod> rows = withScope(
            filterCatalogMods(catalog.findModsForRom(file.crc32, file.md5, file.sha1), queryOptions),
            QStringLiteral("hash"));

        if (rows.isEmpty() && queryOptions.allowSystemFallback && file.systemId > 0) {
            ModQueryOptions fallbackOptions = queryOptions;
            if (fallbackOptions.systemFilter.isEmpty()) {
                fallbackOptions.systemFilter = ctx.db.getSystemDisplayName(file.systemId);
            }

            const auto fallbackMods = filterCatalogMods(catalog.allMods(), fallbackOptions);
            rows = withScope(fallbackMods, QStringLiteral("system"));

            if (!rows.isEmpty() && !queryOptions.jsonOutput) {
                qInfo() << "No hash-exact mods found; showing catalog entries for system" << fallbackOptions.systemFilter;
            }
        }

        if (rows.isEmpty()) {
            if (queryOptions.jsonOutput) {
                printJsonArray(QJsonArray{});
            } else if (!queryOptions.allowSystemFallback) {
                qInfo() << "No exact-hash mods available for" << file.filename;
            } else {
                qInfo() << "No mods available for" << file.filename;
            }
            return 0;
        }

        if (!queryOptions.jsonOutput) {
            qInfo().noquote() << QString("=== Available Mods for \"%1\" ===").arg(file.filename);
            if (rows.first().matchScope == QStringLiteral("system")) {
                qInfo() << "These results are filtered by system rather than exact ROM hashes.";
            }
        }
        emitRows(sortListedMods(rows, queryOptions));
        return 0;
    }

    if (hasInstall) {
        const QString modId = ctx.parser.value("mod-install");
        auto modOpt = catalog.getModById(modId);
        if (!modOpt) {
            qCritical() << "Mod not found in catalog:" << modId;
            return 1;
        }

        if (!ctx.parser.isSet("mod-file")) {
            qCritical() << "--mod-install requires --mod-file <fileId>";
            return 1;
        }

        bool ok = false;
        const int fileId = ctx.parser.value("mod-file").toInt(&ok);
        if (!ok || fileId <= 0) {
            qCritical() << "Invalid file ID for --mod-file";
            return 1;
        }

        FileRecord baseFile = ctx.db.getFileById(fileId);
        if (baseFile.id == 0) {
            qCritical() << "Base file not found:" << fileId;
            return 1;
        }

        QString outputDir = ctx.parser.value("mod-output");
        if (outputDir.isEmpty()) {
            const QString basePath = baseFile.currentPath.isEmpty()
                                     ? baseFile.archivePath : baseFile.currentPath;
            outputDir = QFileInfo(basePath).absolutePath();
        }

        PatchService patchSvc;
        ModWorkflowService workflow(ctx.db, patchSvc);

        if (ctx.dryRunAll) {
            qInfo() << "[DRY-RUN] Would install mod" << modOpt->title
                    << "for" << baseFile.filename << "→" << outputDir;
            return 0;
        }

        auto result = workflow.install(baseFile, *modOpt, outputDir,
            [](const QString &stage, int percent) {
                qInfo().noquote() << QString("  [%1%] %2...").arg(percent, 3).arg(stage);
            });

        if (result.success) {
            qInfo() << "";
            qInfo() << "✓ Mod installed:" << modOpt->title;
            qInfo() << "  Patched ROM:" << result.patchedRomPath;
            qInfo() << "  Original ROM untouched:" << baseFile.filename;
        } else {
            qCritical() << "Mod installation failed:" << result.error;
            return 1;
        }
        return 0;
    }

    return 0;
}
