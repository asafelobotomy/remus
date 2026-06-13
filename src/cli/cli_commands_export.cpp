#include "cli_commands.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "../core/constants/constants.h"
#include "../core/database.h"
#include "../core/hasher.h"
#include "../core/library_exporter.h"
#include "../core/patch_engine.h"
#include "../core/patched_rom_parser.h"
#include "../metadata/filename_normalizer.h"
#include "cli_logging.h"

// ── Export ────────────────────────────────────────────────────────────────────

int handleExportCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("export"))
        return 0;

    const QString format = ctx.parser.value("export").toLower();
    QString outputPath = ctx.parser.value("export-path");
    const QString systemsArg = ctx.parser.value("export-systems");
    const QStringList systemFilters = systemsArg.isEmpty() ? QStringList() : systemsArg.split(',', Qt::SkipEmptyParts);

    if (ctx.dryRunAll) {
        const QList<LibraryExportRow> rows = LibraryExporter::buildRows(ctx.db, systemFilters);
        qInfo() << "[DRY-RUN] Would write" << format << "export to"
                << LibraryExporter::resolveOutputPath(format, outputPath) << "(" << rows.size() << "entries)";
        return 0;
    }

    QString error;
    if (!LibraryExporter::exportToFile(ctx.db, format, outputPath, systemFilters, &error)) {
        if (error.contains(QStringLiteral("No matched files"))) {
            qWarning() << error;
            return 0;
        }
        qCritical() << error;
        return 1;
    }

    const QString resolvedPath = LibraryExporter::resolveOutputPath(format, outputPath);
    if (format == Constants::Exports::Formats::RETROARCH)
        qInfo() << "✓ RetroArch playlist exported to" << resolvedPath;
    else if (format == Constants::Exports::Formats::EMUSTATION)
        qInfo() << "✓ EmulationStation gamelist exported to" << resolvedPath;
    else if (format == Constants::Exports::Formats::LAUNCHBOX)
        qInfo() << "✓ LaunchBox XML exported to" << resolvedPath;
    else if (format == Constants::Exports::Formats::CSV)
        qInfo() << "✓ CSV exported to" << resolvedPath;
    else
        qInfo() << "✓ JSON exported to" << resolvedPath;
    return 0;
}

// ── Patch ─────────────────────────────────────────────────────────────────────

static bool persistAppliedPatchLineage(Database &db, const QString &basePath, const QString &patchPath,
    const QString &outputPath, const PatchInfo &patchInfo) {
    Hasher hasher;
    const HashResult baseHashes = hasher.calculateHashes(basePath);
    const HashResult outputHashes = hasher.calculateHashes(outputPath);
    if (!baseHashes.success || !outputHashes.success) {
        return false;
    }

    const PatchedRomInfo outputInfo = PatchedRomParser::parse(QFileInfo(outputPath).completeBaseName());
    const PatchedRomInfo patchInfoFromName = PatchedRomParser::parse(QFileInfo(patchPath).completeBaseName());
    const QString baseTitle
        = !outputInfo.baseTitle.isEmpty() ? outputInfo.baseTitle : QFileInfo(basePath).completeBaseName();
    const QString patchName = !outputInfo.patchName.isEmpty()
        ? outputInfo.patchName
        : (!patchInfoFromName.patchName.isEmpty() ? patchInfoFromName.patchName
                                                  : QFileInfo(patchPath).completeBaseName());
    const QString fileType = !Constants::FileTypes::isOfficial(outputInfo.fileType)
        ? outputInfo.fileType
        : (!Constants::FileTypes::isOfficial(patchInfoFromName.fileType) ? patchInfoFromName.fileType
                                                                         : Constants::FileTypes::HACK);

    Database::AppliedPatchRecord record;
    record.basePath = basePath;
    record.outputPath = outputPath;
    record.patchPath = patchPath;
    record.patchFormat = patchInfo.formatName;
    record.baseTitle = baseTitle;
    record.patchName = patchName;
    record.fileType = fileType;
    record.sourceChecksum = patchInfo.sourceChecksum;
    record.targetChecksum = patchInfo.targetChecksum;
    record.patchChecksum = patchInfo.patchChecksum;
    record.baseCrc32 = baseHashes.crc32;
    record.baseMd5 = baseHashes.md5;
    record.baseSha1 = baseHashes.sha1;
    record.outputCrc32 = outputHashes.crc32;
    record.outputMd5 = outputHashes.md5;
    record.outputSha1 = outputHashes.sha1;
    return db.insertAppliedPatch(record);
}

int handlePatchCommands(CliContext &ctx) {
    if (ctx.parser.isSet("patch-tools")) {
        PatchEngine pe;
        auto tools = pe.checkToolAvailability();
        qInfo() << "=== Patch Tool Availability ===";
        for (auto it = tools.constBegin(); it != tools.constEnd(); ++it)
            qInfo() << it.key() << ":" << (it.value() ? "✓" : "✗");
    }

    if (ctx.parser.isSet("patch-info")) {
        PatchEngine pe;
        PatchInfo info = pe.detectFormat(ctx.parser.value("patch-info"));
        if (info.valid) {
            qInfo() << "Format:" << info.formatName;
            qInfo() << "Size:" << info.size;
            if (!info.sourceChecksum.isEmpty()) {
                qInfo() << "Source CRC:" << info.sourceChecksum;
                qInfo() << "Target CRC:" << info.targetChecksum;
                qInfo() << "Patch CRC:" << info.patchChecksum;
            }
        } else {
            qWarning() << "Could not detect patch format:" << info.error;
        }
    }

    if (ctx.parser.isSet("patch-apply") && ctx.parser.isSet("patch-patch")) {
        const QString basePath = ctx.parser.value("patch-apply");
        const QString patchPath = ctx.parser.value("patch-patch");
        const QString outputPath = ctx.parser.value("patch-output");

        PatchEngine pe;
        PatchInfo info = pe.detectFormat(patchPath);
        if (!info.valid) {
            qCritical() << "Invalid patch file" << info.error;
            return 1;
        }

        if (ctx.dryRunAll) {
            qInfo() << "[DRY-RUN] Would apply patch" << patchPath << "to" << basePath << "->" << outputPath;
        } else {
            PatchResult result = pe.apply(basePath, info, outputPath);
            if (result.success) {
                if (!persistAppliedPatchLineage(ctx.db, basePath, patchPath, result.outputPath, info)) {
                    qWarning() << "Failed to persist applied patch lineage for" << result.outputPath;
                }
                qInfo() << "✓ Patch applied:" << result.outputPath;
            } else {
                qCritical() << "✗ Patch failed:" << result.error;
                return 1;
            }
        }
    }

    if (ctx.parser.isSet("patch-create") && ctx.parser.isSet("patch-original")) {
        const QString modified = ctx.parser.value("patch-create");
        const QString original = ctx.parser.value("patch-original");
        QString patchPath = ctx.parser.value("patch-patch");
        const QString fmtStr = ctx.parser.value("patch-format").toLower();

        PatchFormat format = PatchFormat::BPS;
        if (fmtStr == "ips")
            format = PatchFormat::IPS;
        else if (fmtStr == "ups")
            format = PatchFormat::UPS;
        else if (fmtStr == "xdelta")
            format = PatchFormat::XDelta3;
        else if (fmtStr == "ppf")
            format = PatchFormat::PPF;

        if (patchPath.isEmpty()) {
            QFileInfo baseInfo(original), modInfo(modified);
            const QString ext = (format == PatchFormat::IPS) ? "ips"
                : (format == PatchFormat::UPS)               ? "ups"
                : (format == PatchFormat::XDelta3)           ? "xdelta"
                : (format == PatchFormat::PPF)               ? "ppf"
                                                             : "bps";
            patchPath = baseInfo.absolutePath() + "/" + baseInfo.completeBaseName() + "_to_"
                + modInfo.completeBaseName() + "." + ext;
        }

        PatchEngine pe;
        if (ctx.dryRunAll) {
            qInfo() << "[DRY-RUN] Would create patch" << patchPath << "from" << original << "to" << modified;
        } else if (pe.createPatch(original, modified, patchPath, format)) {
            qInfo() << "✓ Patch created:" << patchPath;
        } else {
            qCritical() << "✗ Failed to create patch";
            return 1;
        }
    }
    return 0;
}
