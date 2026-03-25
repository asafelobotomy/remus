#pragma once

#include <QString>

#include "exports.h"

namespace Remus {
namespace Constants {
namespace Cli {

inline constexpr const char *APPLICATION_NAME = "remus-cli";

namespace Options {
inline const QString JSON = QStringLiteral("json");
inline const QString MOD_JSON = QStringLiteral("mod-json");
inline const QString MIN_CONFIDENCE = QStringLiteral("min-confidence");
inline const QString INTERACTIVE = QStringLiteral("interactive");
inline const QString NO_INTERACTIVE = QStringLiteral("no-interactive");
inline const QString DRY_RUN_ALL = QStringLiteral("dry-run-all");
inline const QString EXPORT = QStringLiteral("export");
inline const QString EXPORT_PATH = QStringLiteral("export-path");
inline const QString EXPORT_SYSTEMS = QStringLiteral("export-systems");
inline const QString PROVIDER = QStringLiteral("provider");
}

namespace Defaults {
inline constexpr const char *PROVIDER = "auto";
inline constexpr const char *CHD_CODEC = "auto";
inline constexpr const char *BUNDLE_FORMAT = "zip";
inline constexpr const char *BUNDLE_DISC_FORMAT = "original";
inline constexpr const char *PATCH_FORMAT = "bps";
inline const QString EXPORT_FORMAT = Exports::Formats::CSV;
}

} // Cli
} // Constants
} // Remus