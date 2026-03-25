#pragma once

#include <QString>
#include <QStringList>

namespace Remus {
namespace Constants {
namespace Files {

inline const QString ZIP = QStringLiteral(".zip");
inline const QString SEVEN_Z = QStringLiteral(".7z");
inline const QString RAR = QStringLiteral(".rar");
inline const QString GZ = QStringLiteral(".gz");
inline const QString TAR = QStringLiteral(".tar");
inline const QString BZ2 = QStringLiteral(".bz2");
inline const QString XZ = QStringLiteral(".xz");
inline const QString TAR_GZ = QStringLiteral(".tar.gz");
inline const QString TGZ = QStringLiteral(".tgz");
inline const QString TAR_BZ2 = QStringLiteral(".tar.bz2");
inline const QString TBZ2 = QStringLiteral(".tbz2");

inline const QString CUE = QStringLiteral(".cue");
inline const QString GDI = QStringLiteral(".gdi");
inline const QString ISO = QStringLiteral(".iso");
inline const QString BIN = QStringLiteral(".bin");
inline const QString IMG = QStringLiteral(".img");
inline const QString MDF = QStringLiteral(".mdf");
inline const QString CCD = QStringLiteral(".ccd");
inline const QString MDS = QStringLiteral(".mds");
inline const QString CDI = QStringLiteral(".cdi");
inline const QString NRG = QStringLiteral(".nrg");
inline const QString RAW = QStringLiteral(".raw");
inline const QString SUB = QStringLiteral(".sub");
inline const QString CHD = QStringLiteral(".chd");
inline const QString M3U = QStringLiteral(".m3u");

inline const QStringList ARCHIVE_EXTENSIONS = {
    ZIP, SEVEN_Z, RAR, GZ, TAR, BZ2, XZ, TAR_GZ, TGZ, TAR_BZ2, TBZ2
};

inline const QStringList CHD_SOURCE_EXTENSIONS = {
    CUE, GDI, ISO, BIN, IMG, MDF, CDI, NRG
};

inline const QStringList COMPRESSIBLE_DISC_EXTENSIONS = {
    CUE, ISO, GDI
};

inline const QStringList EXTRACTABLE_DISC_EXTENSIONS = {
    CHD
};

inline const QStringList ARCHIVE_OPERATION_EXTENSIONS = {
    ZIP, SEVEN_Z, RAR
};

inline const QStringList SPACE_SCAN_EXTENSIONS = {
    CUE, ISO, GDI, BIN, CHD
};

inline const QStringList M3U_SOURCE_EXTENSIONS = {
    CUE, CHD, ISO, GDI
};

inline const QStringList PRIMARY_DISC_EXTENSIONS = {
    CUE, GDI, M3U
};

inline bool containsExtension(const QStringList &extensions, const QString &extension)
{
    return extensions.contains(extension.trimmed().toLower());
}

inline QStringList globPatternsFor(const QStringList &extensions)
{
    QStringList patterns;
    patterns.reserve(extensions.size());

    for (const QString &extension : extensions) {
        patterns.append(QStringLiteral("*") + extension.trimmed().toLower());
    }

    return patterns;
}

inline bool isArchiveExtension(const QString &extension)
{
    return containsExtension(ARCHIVE_EXTENSIONS, extension);
}

inline bool isChdSourceExtension(const QString &extension)
{
    return containsExtension(CHD_SOURCE_EXTENSIONS, extension);
}

inline bool isPrimaryDiscExtension(const QString &extension)
{
    return containsExtension(PRIMARY_DISC_EXTENSIONS, extension);
}

inline int displayPriority(const QString &extension)
{
    const QString normalized = extension.trimmed().toLower();

    if (normalized == CUE) return 0;
    if (normalized == GDI) return 1;
    if (normalized == M3U) return 2;
    if (normalized == ISO) return 3;
    if (normalized == CHD) return 4;
    if (normalized == BIN) return 10;
    if (normalized == IMG) return 11;
    if (normalized == RAW) return 12;

    return 5;
}

} // Files
} // Constants
} // Remus