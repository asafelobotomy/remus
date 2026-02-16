#include "patch_engine.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>
#include <QCryptographicHash>
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

static quint32 readLe32(const QByteArray &data, int offset)
{
    if (offset + 4 > data.size()) {
        return 0;
    }

    return static_cast<quint32>(static_cast<unsigned char>(data[offset])) |
           (static_cast<quint32>(static_cast<unsigned char>(data[offset + 1])) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(data[offset + 2])) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(data[offset + 3])) << 24);
}

static QString formatChecksum(quint32 value)
{
    return QString("%1").arg(value, 8, 16, QChar('0'));
}

PatchEngine::PatchEngine(QObject *parent)
    : QObject(parent)
{
}

QString PatchEngine::findExecutable(const QString &name)
{
    // Check in PATH
    QString path = QStandardPaths::findExecutable(name);
    if (!path.isEmpty()) {
        return path;
    }

    // Check common locations
    QStringList searchPaths = {
        "/usr/bin",
        "/usr/local/bin",
        "/opt/homebrew/bin",  // macOS Homebrew
        QDir::homePath() + "/.local/bin",
        QCoreApplication::applicationDirPath(),  // Same dir as Remus
        QCoreApplication::applicationDirPath() + "/tools"
    };

    for (const QString &searchPath : searchPaths) {
        QString candidate = searchPath + "/" + name;
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }

    return QString();
}

QString PatchEngine::getFlipsPath()
{
    if (!m_flipsPath.isEmpty() && QFile::exists(m_flipsPath)) {
        return m_flipsPath;
    }

    // Try to find flips
    m_flipsPath = findExecutable("flips");
    if (m_flipsPath.isEmpty()) {
        m_flipsPath = findExecutable("flips-linux");
    }
    
    return m_flipsPath;
}

QString PatchEngine::getXdelta3Path()
{
    if (!m_xdelta3Path.isEmpty() && QFile::exists(m_xdelta3Path)) {
        return m_xdelta3Path;
    }

    m_xdelta3Path = findExecutable("xdelta3");
    return m_xdelta3Path;
}

QString PatchEngine::getPpfPath()
{
    if (!m_ppfPath.isEmpty() && QFile::exists(m_ppfPath)) {
        return m_ppfPath;
    }

    m_ppfPath = findExecutable("applyppf");
    if (m_ppfPath.isEmpty()) {
        m_ppfPath = findExecutable("ppf3");
    }

    return m_ppfPath;
}

void PatchEngine::setFlipsPath(const QString &path)
{
    m_flipsPath = path;
}

void PatchEngine::setXdelta3Path(const QString &path)
{
    m_xdelta3Path = path;
}

void PatchEngine::setPpfPath(const QString &path)
{
    m_ppfPath = path;
}

QMap<QString, bool> PatchEngine::checkToolAvailability()
{
    QMap<QString, bool> tools;
    
    tools["flips"] = !getFlipsPath().isEmpty();
    tools["xdelta3"] = !getXdelta3Path().isEmpty();
    tools["ppf"] = !getPpfPath().isEmpty();
    tools["ips_builtin"] = true;  // Always available
    
    return tools;
}

bool PatchEngine::isFormatSupported(PatchFormat format)
{
    switch (format) {
        case PatchFormat::IPS:
            return true;  // Built-in support + Flips
        case PatchFormat::BPS:
        case PatchFormat::UPS:
            return !getFlipsPath().isEmpty();
        case PatchFormat::XDelta3:
            return !getXdelta3Path().isEmpty();
        case PatchFormat::PPF:
            return !getPpfPath().isEmpty();
        default:
            return false;
    }
}

PatchFormat PatchEngine::formatFromExtension(const QString &extension)
{
    QString ext = extension.toLower();
    if (!ext.startsWith('.')) {
        ext = "." + ext;
    }

    if (ext == ".ips") return PatchFormat::IPS;
    if (ext == ".bps") return PatchFormat::BPS;
    if (ext == ".ups") return PatchFormat::UPS;
    if (ext == ".xdelta" || ext == ".xdelta3" || ext == ".vcdiff") return PatchFormat::XDelta3;
    if (ext == ".ppf") return PatchFormat::PPF;

    return PatchFormat::Unknown;
}

QString PatchEngine::formatName(PatchFormat format)
{
    switch (format) {
        case PatchFormat::IPS: return "IPS";
        case PatchFormat::BPS: return "BPS";
        case PatchFormat::UPS: return "UPS";
        case PatchFormat::XDelta3: return "XDelta3";
        case PatchFormat::PPF: return "PPF";
        default: return "Unknown";
    }
}

PatchInfo PatchEngine::detectFormat(const QString &patchPath)
{
    PatchInfo info;
    info.path = patchPath;

    QFile file(patchPath);
    if (!file.open(QIODevice::ReadOnly)) {
        info.error = "Failed to open patch file";
        return info;
    }

    info.size = file.size();
    QByteArray header = file.read(8);
    file.close();

    // Check magic bytes
    if (header.startsWith("PATCH")) {
        info.format = PatchFormat::IPS;
        info.formatName = "IPS";
        info.valid = true;
    } else if (header.startsWith("BPS1")) {
        info.format = PatchFormat::BPS;
        info.formatName = "BPS";
        info.valid = true;

        QFile bpsFile(patchPath);
        if (bpsFile.open(QIODevice::ReadOnly) && bpsFile.size() >= 12) {
            bpsFile.seek(bpsFile.size() - 12);
            QByteArray footer = bpsFile.read(12);
            bpsFile.close();

            quint32 sourceCrc = readLe32(footer, 0);
            quint32 targetCrc = readLe32(footer, 4);
            quint32 patchCrc = readLe32(footer, 8);

            info.sourceChecksum = formatChecksum(sourceCrc);
            info.targetChecksum = formatChecksum(targetCrc);
            info.patchChecksum = formatChecksum(patchCrc);
        } else {
            info.error = "Failed to parse BPS checksums";
        }
    } else if (header.startsWith("UPS1")) {
        info.format = PatchFormat::UPS;
        info.formatName = "UPS";
        info.valid = true;
    } else if (header.size() >= 4 && 
               static_cast<unsigned char>(header[0]) == 0xD6 &&
               static_cast<unsigned char>(header[1]) == 0xC3 &&
               static_cast<unsigned char>(header[2]) == 0xC4) {
        // XDelta3 magic: 0xD6C3C4
        info.format = PatchFormat::XDelta3;
        info.formatName = "XDelta3";
        info.valid = true;
    } else if (header.startsWith("PPF")) {
        info.format = PatchFormat::PPF;
        info.formatName = "PPF";
        info.valid = true;
    } else {
        // Try extension-based detection as fallback
        info.format = formatFromExtension(QFileInfo(patchPath).suffix());
        if (info.format != PatchFormat::Unknown) {
            info.formatName = formatName(info.format);
            info.valid = true;
        } else {
            info.error = "Unable to detect patch format";
        }
    }

    return info;
}

QString PatchEngine::generateOutputPath(const QString &basePath, const QString &patchPath)
{
    QFileInfo baseInfo(basePath);
    QFileInfo patchInfo(patchPath);

    // Generate name like "BaseRom [PatchName].ext"
    QString baseName = baseInfo.completeBaseName();
    QString patchName = patchInfo.completeBaseName();
    QString ext = baseInfo.suffix();

    QString outputName = QString("%1 [%2].%3")
        .arg(baseName)
        .arg(patchName)
        .arg(ext);

    return baseInfo.dir().filePath(outputName);
}

PatchResult PatchEngine::apply(const QString &basePath, const PatchInfo &patch,
                                const QString &outputPath)
{
    PatchResult result;
    
    if (!patch.valid) {
        result.error = "Invalid patch: " + patch.error;
        emit patchError(result.error);
        return result;
    }

    QString output = outputPath.isEmpty() ? generateOutputPath(basePath, patch.path) : outputPath;
    result.outputPath = output;

    // Check if base file exists
    if (!QFile::exists(basePath)) {
        result.error = "Base ROM file not found: " + basePath;
        emit patchError(result.error);
        return result;
    }

    // Apply based on format
    switch (patch.format) {
        case PatchFormat::IPS:
            result = applyIPS(basePath, patch.path, output);
            break;
        case PatchFormat::BPS:
        case PatchFormat::UPS:
            result = applyBPS(basePath, patch.path, output);
            break;
        case PatchFormat::XDelta3:
            result = applyXDelta(basePath, patch.path, output);
            break;
        case PatchFormat::PPF:
            result = applyPPF(basePath, patch.path, output);
            break;
        default:
            result.error = QString("Unsupported patch format: %1").arg(patch.formatName);
            emit patchError(result.error);
            return result;
    }

    if (result.success) {
        emit patchComplete(result);
    } else {
        emit patchError(result.error);
    }

    return result;
}

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
    process.waitForFinished(60000);  // 60 second timeout

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
    process.waitForFinished(120000);  // 2 minute timeout for larger patches

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
    process.waitForFinished(300000);  // 5 minute timeout for large disc images

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
    process.waitForFinished(60000);

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
