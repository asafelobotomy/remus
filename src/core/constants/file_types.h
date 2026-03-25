#pragma once

#include <QString>

namespace Remus {
namespace Constants {
namespace FileTypes {

inline const QString OFFICIAL = QStringLiteral("official");
inline const QString HACK = QStringLiteral("hack");
inline const QString TRANSLATION = QStringLiteral("translation");
inline const QString IMPROVEMENT = QStringLiteral("improvement");
inline const QString HOMEBREW = QStringLiteral("homebrew");
inline const QString PROTOTYPE = QStringLiteral("prototype");

inline QString normalize(const QString &fileType)
{
    return fileType.trimmed().toLower();
}

inline bool isOfficial(const QString &fileType)
{
    return normalize(fileType).isEmpty() || normalize(fileType) == OFFICIAL;
}

inline bool isPatchedVariant(const QString &fileType)
{
    return !isOfficial(fileType);
}

} // FileTypes
} // Constants
} // Remus