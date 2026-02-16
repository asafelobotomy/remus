#pragma once

#include <QString>
#include "templates.h"

namespace Remus {
namespace Constants {
namespace Settings {

namespace Providers {
inline constexpr const char* SCREENSCRAPER_USERNAME = "screenscraper/username";
inline constexpr const char* SCREENSCRAPER_PASSWORD = "screenscraper/password";
inline constexpr const char* SCREENSCRAPER_DEVID = "screenscraper/devid";
inline constexpr const char* SCREENSCRAPER_DEVPASSWORD = "screenscraper/devpassword";
inline constexpr const char* THEGAMESDB_API_KEY = "thegamesdb/api_key";
inline constexpr const char* IGDB_CLIENT_ID = "igdb/client_id";
inline constexpr const char* IGDB_CLIENT_SECRET = "igdb/client_secret";
}

namespace Metadata {
inline constexpr const char* PROVIDER_PRIORITY = "metadata/provider_priority";
inline constexpr const char* PROVIDER_PRIORITY_ORDER = "metadata/provider_priority_order";
inline constexpr const char* PROVIDERS_ENABLED = "metadata/providers_enabled";
}

namespace Organize {
inline constexpr const char* NAMING_TEMPLATE = "organize/naming_template";
inline constexpr const char* BY_SYSTEM = "organize/by_system";
inline constexpr const char* PRESERVE_ORIGINALS = "organize/preserve_originals";
}

namespace Performance {
inline constexpr const char* HASH_ALGORITHM = "performance/hash_algorithm";
inline constexpr const char* PARALLEL_HASHING = "performance/parallel_hashing";
}

namespace Defaults {
inline const QString PROVIDER_PRIORITY = QStringLiteral("ScreenScraper (Primary)");
inline const QString NAMING_TEMPLATE = Templates::DEFAULT_SIMPLE;
inline const QString HASH_ALGORITHM = QStringLiteral("Auto (System Default)");
inline const QString ORGANIZE_BY_SYSTEM = QStringLiteral("true");
inline const QString PRESERVE_ORIGINALS = QStringLiteral("false");
inline const QString PARALLEL_HASHING = QStringLiteral("true");
inline const QString TEMPLATE_VARIABLE_HINT = Templates::VARIABLE_HINT;
}

} // Settings
} // Constants
} // Remus
