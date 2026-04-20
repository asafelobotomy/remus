#include "system_detector.h"
#include <QtEndian>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace Remus {

QString SystemDetector::detectFromIsoSystemCnf(const QString &path, const QStringList &candidates, QStringList *evidence) const
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        if (evidence) {
            evidence->append(QStringLiteral("system-cnf:open-failed"));
        }
        return QString();
    }

    // --- Stage 1: ISO 9660 PVD-based fast path ---
    // PVD is at sector 16 (byte offset 0x8000).  Root Directory Record is embedded
    // in the PVD at byte 0x9C.  Scanning the root directory for SYSTEM.CNF costs
    // ~3 sector reads (~6 KB) versus the previous 8 MB linear scan.
    constexpr qint64 SECTOR   = 2048;
    constexpr qint64 PVD_BYTE = 16 * SECTOR;  // 0x8000

    bool       usedPvdPath  = false;
    QByteArray syscnfContent;

    if (file.size() > PVD_BYTE + SECTOR && file.seek(PVD_BYTE)) {
        const QByteArray pvd = file.read(SECTOR);
        const bool validPvd  = pvd.size() >= SECTOR
                                && static_cast<unsigned char>(pvd[0]) == 0x01
                                && pvd.mid(1, 5) == QByteArray("CD001", 5);

        if (validPvd) {
            // Root Directory Record at PVD[0x9C].
            // Bytes [+2..+5]: extent sector (LE uint32).  Bytes [+10..+13]: data length.
            constexpr int DR_BASE = 0x9C;
            const quint32 rootSector = qFromLittleEndian<quint32>(
                reinterpret_cast<const uchar *>(pvd.constData() + DR_BASE + 2));
            const quint32 rootLength = qFromLittleEndian<quint32>(
                reinterpret_cast<const uchar *>(pvd.constData() + DR_BASE + 10));

            if (file.seek(static_cast<qint64>(rootSector) * SECTOR)) {
                const qint64 readLen = static_cast<qint64>(qMin(rootLength, quint32(4 * SECTOR)));
                const QByteArray rootDir = file.read(readLen);

                quint32 cnfSector = 0;
                quint32 cnfLength = 0;
                int pos = 0;
                while (pos < rootDir.size()) {
                    const auto recLen = static_cast<unsigned char>(rootDir[pos]);
                    if (recLen == 0) {
                        // Zero padding — advance to next sector boundary.
                        pos = (pos / static_cast<int>(SECTOR) + 1) * static_cast<int>(SECTOR);
                        continue;
                    }
                    if (pos + static_cast<int>(recLen) > rootDir.size()) break;

                    const auto nameLen = static_cast<unsigned char>(rootDir[pos + 32]);
                    if (nameLen > 0 && pos + 33 + nameLen <= rootDir.size()) {
                        QByteArray name = rootDir.mid(pos + 33, nameLen);
                        const int semi = name.indexOf(';');
                        if (semi >= 0) name = name.left(semi);  // strip ";1" version suffix

                        if (name == QByteArray("SYSTEM.CNF", 10)) {
                            cnfSector = qFromLittleEndian<quint32>(
                                reinterpret_cast<const uchar *>(rootDir.constData() + pos + 2));
                            cnfLength = qFromLittleEndian<quint32>(
                                reinterpret_cast<const uchar *>(rootDir.constData() + pos + 10));
                            break;
                        }
                    }
                    pos += recLen;
                }

                if (cnfSector > 0 && file.seek(static_cast<qint64>(cnfSector) * SECTOR)) {
                    constexpr quint32 MAX_CNF = 4096;
                    syscnfContent = file.read(static_cast<qint64>(qMin(cnfLength, MAX_CNF)));
                    usedPvdPath   = true;
                    if (evidence) evidence->append(QStringLiteral("system-cnf:iso9660-pvd-path"));
                }
            }
        }
    }

    // --- Stage 2: Fallback raw scan for non-ISO9660 files (raw BIN tracks, test stubs) ---
    if (!usedPvdPath) {
        file.seek(0);
        constexpr qint64 SCAN_SIZE = 2LL * 1024 * 1024;  // 2 MB (down from 8 MB)
        syscnfContent = file.read(SCAN_SIZE);
        if (evidence) evidence->append(QStringLiteral("system-cnf:raw-scan-fallback"));
    }

    if (syscnfContent.isEmpty()) {
        if (evidence) evidence->append(QStringLiteral("system-cnf:no-content"));
        return QString();
    }

    const QByteArray upper       = syscnfContent.toUpper();
    const bool hasSystemCnf      = usedPvdPath || upper.contains("SYSTEM.CNF");
    // Single backslash matches real SYSTEM.CNF content; also matches test stubs with
    // doubled backslashes since the single-backslash string is a substring of the double.
    const bool hasBoot2          = upper.contains("BOOT2 = CDROM0:\\");
    const bool hasBoot           = upper.contains("BOOT = CDROM:\\");
    const QRegularExpression ps1BootExpr(
        QStringLiteral("BOOT\\s*=\\s*CDROM:\\\\[A-Z]{4}_[0-9]{3}\\.[0-9]{2};1"),
        QRegularExpression::CaseInsensitiveOption);
    const bool hasPs1BootPattern = ps1BootExpr.match(QString::fromLatin1(upper)).hasMatch();

    if (evidence) {
        evidence->append(QStringLiteral("system-cnf:present=") + (hasSystemCnf ? QStringLiteral("true") : QStringLiteral("false")));
        evidence->append(QStringLiteral("system-cnf:boot2=") + (hasBoot2 ? QStringLiteral("true") : QStringLiteral("false")));
        evidence->append(QStringLiteral("system-cnf:boot=") + (hasBoot ? QStringLiteral("true") : QStringLiteral("false")));
        evidence->append(QStringLiteral("system-cnf:ps1-boot-pattern=") + (hasPs1BootPattern ? QStringLiteral("true") : QStringLiteral("false")));
    }

    if (hasBoot2 && candidates.contains(QStringLiteral("PlayStation 2"))) {
        return QStringLiteral("PlayStation 2");
    }

    if ((hasPs1BootPattern || (hasBoot && !hasBoot2)) &&
        candidates.contains(QStringLiteral("PlayStation"))) {
        return QStringLiteral("PlayStation");
    }

    return QString();
}

QString SystemDetector::detectFromIsoHeader(const QString &path, const QStringList &candidates) const
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    // Scoped to PSP UMD filesystem directory markers.  DiscMagicDetector covers
    // PS1/PS2 via fixed-offset license strings; those branches are not duplicated here.
    // 512 KB is sufficient — PSP_GAME appears in the ISO 9660 root directory,
    // which is typically within the first ~300 sectors (< 600 KB).
    if (file.seek(0)) {
        const QByteArray head = file.read(512 * 1024);
        if (!head.isEmpty() && candidates.contains(QStringLiteral("PSP"))) {
            const QByteArray upper = head.toUpper();
            if (upper.contains("PSP_GAME") || upper.contains("UMD_DATA.BIN"))
                return QStringLiteral("PSP");
        }
    }

    return QString();
}

} // namespace Remus
