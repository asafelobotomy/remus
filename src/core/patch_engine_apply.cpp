#include "patch_engine.h"
#include "constants/constants.h"
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>
#include "logging_categories.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug() qCDebug(logCore)
#define qInfo() qCInfo(logCore)
#define qWarning() qCWarning(logCore)
#define qCritical() qCCritical(logCore)

namespace Remus {

PatchResult PatchEngine::applyIPS(const QString &basePath, const QString &patchPath,
                                   const QString &outputPath)
{
    QString flips = getFlipsPath();
    
    if (flips.isEmpty()) {
        // Fall back to built-in implementation
        return applyIPSBuiltin(basePath, patchPath, outputPath);
    }

    PatchResult result;
    result.outputPath = outputPath;

    // Copy base to output first
    if (QFile::exists(outputPath)) {
        QFile::remove(outputPath);
    }
    if (!QFile::copy(basePath, outputPath)) {
        result.error = "Failed to copy base ROM to output location";
        return result;
    }

    // Run flips
    QProcess process;
    process.setProgram(flips);
    process.setArguments({"--apply", patchPath, basePath, outputPath});
    
    process.start();
    process.waitForFinished(Constants::Engines::Patch::EXTERNAL_TOOL_TIMEOUT_MS);

    if (process.exitCode() == 0) {
        result.success = true;
    } else {
        result.error = QString("Flips failed: %1").arg(QString::fromUtf8(process.readAllStandardError()));
        QFile::remove(outputPath);
    }

    return result;
}

PatchResult PatchEngine::applyIPSBuiltin(const QString &basePath, const QString &patchPath,
                                          const QString &outputPath)
{
    PatchResult result;
    result.outputPath = outputPath;

    // Read base ROM
    QFile baseFile(basePath);
    if (!baseFile.open(QIODevice::ReadOnly)) {
        result.error = "Failed to open base ROM";
        return result;
    }
    QByteArray romData = baseFile.readAll();
    baseFile.close();

    // Read patch
    QFile patchFile(patchPath);
    if (!patchFile.open(QIODevice::ReadOnly)) {
        result.error = "Failed to open patch file";
        return result;
    }

    // Verify IPS header "PATCH"
    QByteArray header = patchFile.read(5);
    if (header != "PATCH") {
        result.error = "Invalid IPS header";
        patchFile.close();
        return result;
    }

    // Apply IPS records
    while (!patchFile.atEnd()) {
        QByteArray offsetBytes = patchFile.read(3);
        if (offsetBytes.size() < 3) break;

        // Check for EOF marker
        if (offsetBytes == "EOF") {
            break;
        }

        // Parse 3-byte offset (big endian)
        int offset = (static_cast<unsigned char>(offsetBytes[0]) << 16) |
                     (static_cast<unsigned char>(offsetBytes[1]) << 8) |
                     static_cast<unsigned char>(offsetBytes[2]);

        // Read 2-byte size
        QByteArray sizeBytes = patchFile.read(2);
        if (sizeBytes.size() < 2) {
            result.error = "Truncated patch file";
            patchFile.close();
            return result;
        }

        int size = (static_cast<unsigned char>(sizeBytes[0]) << 8) |
                   static_cast<unsigned char>(sizeBytes[1]);

        // Expand ROM if needed
        if (offset + size > romData.size()) {
            romData.resize(offset + size);
        }

        if (size == 0) {
            // RLE record
            QByteArray rleSizeBytes = patchFile.read(2);
            int rleSize = (static_cast<unsigned char>(rleSizeBytes[0]) << 8) |
                          static_cast<unsigned char>(rleSizeBytes[1]);
            char rleByte = patchFile.read(1)[0];

            for (int i = 0; i < rleSize; i++) {
                romData[offset + i] = rleByte;
            }
        } else {
            // Normal record
            QByteArray data = patchFile.read(size);
            for (int i = 0; i < data.size(); i++) {
                romData[offset + i] = data[i];
            }
        }
    }

    patchFile.close();

    // Write patched ROM
    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        result.error = "Failed to create output file";
        return result;
    }

    outputFile.write(romData);
    outputFile.close();

    result.success = true;
    return result;
}

PatchResult PatchEngine::applyBPS(const QString &basePath, const QString &patchPath,
                                   const QString &outputPath)
{
    PatchResult result;
    result.outputPath = outputPath;

    QString flips = getFlipsPath();
    if (flips.isEmpty()) {
        result.error = "Flips not found - required for BPS/UPS patches";
        return result;
    }

    // Run flips
    QProcess process;
    process.setProgram(flips);
    process.setArguments({"--apply", patchPath, basePath, outputPath});
    
    process.start();
    process.waitForFinished(Constants::Engines::Patch::EXTERNAL_TOOL_LARGE_TIMEOUT_MS);

    if (process.exitCode() == 0) {
        result.success = true;
        result.checksumVerified = true;  // BPS verifies checksums internally
    } else {
        QString errorOutput = QString::fromUtf8(process.readAllStandardError());
        if (errorOutput.isEmpty()) {
            errorOutput = QString::fromUtf8(process.readAllStandardOutput());
        }
        result.error = QString("Flips failed: %1").arg(errorOutput);
    }

    return result;
}

PatchResult PatchEngine::applyXDelta(const QString &basePath, const QString &patchPath,
                                      const QString &outputPath)
{
    PatchResult result;
    result.outputPath = outputPath;

    QString xdelta = getXdelta3Path();
    if (xdelta.isEmpty()) {
        result.error = "xdelta3 not found - required for XDelta patches";
        return result;
    }

    // Run xdelta3: xdelta3 -d -s source patch output
    QProcess process;
    process.setProgram(xdelta);
    process.setArguments({"-d", "-s", basePath, patchPath, outputPath});
    
    process.start();
    process.waitForFinished(Constants::Engines::Patch::EXTERNAL_TOOL_XDELTA_TIMEOUT_MS);

    if (process.exitCode() == 0) {
        result.success = true;
    } else {
        QString errorOutput = QString::fromUtf8(process.readAllStandardError());
        result.error = QString("xdelta3 failed: %1").arg(errorOutput);
    }

    return result;
}

PatchResult PatchEngine::applyPPF(const QString &basePath, const QString &patchPath,
                                  const QString &outputPath)
{
    PatchResult result;
    result.outputPath = outputPath;

    QString ppfTool = getPpfPath();
    if (ppfTool.isEmpty()) {
        result.error = "PPF tool not found - install applyppf or ppf3";
        return result;
    }

    if (QFile::exists(outputPath)) {
        QFile::remove(outputPath);
    }

    if (!QFile::copy(basePath, outputPath)) {
        result.error = "Failed to copy base ROM to output location";
        return result;
    }

    QProcess process;
    process.setProgram(ppfTool);
    process.setArguments({patchPath, outputPath});

    process.start();
    process.waitForFinished(Constants::Engines::Patch::EXTERNAL_TOOL_TIMEOUT_MS);

    if (process.exitCode() == 0) {
        result.success = true;
    } else {
        QString errorOutput = QString::fromUtf8(process.readAllStandardError());
        if (errorOutput.isEmpty()) {
            errorOutput = QString::fromUtf8(process.readAllStandardOutput());
        }
        result.error = QString("PPF patch failed: %1").arg(errorOutput);
        QFile::remove(outputPath);
    }

    return result;
}

bool PatchEngine::createPatch(const QString &originalPath, const QString &modifiedPath,
                               const QString &patchPath, PatchFormat format)
{
    QString flips = getFlipsPath();
    if (flips.isEmpty() && format != PatchFormat::XDelta3) {
        qWarning() << "Flips not found, cannot create IPS/BPS patches";
        return false;
    }

    QString xdelta = getXdelta3Path();
    if (xdelta.isEmpty() && format == PatchFormat::XDelta3) {
        qWarning() << "xdelta3 not found, cannot create XDelta patches";
        return false;
    }

    QProcess process;
    
    switch (format) {
        case PatchFormat::IPS:
            process.setProgram(flips);
            process.setArguments({"--create", "--ips", originalPath, modifiedPath, patchPath});
            break;
            
        case PatchFormat::BPS:
            process.setProgram(flips);
            process.setArguments({"--create", "--bps", originalPath, modifiedPath, patchPath});
            break;
            
        case PatchFormat::XDelta3:
            process.setProgram(xdelta);
            process.setArguments({"-e", "-s", originalPath, modifiedPath, patchPath});
            break;
            
        default:
            qWarning() << "Unsupported format for patch creation";
            return false;
    }

    process.start();
    process.waitForFinished(300000);

    return process.exitCode() == 0;
}

} // namespace Remus
