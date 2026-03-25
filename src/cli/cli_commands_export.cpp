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
#include "../core/patch_engine.h"
#include "../core/patched_rom_parser.h"
#include "cli_logging.h"

// ── Export ────────────────────────────────────────────────────────────────────

struct ExportRow {
    FileRecord           file;
    Database::MatchResult match;
};

static bool persistAppliedPatchLineage(Database &db,
                                       const QString &basePath,
                                       const QString &patchPath,
                                       const QString &outputPath,
                                       const PatchInfo &patchInfo)
{
    Hasher hasher;
    const HashResult baseHashes = hasher.calculateHashes(basePath);
    const HashResult outputHashes = hasher.calculateHashes(outputPath);
    if (!baseHashes.success || !outputHashes.success) {
        return false;
    }

    const PatchedRomInfo outputInfo = PatchedRomParser::parse(QFileInfo(outputPath).completeBaseName());
    const PatchedRomInfo patchInfoFromName = PatchedRomParser::parse(QFileInfo(patchPath).completeBaseName());
    const QString baseTitle = !outputInfo.baseTitle.isEmpty()
        ? outputInfo.baseTitle
        : QFileInfo(basePath).completeBaseName();
    const QString patchName = !outputInfo.patchName.isEmpty()
        ? outputInfo.patchName
        : (!patchInfoFromName.patchName.isEmpty()
            ? patchInfoFromName.patchName
            : QFileInfo(patchPath).completeBaseName());
    const QString fileType = !Constants::FileTypes::isOfficial(outputInfo.fileType)
        ? outputInfo.fileType
        : (!Constants::FileTypes::isOfficial(patchInfoFromName.fileType)
            ? patchInfoFromName.fileType
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

static QList<ExportRow> buildExportRows(CliContext &ctx, const QString &systemsArg)
{
    const QStringList systemFilters = systemsArg.isEmpty()
        ? QStringList() : systemsArg.split(',', Qt::SkipEmptyParts);

    QMap<int, Database::MatchResult> matches = ctx.db.getAllMatches();
    QList<FileRecord> files = ctx.db.getExistingFiles();

    QList<ExportRow> rows;
    for (const FileRecord &file : files) {
        if (!matches.contains(file.id)) continue;
        const QString systemName = ctx.db.getSystemDisplayName(file.systemId);
        if (!systemFilters.isEmpty() && !systemFilters.contains(systemName)) continue;
        rows.append({file, matches.value(file.id)});
    }
    return rows;
}

int handleExportCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("export")) return 0;

    const QString format     = ctx.parser.value("export").toLower();
    QString outputPath       = ctx.parser.value("export-path");
    const QString systemsArg = ctx.parser.value("export-systems");

    if (ctx.dryRunAll) qInfo() << "[DRY-RUN] Export outputs will not be written";

    if (outputPath.isEmpty()) {
        if      (format == Constants::Exports::Formats::RETROARCH)  outputPath = Constants::Exports::Files::DEFAULT_RETROARCH_EXPORT;
        else if (format == Constants::Exports::Formats::EMUSTATION) outputPath = Constants::Exports::Files::ES_GAMELIST;
        else if (format == Constants::Exports::Formats::LAUNCHBOX)  outputPath = Constants::Exports::Files::DEFAULT_LAUNCHBOX_EXPORT;
        else if (format == Constants::Exports::Formats::CSV)        outputPath = Constants::Exports::Files::DEFAULT_CSV_EXPORT;
        else                                                        outputPath = Constants::Exports::Files::DEFAULT_JSON_EXPORT;
    }

    const QList<ExportRow> rows = buildExportRows(ctx, systemsArg);
    if (rows.isEmpty()) { qWarning() << "No matched files to export"; return 0; }

    auto openFile = [&](QFile &f) -> bool {
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qCritical() << "Failed to open" << outputPath;
            return false;
        }
        return true;
    };

    if (format == Constants::Exports::Formats::RETROARCH) {
        if (ctx.dryRunAll) {
            qInfo() << "[DRY-RUN] Would write RetroArch playlist to" << outputPath << "(" << rows.size() << "entries)";
            return 0;
        }
        QFile f(outputPath); if (!openFile(f)) return 1;
        QTextStream out(&f);
        for (const auto &row : rows) {
            out << row.file.currentPath << "\n";
            out << (!row.match.gameTitle.isEmpty() ? row.match.gameTitle : row.file.filename) << "\n";
            out << "DETECT\nDETECT\n";
            out << (row.file.crc32.isEmpty() ? "00000000" : row.file.crc32) << "|crc\n";
            out << ctx.db.getSystemDisplayName(row.file.systemId) << Constants::Exports::Files::PLAYLIST_EXTENSION << "\n";
        }
        qInfo() << "✓ RetroArch playlist exported to" << outputPath;

    } else if (format == Constants::Exports::Formats::EMUSTATION) {
        if (ctx.dryRunAll) {
            qInfo() << "[DRY-RUN] Would write EmulationStation gamelist to" << outputPath << "(" << rows.size() << "entries)";
            return 0;
        }
        QFile f(outputPath); if (!openFile(f)) return 1;
        QTextStream out(&f);
        out << "<gameList>\n";
        for (const auto &row : rows) {
            out << "  <game>\n";
            out << "    <path>"    << row.file.currentPath << "</path>\n";
            out << "    <name>"    << (!row.match.gameTitle.isEmpty() ? row.match.gameTitle : row.file.filename) << "</name>\n";
            out << "    <desc>"    << row.match.description << "</desc>\n";
            out << "    <genre>"   << row.match.genre << "</genre>\n";
            out << "    <players>" << row.match.players << "</players>\n";
            out << "    <region>"  << row.match.region << "</region>\n";
            out << "  </game>\n";
        }
        out << "</gameList>\n";
        qInfo() << "✓ EmulationStation gamelist exported to" << outputPath;

    } else if (format == Constants::Exports::Formats::LAUNCHBOX) {
        if (ctx.dryRunAll) {
            qInfo() << "[DRY-RUN] Would write LaunchBox XML to" << outputPath << "(" << rows.size() << "entries)";
            return 0;
        }
        QFile f(outputPath); if (!openFile(f)) return 1;
        QTextStream out(&f);
        out << "<LaunchBox>\n";
        for (const auto &row : rows) {
            out << "  <Game>\n";
            out << "    <Title>"           << (!row.match.gameTitle.isEmpty() ? row.match.gameTitle : row.file.filename) << "</Title>\n";
            out << "    <ApplicationPath>" << row.file.currentPath << "</ApplicationPath>\n";
            out << "    <Region>"          << row.match.region << "</Region>\n";
            out << "    <Genre>"           << row.match.genre << "</Genre>\n";
            out << "  </Game>\n";
        }
        out << "</LaunchBox>\n";
        qInfo() << "✓ LaunchBox XML exported to" << outputPath;

    } else if (format == Constants::Exports::Formats::CSV) {
        if (ctx.dryRunAll) {
            qInfo() << "[DRY-RUN] Would write CSV to" << outputPath << "(" << rows.size() << "entries)";
            return 0;
        }
        QFile f(outputPath); if (!openFile(f)) return 1;
        QTextStream out(&f);
        out << "file_id,title,system,path,region,confidence\n";
        for (const auto &row : rows) {
            QString title = row.match.gameTitle;
            title.replace(',', ' ');
            out << row.file.id << "," << title << ","
                << ctx.db.getSystemDisplayName(row.file.systemId) << ","
                << row.file.currentPath << "," << row.match.region << ","
                << row.match.confidence << "\n";
        }
        qInfo() << "✓ CSV exported to" << outputPath;

    } else {
        if (ctx.dryRunAll) {
            qInfo() << "[DRY-RUN] Would write JSON to" << outputPath << "(" << rows.size() << "entries)";
            return 0;
        }
        QFile f(outputPath); if (!openFile(f)) return 1;
        QJsonArray arr;
        for (const auto &row : rows) {
            QJsonObject obj;
            obj["fileId"]  = row.file.id;
            obj["title"]   = row.match.gameTitle;
            obj["system"]  = ctx.db.getSystemDisplayName(row.file.systemId);
            obj["path"]    = row.file.currentPath;
            obj["region"]  = row.match.region;
            obj["confidence"] = row.match.confidence;
            arr.append(obj);
        }
        f.write(QJsonDocument(arr).toJson());
        qInfo() << "✓ JSON exported to" << outputPath;
    }
    return 0;
}

// ── Patch ─────────────────────────────────────────────────────────────────────

int handlePatchCommands(CliContext &ctx)
{
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
            qInfo() << "Size:"   << info.size;
            if (!info.sourceChecksum.isEmpty()) {
                qInfo() << "Source CRC:" << info.sourceChecksum;
                qInfo() << "Target CRC:" << info.targetChecksum;
                qInfo() << "Patch CRC:"  << info.patchChecksum;
            }
        } else {
            qWarning() << "Could not detect patch format:" << info.error;
        }
    }

    if (ctx.parser.isSet("patch-apply") && ctx.parser.isSet("patch-patch")) {
        const QString basePath   = ctx.parser.value("patch-apply");
        const QString patchPath  = ctx.parser.value("patch-patch");
        const QString outputPath = ctx.parser.value("patch-output");

        PatchEngine pe;
        PatchInfo info = pe.detectFormat(patchPath);
        if (!info.valid) { qCritical() << "Invalid patch file" << info.error; return 1; }

        if (ctx.dryRunAll) {
            qInfo() << "[DRY-RUN] Would apply patch" << patchPath << "to" << basePath << "->" << outputPath;
        } else {
            PatchResult result = pe.apply(basePath, info, outputPath);
            if (result.success) {
                if (!persistAppliedPatchLineage(ctx.db, basePath, patchPath, result.outputPath, info)) {
                    qWarning() << "Failed to persist applied patch lineage for" << result.outputPath;
                }
                qInfo() << "✓ Patch applied:" << result.outputPath;
            }
            else { qCritical() << "✗ Patch failed:" << result.error; return 1; }
        }
    }

    if (ctx.parser.isSet("patch-create") && ctx.parser.isSet("patch-original")) {
        const QString modified  = ctx.parser.value("patch-create");
        const QString original  = ctx.parser.value("patch-original");
        QString patchPath       = ctx.parser.value("patch-patch");
        const QString fmtStr    = ctx.parser.value("patch-format").toLower();

        PatchFormat format = PatchFormat::BPS;
        if      (fmtStr == "ips")    format = PatchFormat::IPS;
        else if (fmtStr == "ups")    format = PatchFormat::UPS;
        else if (fmtStr == "xdelta") format = PatchFormat::XDelta3;
        else if (fmtStr == "ppf")    format = PatchFormat::PPF;

        if (patchPath.isEmpty()) {
            QFileInfo baseInfo(original), modInfo(modified);
            const QString ext = (format == PatchFormat::IPS)    ? "ips"  :
                                 (format == PatchFormat::UPS)    ? "ups"  :
                                 (format == PatchFormat::XDelta3)? "xdelta" :
                                 (format == PatchFormat::PPF)    ? "ppf"  : "bps";
            patchPath = baseInfo.absolutePath() + "/" +
                baseInfo.completeBaseName() + "_to_" + modInfo.completeBaseName() + "." + ext;
        }

        PatchEngine pe;
        if (ctx.dryRunAll) {
            qInfo() << "[DRY-RUN] Would create patch" << patchPath
                    << "from" << original << "to" << modified;
        } else if (pe.createPatch(original, modified, patchPath, format)) {
            qInfo() << "✓ Patch created:" << patchPath;
        } else {
            qCritical() << "✗ Failed to create patch";
            return 1;
        }
    }
    return 0;
}
