#include "hasher.h"
#include <QFile>
#include <QCryptographicHash>
#include <QDebug>
#include <zlib.h>

namespace Remus {

Hasher::Hasher(QObject *parent)
    : QObject(parent) { }

HashResult Hasher::calculateHashes(const QString &filePath, bool stripHeader, int headerSize) {
    HashResult result;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.success = false;
        result.error = QStringLiteral("Failed to open file: ") + filePath;
        return result;
    }

    if (stripHeader && headerSize > 0)
        file.seek(headerSize);

    uLong crc = crc32(0L, Z_NULL, 0);
    QCryptographicHash md5Hash(QCryptographicHash::Md5);
    QCryptographicHash sha1Hash(QCryptographicHash::Sha1);

    static constexpr qint64 kChunkSize = 65536; // 64 KB read buffer
    QByteArray chunk(kChunkSize, Qt::Uninitialized);

    while (!file.atEnd()) {
        const qint64 bytesRead = file.read(chunk.data(), kChunkSize);
        if (bytesRead < 0) {
            result.success = false;
            result.error = QStringLiteral("Read error on: ") + filePath;
            return result;
        }
        if (bytesRead == 0)
            break;
        crc = crc32(crc, reinterpret_cast<const Bytef *>(chunk.constData()), static_cast<uInt>(bytesRead));
        md5Hash.addData(QByteArrayView(chunk.constData(), bytesRead));
        sha1Hash.addData(QByteArrayView(chunk.constData(), bytesRead));
    }

    result.crc32 = QString("%1").arg(crc, 8, 16, QChar('0')).toLower();
    result.md5 = QString(md5Hash.result().toHex()).toLower();
    result.sha1 = QString(sha1Hash.result().toHex()).toLower();
    result.success = true;
    return result;
}

QString Hasher::calculateHash(const QString &filePath, const QString &algorithm, bool stripHeader, int headerSize) {
    const HashResult all = calculateHashes(filePath, stripHeader, headerSize);
    if (!all.success)
        return QString();

    if (algorithm == QStringLiteral("CRC32"))
        return all.crc32;
    if (algorithm == QStringLiteral("MD5"))
        return all.md5;
    if (algorithm == QStringLiteral("SHA1"))
        return all.sha1;

    return QString();
}

int Hasher::detectHeaderSize(const QString &filePath, const QString &extension) {
    if (extension == ".nes") {
        // iNES header detection
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray header = file.read(4);
            file.close();

            // Check for "NES\x1A" magic
            if (header.size() == 4 && header[0] == 'N' && header[1] == 'E' && header[2] == 'S' && header[3] == 0x1A) {
                return 16; // iNES header is 16 bytes
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

    return 0; // No header
}

} // namespace Remus
