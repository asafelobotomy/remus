#include "hasher.h"
#include <QFile>
#include <QCryptographicHash>
#include <QDebug>
#include <zlib.h>

namespace Remus {

Hasher::Hasher(QObject *parent)
    : QObject(parent)
{
}

HashResult Hasher::calculateHashes(const QString &filePath, bool stripHeader, int headerSize)
{
    HashResult result;

    QByteArray data = readFileData(filePath, stripHeader, headerSize);
    if (data.isEmpty()) {
        result.success = false;
        result.error = "Failed to read file or file is empty";
        return result;
    }

    result.crc32 = calculateCRC32(data);
    result.md5 = calculateMD5(data);
    result.sha1 = calculateSHA1(data);
    result.success = true;

    return result;
}

QString Hasher::calculateHash(const QString &filePath, const QString &algorithm,
                               bool stripHeader, int headerSize)
{
    QByteArray data = readFileData(filePath, stripHeader, headerSize);
    if (data.isEmpty()) {
        return QString();
    }

    if (algorithm == "CRC32") {
        return calculateCRC32(data);
    } else if (algorithm == "MD5") {
        return calculateMD5(data);
    } else if (algorithm == "SHA1") {
        return calculateSHA1(data);
    }

    return QString();
}

QByteArray Hasher::readFileData(const QString &filePath, bool stripHeader, int headerSize)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file for hashing:" << filePath;
        return QByteArray();
    }

    if (stripHeader && headerSize > 0) {
        file.seek(headerSize);
    }

    QByteArray data = file.readAll();
    file.close();

    return data;
}

QString Hasher::calculateCRC32(const QByteArray &data)
{
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef*>(data.constData()), data.size());
    return QString("%1").arg(crc, 8, 16, QChar('0')).toLower();
}

QString Hasher::calculateMD5(const QByteArray &data)
{
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Md5);
    return QString(hash.toHex()).toLower();
}

QString Hasher::calculateSHA1(const QByteArray &data)
{
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
    return QString(hash.toHex()).toLower();
}

int Hasher::detectHeaderSize(const QString &filePath, const QString &extension)
{
    if (extension == ".nes") {
        // iNES header detection
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray header = file.read(4);
            file.close();
            
            // Check for "NES\x1A" magic
            if (header.size() == 4 && 
                header[0] == 'N' && 
                header[1] == 'E' && 
                header[2] == 'S' && 
                header[3] == 0x1A) {
                return 16;  // iNES header is 16 bytes
            }
        }
    } else if (extension == ".lnx") {
        // Atari Lynx header is always 64 bytes
        return 64;
    } else if (extension == ".smc") {
        // SNES SMC copier header (rare)
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            qint64 size = file.size();
            file.close();
            
            // If file size is not a power of 2, likely has 512-byte header
            if ((size & (size - 1)) != 0 && (size % 512) == 0) {
                return 512;
            }
        }
    }

    return 0;  // No header
}

} // namespace Remus
