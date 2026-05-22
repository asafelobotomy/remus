#include "cli_commands.h"
#include "cli_helpers.h"
#include <QDir>
#include <QFileInfo>
#include "../core/rvz_converter.h"
#include "../core/cso_converter.h"
#include "../core/wbfs_converter.h"
#include "../core/pbp_exporter.h"
#include "../core/constants/files.h"
#include "../core/space_calculator.h"
#include "cli_logging.h"

int handleConvertRvzCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("convert-rvz")) return 0;

    const QString inputPath = ctx.parser.value("convert-rvz");
    const QString outputDir = ctx.parser.value("output-dir");
    const QString compressionStr = ctx.parser.value("rvz-compression");

    qInfo() << "";
    qInfo() << "=== Convert Disc Image to RVZ ===";
    qInfo() << "Input:" << inputPath;

    RVZConverter converter;
    if (!converter.isDolphinToolAvailable()) {
        qCritical() << "✗ dolphin-tool not found. Install dolphin-emu-tool package";
        return 1;
    }
    qInfo() << "dolphin-tool version:" << converter.getDolphinToolVersion();

    RVZCompression compression = RVZCompression::Auto;
    if      (compressionStr == "zstd")  compression = RVZCompression::Zstd;
    else if (compressionStr == "bzip2") compression = RVZCompression::Bzip2;
    else if (compressionStr == "lzma")  compression = RVZCompression::LZMA;
    else if (compressionStr == "lzma2") compression = RVZCompression::LZMA2;
    else if (compressionStr == "none")  compression = RVZCompression::None;
    converter.setCompression(compression);

    QFileInfo info(inputPath);
    const QString ext = QStringLiteral(".") + info.suffix().toLower();
    if (!Remus::Constants::Files::containsExtension(Remus::Constants::Files::RVZ_SOURCE_EXTENSIONS, ext)) {
        qCritical() << "✗ Unsupported format:" << ext;
        qInfo() << "Supported formats: .iso, .gcm";
        return 1;
    }

    QString outputPath = buildOutputPath(inputPath, outputDir, "rvz");

    qInfo() << "Output:" << outputPath;
    qInfo() << "Compression:" << compressionStr;
    qInfo() << "";

    if (ctx.dryRunAll || ctx.parser.isSet(QStringLiteral("dry-run"))) {
        qInfo() << "[DRY-RUN] Would convert" << inputPath << "to" << outputPath;
        return 0;
    }

    ConversionResult result = converter.convertIsoToRVZ(inputPath, outputPath);
    if (!printConversionResult(result, "RVZ")) return 1;
    return 0;
}

int handleRvzExtractCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("rvz-extract")) return 0;

    const QString rvzPath  = ctx.parser.value("rvz-extract");
    const QString outputDir = ctx.parser.value("output-dir");

    qInfo() << "";
    qInfo() << "=== Extract RVZ to ISO ===";
    qInfo() << "Input:" << rvzPath;

    RVZConverter converter;
    if (!converter.isDolphinToolAvailable()) {
        qCritical() << "✗ dolphin-tool not found";
        return 1;
    }

    QFileInfo info(rvzPath);
    QString outputPath = buildOutputPath(rvzPath, outputDir, "iso");

    qInfo() << "Output:" << outputPath;
    qInfo() << "";

    if (ctx.dryRunAll || ctx.parser.isSet(QStringLiteral("dry-run"))) {
        qInfo() << "[DRY-RUN] Would extract" << rvzPath << "to" << outputPath;
        return 0;
    }

    ConversionResult result = converter.extractRVZToIso(rvzPath, outputPath);
    if (result.success) {
        qInfo() << "✓ Extraction successful!";
        qInfo() << "  Extracted to:" << outputPath;
    } else {
        qCritical() << "✗ Extraction failed:" << result.error;
        return 1;
    }
    return 0;
}

int handleRvzVerifyCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("rvz-verify")) return 0;

    const QString rvzPath = ctx.parser.value("rvz-verify");
    qInfo() << "";
    qInfo() << "=== Verify RVZ Integrity ===";
    qInfo() << "File:" << rvzPath;
    qInfo() << "";

    RVZConverter converter;
    if (!converter.isDolphinToolAvailable()) {
        qCritical() << "✗ dolphin-tool not found";
        return 1;
    }

    VerifyResult result = converter.verifyRVZ(rvzPath);
    if (result.valid) {
        qInfo() << "✓ RVZ is valid!";
        qInfo() << "  " << result.details;
    } else {
        qCritical() << "✗ RVZ verification failed!";
        qInfo() << "  Error:" << result.error;
        return 1;
    }
    return 0;
}

int handleConvertCsoCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("convert-cso")) return 0;

    const QString inputPath = ctx.parser.value("convert-cso");
    const QString outputDir = ctx.parser.value("output-dir");

    qInfo() << "";
    qInfo() << "=== Convert ISO to CSO ===";
    qInfo() << "Input:" << inputPath;

    CSOConverter converter;
    if (!converter.isMaxcsoAvailable()) {
        qCritical() << "✗ maxcso not found. Install maxcso package";
        return 1;
    }
    qInfo() << "maxcso version:" << converter.getMaxcsoVersion();

    QFileInfo info(inputPath);
    const QString ext = QStringLiteral(".") + info.suffix().toLower();
    if (!Remus::Constants::Files::containsExtension(Remus::Constants::Files::CSO_SOURCE_EXTENSIONS, ext)) {
        qCritical() << "✗ Unsupported format:" << ext;
        qInfo() << "Supported formats: .iso";
        return 1;
    }

    QString outputPath = buildOutputPath(inputPath, outputDir, "cso");

    qInfo() << "Output:" << outputPath;
    qInfo() << "";

    if (ctx.dryRunAll || ctx.parser.isSet(QStringLiteral("dry-run"))) {
        qInfo() << "[DRY-RUN] Would convert" << inputPath << "to" << outputPath;
        return 0;
    }

    ConversionResult result = converter.convertIsoToCSO(inputPath, outputPath);
    if (!printConversionResult(result, "CSO")) return 1;
    return 0;
}

int handleCsoExtractCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("cso-extract")) return 0;

    const QString csoPath  = ctx.parser.value("cso-extract");
    const QString outputDir = ctx.parser.value("output-dir");

    qInfo() << "";
    qInfo() << "=== Extract CSO to ISO ===";
    qInfo() << "Input:" << csoPath;

    CSOConverter converter;
    if (!converter.isMaxcsoAvailable()) {
        qCritical() << "✗ maxcso not found";
        return 1;
    }

    QFileInfo info(csoPath);
    QString outputPath = buildOutputPath(csoPath, outputDir, "iso");

    qInfo() << "Output:" << outputPath;
    qInfo() << "";

    if (ctx.dryRunAll || ctx.parser.isSet(QStringLiteral("dry-run"))) {
        qInfo() << "[DRY-RUN] Would extract" << csoPath << "to" << outputPath;
        return 0;
    }

    ConversionResult result = converter.extractCSOToIso(csoPath, outputPath);
    if (result.success) {
        qInfo() << "✓ Extraction successful!";
        qInfo() << "  Extracted to:" << outputPath;
    } else {
        qCritical() << "✗ Extraction failed:" << result.error;
        return 1;
    }
    return 0;
}

int handleConvertWbfsCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("convert-wbfs")) return 0;

    const QString inputPath = ctx.parser.value("convert-wbfs");
    const QString outputDir = ctx.parser.value("output-dir");

    qInfo() << "";
    qInfo() << "=== Convert ISO to WBFS ===";
    qInfo() << "Input:" << inputPath;

    WBFSConverter converter;
    if (!converter.isWitAvailable()) {
        qCritical() << "✗ wit not found. Install wiimms-iso-tools package";
        return 1;
    }
    qInfo() << "wit version:" << converter.getWitVersion();

    QFileInfo info(inputPath);
    const QString ext = QStringLiteral(".") + info.suffix().toLower();
    if (!Remus::Constants::Files::containsExtension(Remus::Constants::Files::WBFS_SOURCE_EXTENSIONS, ext)) {
        qCritical() << "✗ Unsupported format:" << ext;
        qInfo() << "Supported formats: .iso, .gcm";
        return 1;
    }

    QString outputPath = buildOutputPath(inputPath, outputDir, "wbfs");

    qInfo() << "Output:" << outputPath;
    qInfo() << "";

    if (ctx.dryRunAll || ctx.parser.isSet(QStringLiteral("dry-run"))) {
        qInfo() << "[DRY-RUN] Would convert" << inputPath << "to" << outputPath;
        return 0;
    }

    ConversionResult result = converter.convertIsoToWbfs(inputPath, outputPath);
    if (!printConversionResult(result, "WBFS")) return 1;
    return 0;
}

int handleWbfsExtractCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("wbfs-extract")) return 0;

    const QString wbfsPath  = ctx.parser.value("wbfs-extract");
    const QString outputDir = ctx.parser.value("output-dir");

    qInfo() << "";
    qInfo() << "=== Extract WBFS to ISO ===";
    qInfo() << "Input:" << wbfsPath;

    WBFSConverter converter;
    if (!converter.isWitAvailable()) {
        qCritical() << "✗ wit not found. Install wiimms-iso-tools package";
        return 1;
    }

    QFileInfo info(wbfsPath);
    QString outputPath = buildOutputPath(wbfsPath, outputDir, "iso");

    qInfo() << "Output:" << outputPath;
    qInfo() << "";

    if (ctx.dryRunAll || ctx.parser.isSet(QStringLiteral("dry-run"))) {
        qInfo() << "[DRY-RUN] Would extract" << wbfsPath << "to" << outputPath;
        return 0;
    }

    ConversionResult result = converter.extractWbfsToIso(wbfsPath, outputPath);
    if (result.success) {
        qInfo() << "✓ Extraction successful!";
        qInfo() << "  Extracted to:" << outputPath;
    } else {
        qCritical() << "✗ Extraction failed:" << result.error;
        return 1;
    }
    return 0;
}

int handleExportPBPCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("export-pbp")) return 0;

    const QString inputPath = ctx.parser.value("export-pbp");
    const QString outputDir = ctx.parser.value("output-dir");

    qInfo() << "";
    qInfo() << "=== Export PS1 to PBP ===";
    qInfo() << "Input:" << inputPath;
    qInfo() << "Note: PBP is an export-only format. Source files are not modified.";

    PBPExporter exporter;
    if (!exporter.isPSXPackagerAvailable()) {
        qCritical() << "✗ PSXPackager not found. Install PSXPackager from";
        qCritical() << "  https://github.com/nicholasstephan/psxpackager";
        return 1;
    }
    qInfo() << "PSXPackager version:" << exporter.getPSXPackagerVersion();

    QFileInfo info(inputPath);
    const QString ext = QStringLiteral(".") + info.suffix().toLower();
    if (!Remus::Constants::Files::containsExtension(Remus::Constants::Files::PBP_SOURCE_EXTENSIONS, ext)) {
        qCritical() << "✗ Unsupported format:" << ext;
        qInfo() << "Supported formats: .cue, .iso, .m3u";
        return 1;
    }

    QString outputPath = buildOutputPath(inputPath, outputDir, "pbp");

    qInfo() << "Output:" << outputPath;
    qInfo() << "";

    if (ctx.dryRunAll || ctx.parser.isSet(QStringLiteral("dry-run"))) {
        qInfo() << "[DRY-RUN] Would export" << inputPath << "to" << outputPath;
        return 0;
    }

    ConversionResult result = exporter.exportToPBP(inputPath, outputPath);
    if (!printConversionResult(result, "PBP")) return 1;
    return 0;
}
