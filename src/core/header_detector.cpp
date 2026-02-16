#include "header_detector.h"
#include <QFile>
#include <QDebug>
#include <QFileInfo>

namespace Remus {

HeaderDetector::HeaderDetector(QObject *parent)
    : QObject(parent)
{
}

HeaderInfo HeaderDetector::detect(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        HeaderInfo info;
        info.info = QString("Failed to open file: %1").arg(filePath);
        return info;
    }

    // Read enough data for any header type (512 bytes max for SMC)
    QByteArray data = file.read(512);
    qint64 fileSize = file.size();
    file.close();

    QString extension = QFileInfo(filePath).suffix().toLower();
    if (!extension.startsWith('.')) {
        extension = "." + extension;
    }

    HeaderInfo info = detectFromData(data, extension);
    
    // For SNES, we need file size for SMC detection
    if (extension == ".smc" || extension == ".sfc") {
        info = detectSNES(data, fileSize);
    }

    return info;
}

HeaderInfo HeaderDetector::detectFromData(const QByteArray &data, const QString &extension)
{
    HeaderInfo info;
    
    // Check by extension and magic bytes
    if (extension == ".nes" || extension == ".unf") {
        return detectNES(data);
    } else if (extension == ".lnx") {
        return detectLynx(data);
    } else if (extension == ".smc" || extension == ".sfc") {
        return detectSNES(data, 0);  // Will be called again with file size
    } else if (extension == ".fds") {
        return detectFDS(data);
    } else if (extension == ".a78") {
        return detectA78(data);
    }

    // Try to detect by magic bytes if extension is unknown
    if (data.size() >= 4) {
        // Check for iNES magic
        if (data[0] == 'N' && data[1] == 'E' && data[2] == 'S' && data[3] == 0x1A) {
            return detectNES(data);
        }
        // Check for Lynx magic
        if (data[0] == 'L' && data[1] == 'Y' && data[2] == 'N' && data[3] == 'X') {
            return detectLynx(data);
        }
        // Check for FDS magic
        if (data[0] == 'F' && data[1] == 'D' && data[2] == 'S' && data[3] == 0x1A) {
            return detectFDS(data);
        }
    }

    return info;  // No header detected
}

HeaderInfo HeaderDetector::detectNES(const QByteArray &data)
{
    HeaderInfo info;

    if (data.size() < 16) {
        return info;
    }

    // Check for iNES magic: "NES\x1A"
    if (data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) {
        return info;
    }

    info.hasHeader = true;
    info.headerSize = 16;
    info.headerData = data.left(16);
    info.systemHint = "NES";
    info.valid = true;

    // Detect iNES vs NES 2.0
    if (isNES20Format(data)) {
        info.headerType = "NES2.0";
    } else {
        info.headerType = "iNES";
    }

    info.info = parseMapperInfo(data);

    return info;
}

bool HeaderDetector::isNES20Format(const QByteArray &header)
{
    // NES 2.0 is identified by bits 2-3 of byte 7 being == 2
    if (header.size() < 8) return false;
    return ((static_cast<unsigned char>(header[7]) & 0x0C) == 0x08);
}

QString HeaderDetector::parseMapperInfo(const QByteArray &header)
{
    if (header.size() < 8) return QString();

    unsigned char prgRom = static_cast<unsigned char>(header[4]);
    unsigned char chrRom = static_cast<unsigned char>(header[5]);
    unsigned char flags6 = static_cast<unsigned char>(header[6]);
    unsigned char flags7 = static_cast<unsigned char>(header[7]);

    int mapper = (flags7 & 0xF0) | ((flags6 & 0xF0) >> 4);
    bool battery = (flags6 & 0x02) != 0;
    bool trainer = (flags6 & 0x04) != 0;

    return QString("PRG: %1KB, CHR: %2KB, Mapper: %3%4%5")
        .arg(prgRom * 16)
        .arg(chrRom * 8)
        .arg(mapper)
        .arg(battery ? ", Battery" : "")
        .arg(trainer ? ", Trainer" : "");
}

HeaderInfo HeaderDetector::detectLynx(const QByteArray &data)
{
    HeaderInfo info;

    if (data.size() < 64) {
        return info;
    }

    // Check for LYNX magic
    if (data[0] == 'L' && data[1] == 'Y' && data[2] == 'N' && data[3] == 'X') {
        info.hasHeader = true;
        info.headerSize = 64;
        info.headerType = "Lynx";
        info.headerData = data.left(64);
        info.systemHint = "Atari Lynx";
        info.valid = true;

        // Parse Lynx header for game name (bytes 10-41)
        QString gameName = QString::fromLatin1(data.mid(10, 32)).trimmed();
        info.info = QString("Game: %1").arg(gameName);
    }

    return info;
}

HeaderInfo HeaderDetector::detectSNES(const QByteArray &data, qint64 fileSize)
{
    HeaderInfo info;

    // SMC/SWC copier headers are 512 bytes
    // They're only present if file size is not a clean power of 2
    // Standard SNES ROM sizes: 256KB, 512KB, 1MB, 2MB, 4MB, etc.

    if (fileSize == 0) {
        // Can't detect without file size
        return info;
    }

    // Check if file has a 512-byte header
    // ROM data after header should be power of 2
    qint64 romSize = fileSize - 512;
    
    // Check if romSize is a power of 2 (and reasonable SNES size)
    if (romSize >= 262144 && (romSize & (romSize - 1)) == 0) {
        // Likely has SMC header
        info.hasHeader = true;
        info.headerSize = 512;
        info.headerType = "SMC";
        info.headerData = data.left(512);
        info.systemHint = "SNES";
        info.valid = true;
        info.info = QString("Copier header detected, ROM size: %1KB").arg(romSize / 1024);
    } else if (fileSize > 0 && (fileSize & (fileSize - 1)) == 0) {
        // File size is power of 2, no header
        info.hasHeader = false;
    }

    return info;
}

HeaderInfo HeaderDetector::detectFDS(const QByteArray &data)
{
    HeaderInfo info;

    if (data.size() < 16) {
        return info;
    }

    // Check for fwNES FDS header magic: "FDS\x1A"
    if (data[0] == 'F' && data[1] == 'D' && data[2] == 'S' && data[3] == 0x1A) {
        info.hasHeader = true;
        info.headerSize = 16;
        info.headerType = "fwNES FDS";
        info.headerData = data.left(16);
        info.systemHint = "Famicom Disk System";
        info.valid = true;

        unsigned char diskSides = static_cast<unsigned char>(data[4]);
        info.info = QString("Disk sides: %1").arg(diskSides);
    }

    return info;
}

HeaderInfo HeaderDetector::detectA78(const QByteArray &data)
{
    HeaderInfo info;

    if (data.size() < 128) {
        return info;
    }

    // A78 header starts with 1-byte header version, then "ATARI7800"
    // Modern A78 v3 format has header at offset 1
    if (data.mid(1, 9) == "ATARI7800") {
        info.hasHeader = true;
        info.headerSize = 128;
        info.headerType = "A78";
        info.headerData = data.left(128);
        info.systemHint = "Atari 7800";
        info.valid = true;

        // Game title is at offset 17, 32 bytes
        QString title = QString::fromLatin1(data.mid(17, 32)).trimmed();
        info.info = QString("Title: %1").arg(title);
    }

    return info;
}

bool HeaderDetector::stripHeader(const QString &inputPath, const QString &outputPath)
{
    HeaderInfo headerInfo = detect(inputPath);
    
    if (!headerInfo.hasHeader) {
        // No header to strip - just copy the file
        return QFile::copy(inputPath, outputPath);
    }

    QFile inputFile(inputPath);
    if (!inputFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open input file:" << inputPath;
        return false;
    }

    // Skip header
    inputFile.seek(headerInfo.headerSize);
    QByteArray romData = inputFile.readAll();
    inputFile.close();

    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to create output file:" << outputPath;
        return false;
    }

    qint64 written = outputFile.write(romData);
    outputFile.close();

    return written == romData.size();
}

QByteArray HeaderDetector::getHeaderlessData(const QString &filePath)
{
    HeaderInfo headerInfo = detect(filePath);
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }

    if (headerInfo.hasHeader && headerInfo.headerSize > 0) {
        file.seek(headerInfo.headerSize);
    }

    return file.readAll();
}

bool HeaderDetector::mayHaveHeader(const QString &extension)
{
    static const QStringList headeredFormats = {
        ".nes", ".unf",   // iNES
        ".lnx",           // Lynx
        ".smc",           // SNES copier
        ".fds",           // FDS
        ".a78"            // Atari 7800
    };

    QString ext = extension.toLower();
    if (!ext.startsWith('.')) {
        ext = "." + ext;
    }

    return headeredFormats.contains(ext);
}

int HeaderDetector::getExpectedHeaderSize(const QString &extension)
{
    static const QMap<QString, int> headerSizes = {
        {".nes", 16},
        {".unf", 16},
        {".lnx", 64},
        {".smc", 512},
        {".fds", 16},
        {".a78", 128}
    };

    QString ext = extension.toLower();
    if (!ext.startsWith('.')) {
        ext = "." + ext;
    }

    return headerSizes.value(ext, 0);
}

} // namespace Remus
