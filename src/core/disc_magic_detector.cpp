#include "disc_magic_detector.h"
#include "archive_extractor.h"
#include "constants/systems.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

namespace Remus {

using namespace Constants::Systems;

// Magic byte table derived from RetroArch task_database_cue.c (MIT/GPL-3.0)
// and cross-confirmed against Dolphin DiscUtils.h and PPSSPP Core/Loaders.cpp.
// Order matters: first match wins. More specific entries come first.
const QList<DiscMagicDetector::MagicEntry> &DiscMagicDetector::magicTable()
{
    static const QList<MagicEntry> table = {
        // GameCube disc magic (big-endian 0xC2339F3D at offset 0x1C)
        {ID_GAMECUBE, "GameCube",   "\xC2\x33\x9F\x3D", 4, 0x0001C},
        // Wii disc magic (big-endian 0x5D1C9EA3): standard ISO, WBFS, RVZ/WIA
        {ID_WII,      "Wii",        "\x5D\x1C\x9E\xA3", 4, 0x00018},
        {ID_WII,      "Wii",        "\x5D\x1C\x9E\xA3", 4, 0x00218},  // WBFS
        {ID_WII,      "Wii",        "\x5D\x1C\x9E\xA3", 4, 0x00070},  // RVZ/WIA
        // Sega disc systems: "SEGA SEGAKATANA" (Dreamcast), "SEGA SEGASATURN", "SEGADISCSYSTEM"
        {ID_DREAMCAST, "Dreamcast", "SEGA SEGAKATANA",  15, 0x00010},
        {ID_SEGA_CD,   "Sega CD",   "SEGADISCSYSTEM",   14, 0x00010},
        {ID_SATURN,    "Saturn",     "SEGA SEGASATURN",  15, 0x00010},
        // PSP: "PSP GAME" in ISO 9660 PVD system identifier at 0x8008
        {ID_PSP,      "PSP",        "PSP GAME",          8, 0x08008},
        // PS2: "PLAYSTATION" at DVD offset 0x8008 (most common PS2 ISO format)
        {ID_PS2,      "PlayStation 2", "PLAYSTATION",    11, 0x08008},
        // PS2: "PLAYSTATION" at CD offset 0x9320
        {ID_PS2,      "PlayStation 2", "PLAYSTATION",    11, 0x09320},
        // PS1: "Sony Computer " license string at offset 0x24F8
        {ID_PSX,      "PlayStation", "Sony Computer ",   14, 0x024F8},
    };
    return table;
}

bool DiscMagicDetector::isDiscImageExtension(const QString &extension)
{
    static const QStringList discExts = {
        ".iso", ".bin", ".img", ".cdi", ".cue", ".gdi",
        ".mdf", ".nrg", ".wbfs", ".gcm",
    };
    return discExts.contains(extension.toLower());
}

DiscHeaderInfo DiscMagicDetector::detect(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    const qint64 fileSize = file.size();

    // Read up to 0x10000 (64KB) — enough for all magic offsets.
    // The deepest offset is 0x9320 + 11 = ~0x932B.
    constexpr qint64 PROBE_SIZE = 0x10000;
    QByteArray data = file.read(qMin(fileSize, PROBE_SIZE));
    file.close();

    return detectFromData(data, fileSize);
}

DiscHeaderInfo DiscMagicDetector::detectFromData(const QByteArray &data, qint64 fileSize)
{
    for (const MagicEntry &entry : magicTable()) {
        const qint64 endOffset = entry.offset + entry.magicLen;
        if (data.size() < endOffset) {
            continue;
        }

        if (memcmp(data.constData() + entry.offset, entry.magic, entry.magicLen) == 0) {
            DiscHeaderInfo info;
            info.systemId = entry.systemId;
            info.systemName = QString::fromLatin1(entry.systemName);
            info.detected = true;

            // PS1/PS2 disambiguation: both can match "PLAYSTATION".
            // PPSSPP heuristic: files ≤ 800 MB are PS1, larger are PS2.
            if (entry.systemId == ID_PS2 && fileSize > 0 && fileSize <= 800LL * 1024 * 1024) {
                info.systemId = ID_PSX;
                info.systemName = QStringLiteral("PlayStation");
            }

            // Extract Dreamcast serial/title from IP.BIN if detected
            if (entry.systemId == ID_DREAMCAST) {
                auto dcInfo = scanForDreamcastHeader(data);
                info.serial = dcInfo.serial;
                info.title = dcInfo.title;
                info.releaseDate = dcInfo.releaseDate;
            }

            // GameCube / Wii: Game ID at offset 0x00 (6 bytes, e.g. "G8ME01")
            if (entry.systemId == ID_GAMECUBE || entry.systemId == ID_WII) {
                if (data.size() >= 6) {
                    QByteArray rawId = data.mid(0, 6);
                    // Validate: game IDs are printable ASCII
                    bool valid = true;
                    for (char c : rawId) {
                        if (c < 0x20 || c > 0x7E) { valid = false; break; }
                    }
                    if (valid) {
                        info.serial = QString::fromLatin1(rawId).trimmed();
                    }
                }
            }

            // Sega Saturn: serial at offset 0x20 (10 bytes) in header
            if (entry.systemId == ID_SATURN) {
                if (data.size() >= 0x20 + 10) {
                    info.serial = QString::fromLatin1(data.mid(0x20, 10)).trimmed();
                }
            }

            // Sega CD: serial at offset 0x183 (8 bytes) in header
            if (entry.systemId == ID_SEGA_CD) {
                if (data.size() >= 0x183 + 11) {
                    info.serial = QString::fromLatin1(data.mid(0x183, 11)).trimmed();
                }
            }

            // PSX / PS2: serial extracted from SYSTEM.CNF (requires filesystem parsing)
            // For now, rely on existing filename-based serial extraction in scanner

            return info;
        }
    }

    return {};
}

DiscHeaderInfo DiscMagicDetector::extractDreamcastHeader(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    // CDI files embed the data track at an unpredictable offset.
    // Strategy: scan the first 256KB for "SEGA SEGAKATANA" magic.
    constexpr qint64 SCAN_SIZE = 256 * 1024;
    QByteArray data = file.read(qMin(file.size(), SCAN_SIZE));
    file.close();

    auto info = scanForDreamcastHeader(data);
    if (!info.detected) {
        // For large CDI files, the data track may start further in.
        // Re-read in 256KB chunks up to 16MB.
        QFile scanFile(filePath);
        if (!scanFile.open(QIODevice::ReadOnly)) return {};

        constexpr qint64 MAX_SCAN = 16LL * 1024 * 1024;
        const qint64 scanLimit = qMin(scanFile.size(), MAX_SCAN);
        qint64 pos = SCAN_SIZE;

        while (pos < scanLimit) {
            scanFile.seek(pos);
            QByteArray chunk = scanFile.read(SCAN_SIZE);
            if (chunk.isEmpty()) break;

            info = scanForDreamcastHeader(chunk);
            if (info.detected) {
                scanFile.close();
                return info;
            }
            // Overlap by 16 bytes to avoid missing a boundary-split magic
            pos += SCAN_SIZE - 16;
        }
        scanFile.close();
    }

    return info;
}

DiscHeaderInfo DiscMagicDetector::scanForDreamcastHeader(const QByteArray &data)
{
    // Scan for "SEGA SEGAKATANA" (15 bytes) — the Dreamcast IP.BIN hardware ID.
    // Once found, that position is offset 0x00 of IP.BIN.
    static const QByteArray MAGIC("SEGA SEGAKATANA", 15);

    int pos = data.indexOf(MAGIC);
    if (pos < 0) {
        return {};
    }

    DiscHeaderInfo info;
    info.systemId = ID_DREAMCAST;
    info.systemName = QStringLiteral("Dreamcast");
    info.detected = true;

    // IP.BIN field offsets (from dreamcast.wiki):
    // 0x040: Product Number (10 bytes) — serial like "HDR-0176"
    // 0x050: Release Date    (16 bytes) — "YYYYMMDD" format
    // 0x080: Game Title     (128 bytes)

    const int serialOffset = pos + 0x040;
    const int dateOffset   = pos + 0x050;
    const int titleOffset  = pos + 0x080;

    if (serialOffset + 10 <= data.size()) {
        info.serial = QString::fromLatin1(data.constData() + serialOffset, 10).trimmed();
        // Truncate at null byte (IP.BIN fields are null-padded)
        int nul = info.serial.indexOf(QChar::fromLatin1('\0'));
        if (nul >= 0) info.serial.truncate(nul);
        info.serial = info.serial.trimmed();
    }

    if (dateOffset + 16 <= data.size()) {
        info.releaseDate = QString::fromLatin1(data.constData() + dateOffset, 16).trimmed();
        int nul = info.releaseDate.indexOf(QChar::fromLatin1('\0'));
        if (nul >= 0) info.releaseDate.truncate(nul);
        info.releaseDate = info.releaseDate.trimmed();
    }

    if (titleOffset + 128 <= data.size()) {
        info.title = QString::fromLatin1(data.constData() + titleOffset, 128).trimmed();
        int nul = info.title.indexOf(QChar::fromLatin1('\0'));
        if (nul >= 0) info.title.truncate(nul);
        info.title = info.title.trimmed();
    }

    return info;
}

DiscHeaderInfo DiscMagicDetector::detectFromArchive(const QString &archivePath,
                                                     const QString &memberPath,
                                                     qint64 memberSize)
{
    // 64 KB covers the deepest known magic offset (PS2 at 0x9320 + 11 ≈ 37.6 KB).
    // Reading more wastes I/O; reading less would miss PS1/PS2 signatures.
    static constexpr qint64 PROBE_SIZE = 0x10000;
    ArchiveExtractor extractor;
    const QByteArray data = extractor.readMemberPrefix(archivePath, memberPath, PROBE_SIZE);
    if (data.isEmpty()) return {};
    return detectFromData(data, memberSize);
}

} // namespace Remus
