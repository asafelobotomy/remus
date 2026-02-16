#pragma once

#include <QString>
#include <QStringList>

namespace Remus {
namespace Constants {
namespace Templates {

namespace Variables {
inline const QString TITLE = QStringLiteral("title");
inline const QString REGION = QStringLiteral("region");
inline const QString LANGUAGES = QStringLiteral("languages");
inline const QString VERSION = QStringLiteral("version");
inline const QString STATUS = QStringLiteral("status");
inline const QString ADDITIONAL = QStringLiteral("additional");
inline const QString TAGS = QStringLiteral("tags");
inline const QString DISC = QStringLiteral("disc");
inline const QString YEAR = QStringLiteral("year");
inline const QString PUBLISHER = QStringLiteral("publisher");
inline const QString SYSTEM = QStringLiteral("system");
inline const QString EXT = QStringLiteral("ext");
inline const QString ID = QStringLiteral("id");
}

inline const QString DEFAULT_SIMPLE = QStringLiteral("{title} ({region})");
inline const QString DEFAULT_NO_INTRO = QStringLiteral("{title} ({region}) ({languages}) ({version}) ({status}) ({additional}) [{tags}]{ext}");
inline const QString DEFAULT_REDUMP = QStringLiteral("{title} ({region}) ({version}) ({additional}) (Disc {disc}){ext}");

inline const QStringList ALL_VARIABLES = {
    Variables::TITLE,
    Variables::REGION,
    Variables::LANGUAGES,
    Variables::VERSION,
    Variables::STATUS,
    Variables::ADDITIONAL,
    Variables::TAGS,
    Variables::DISC,
    Variables::YEAR,
    Variables::PUBLISHER,
    Variables::SYSTEM,
    Variables::EXT,
    Variables::ID
};

inline const QString VARIABLE_HINT = QStringLiteral(
    "Variables: {title}, {region}, {languages}, {version}, {status}, {additional}, {tags}, {disc}, {year}, {publisher}, {system}, {ext}, {id}");

inline bool isValidVariable(const QString &variable) {
    return ALL_VARIABLES.contains(variable);
}

} // Templates
} // Constants
} // Remus
