#include "cli_commands.h"
#include <QDir>
#include <QFileInfo>
#include "../core/chd_converter.h"
#include "../core/archive_extractor.h"
#include "../core/space_calculator.h"
#include "cli_logging.h"

int handleConvertChdCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("convert-chd")) return 0;

    const QString inputPath = ctx.parser.value("convert-chd");
    const QString outputDir = ctx.parser.value("output-dir");
    const QString codecStr  = ctx.parser.value("chd-codec");

    qInfo() << "";
    qInfo() << "=== Convert Disc Image to CHD (M4.5) ===";
    qInfo() << "Input:" << inputPath;

    CHDConverter converter;
    if (!converter.isChdmanAvailable()) {
        qCritical() << "✗ chdman not found. Install MAME tools (mame-tools package)";
        return 1;
    }
    qInfo() << "chdman version:" << converter.getChdmanVersion();

    CHDCodec codec = CHDCodec::Auto;
    if      (codecStr == "lzma") codec = CHDCodec::LZMA;
    else if (codecStr == "zlib") codec = CHDCodec::ZLIB;
    else if (codecStr == "flac") codec = CHDCodec::FLAC;
    else if (codecStr == "huff") codec = CHDCodec::Huffman;
    converter.setCodec(codec);

    QFileInfo info(inputPath);
    QString outputPath;
    if (outputDir.isEmpty()) outputPath = info.absolutePath() + "/" + info.completeBaseName() + ".chd";
    else { QDir().mkpath(outputDir); outputPath = outputDir + "/" + info.completeBaseName() + ".chd"; }

    qInfo() << "Output:" << outputPath;
    qInfo() << "Codec:"  << codecStr;
    qInfo() << "";

    if (ctx.dryRunAll) {
        qInfo() << "[DRY-RUN] Would convert" << inputPath << "to" << outputPath << "using" << codecStr;
        return 0;
    }

    const QString ext = info.suffix().toLower();
    CHDConversionResult result;
    if      (ext == "cue")             result = converter.convertCueToCHD(inputPath, outputPath);
    else if (ext == "iso" || ext == "img") result = converter.convertIsoToCHD(inputPath, outputPath);
    else if (ext == "gdi")             result = converter.convertGdiToCHD(inputPath, outputPath);
    else {
        qCritical() << "✗ Unsupported format:" << ext;
        qInfo() << "Supported formats: .cue, .iso, .img, .gdi";
        return 1;
    }

    if (result.success) {
        qInfo() << "✓ Conversion successful!";
        qInfo() << "  Original size:" << SpaceCalculator::formatBytes(result.inputSize);
        qInfo() << "  CHD size:"      << SpaceCalculator::formatBytes(result.outputSize);
        qInfo() << "  Saved:"         << SpaceCalculator::formatBytes(result.inputSize - result.outputSize);
        qInfo() << "  Compression:"
                << QString::number((1.0 - result.compressionRatio) * 100, 'f', 1) << "%";
    } else {
        qCritical() << "✗ Conversion failed:" << result.error;
        return 1;
    }
    return 0;
}

int handleChdExtractCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("chd-extract")) return 0;

    const QString chdPath  = ctx.parser.value("chd-extract");
    const QString outputDir = ctx.parser.value("output-dir");

    qInfo() << "";
    qInfo() << "=== Extract CHD to BIN/CUE (M4.5) ===";
    qInfo() << "Input:" << chdPath;

    CHDConverter converter;
    if (!converter.isChdmanAvailable()) { qCritical() << "✗ chdman not found"; return 1; }

    QFileInfo info(chdPath);
    QString outputPath;
    if (outputDir.isEmpty()) outputPath = info.absolutePath() + "/" + info.completeBaseName() + ".cue";
    else { QDir().mkpath(outputDir); outputPath = outputDir + "/" + info.completeBaseName() + ".cue"; }

    qInfo() << "Output:" << outputPath;
    qInfo() << "";

    if (ctx.dryRunAll) {
        qInfo() << "[DRY-RUN] Would extract" << chdPath << "to" << outputPath;
        return 0;
    }

    CHDConversionResult result = converter.extractCHDToCue(chdPath, outputPath);
    if (result.success) {
        qInfo() << "✓ Extraction successful!";
        qInfo() << "  Extracted to:" << outputPath;
    } else {
        qCritical() << "✗ Extraction failed:" << result.error;
        return 1;
    }
    return 0;
}

int handleChdVerifyCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("chd-verify")) return 0;

    const QString chdPath = ctx.parser.value("chd-verify");
    qInfo() << "";
    qInfo() << "=== Verify CHD Integrity (M4.5) ===";
    qInfo() << "File:" << chdPath;
    qInfo() << "";

    CHDConverter converter;
    if (!converter.isChdmanAvailable()) { qCritical() << "✗ chdman not found"; return 1; }

    CHDVerifyResult result = converter.verifyCHD(chdPath);
    if (result.valid) {
        qInfo() << "✓ CHD is valid!";
        qInfo() << "  " << result.details;
    } else {
        qCritical() << "✗ CHD verification failed!";
        qInfo() << "  Error:" << result.error;
        return 1;
    }
    return 0;
}

int handleChdInfoCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("chd-info")) return 0;

    const QString chdPath = ctx.parser.value("chd-info");
    qInfo() << "";
    qInfo() << "=== CHD File Information (M4.5) ===";
    qInfo() << "File:" << chdPath;
    qInfo() << "";

    CHDConverter converter;
    if (!converter.isChdmanAvailable()) { qCritical() << "✗ chdman not found"; return 1; }

    CHDInfo info = converter.getCHDInfo(chdPath);
    if (info.version > 0) {
        qInfo() << "  CHD Version:"  << info.version;
        qInfo() << "  Compression:"  << info.compression;
        qInfo() << "  Logical Size:" << SpaceCalculator::formatBytes(info.logicalSize);
        qInfo() << "  Physical Size:" << SpaceCalculator::formatBytes(info.physicalSize);
        const double ratio = (info.logicalSize > 0)
            ? static_cast<double>(info.physicalSize) / static_cast<double>(info.logicalSize) : 0.0;
        qInfo() << "  Compression Ratio:"
                << QString::number((1.0 - ratio) * 100, 'f', 1) << "%";
        qInfo() << "  SHA1:" << info.sha1;
    } else {
        qCritical() << "✗ Failed to read CHD info";
        return 1;
    }
    return 0;
}

int handleExtractArchiveCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("extract-archive")) return 0;

    const QString archivePath = ctx.parser.value("extract-archive");
    QString outputDir         = ctx.parser.value("output-dir");

    qInfo() << "";
    qInfo() << "=== Extract Archive (M4.5) ===";
    qInfo() << "Archive:" << archivePath;

    ArchiveExtractor extractor;
    QMap<ArchiveFormat, bool> tools = extractor.getAvailableTools();
    QStringList available;
    if (tools.value(ArchiveFormat::ZIP))     available << "unzip";
    if (tools.value(ArchiveFormat::SevenZip)) available << "7z";
    if (tools.value(ArchiveFormat::RAR))     available << "unrar";

    if (available.isEmpty()) {
        qCritical() << "✗ No extraction tools found (need unzip, 7z, or unrar)";
        return 1;
    }
    qInfo() << "Available tools:" << available.join(", ");

    ArchiveFormat format = extractor.detectFormat(archivePath);
    if (format == ArchiveFormat::Unknown) { qCritical() << "✗ Unknown archive format"; return 1; }

    if (outputDir.isEmpty()) {
        QFileInfo info(archivePath);
        outputDir = info.absolutePath() + "/" + info.completeBaseName();
    }
    QDir().mkpath(outputDir);

    qInfo() << "Output:" << outputDir;
    qInfo() << "";

    if (ctx.dryRunAll) {
        qInfo() << "[DRY-RUN] Would extract" << archivePath << "to" << outputDir;
        return 0;
    }

    ExtractionResult result = extractor.extract(archivePath, outputDir);
    if (result.success) {
        qInfo() << "✓ Extraction successful!";
        qInfo() << "  Files extracted:" << result.filesExtracted;
        for (const QString &path : result.extractedFiles) qInfo() << "    " << path;
    } else {
        qCritical() << "✗ Extraction failed:" << result.error;
        return 1;
    }
    return 0;
}

int handleSpaceReportCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("space-report")) return 0;

    const QString dirPath = ctx.parser.value("space-report");
    qInfo() << "";
    qInfo() << "=== CHD Conversion Savings Report (M4.5) ===";
    qInfo() << "";

    SpaceCalculator calculator;
    QObject::connect(&calculator, &SpaceCalculator::scanProgress,
        [](int count, const QString &) {
            if (count % 50 == 0) qInfo() << "  Scanned" << count << "files...";
        });

    qInfo() << "Scanning:" << dirPath;
    qInfo() << "";

    ConversionSummary summary = calculator.scanDirectory(dirPath, true);
    qInfo().noquote() << calculator.formatSavingsReport(summary);
    return 0;
}
