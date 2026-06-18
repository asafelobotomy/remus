#include "chd_header.h"

#include <QByteArray>
#include <QFile>
#include <QtEndian>

namespace Remus {

namespace {

    constexpr int kChdTagSize = 8;
    constexpr int kChdV5MinHeaderSize = 124;
    constexpr int kChdV5Sha1Offset = 64;

    QString sha1HexFromBytes(const QByteArray &bytes) {
        if (bytes.size() != 20 || bytes == QByteArray(20, '\0'))
            return QString();
        return QString::fromLatin1(bytes.toHex());
    }

} // namespace

ChdHeaderDigest readChdHeaderDigest(const QString &chdPath) {
    ChdHeaderDigest result;

    QFile file(chdPath);
    if (!file.open(QIODevice::ReadOnly))
        return result;

    const QByteArray header = file.read(kChdV5MinHeaderSize);
    if (header.size() < kChdV5MinHeaderSize)
        return result;

    if (header.left(kChdTagSize) != QByteArrayLiteral("MComprHD"))
        return result;

    const auto readU32 = [&](int offset) -> quint32 {
        return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(header.constData() + offset));
    };

    result.version = static_cast<int>(readU32(kChdTagSize + 4));
    if (result.version != 5)
        return result;

    result.sha1 = sha1HexFromBytes(header.mid(kChdV5Sha1Offset, 20));
    result.valid = result.sha1.size() == 40;
    return result;
}

} // namespace Remus
