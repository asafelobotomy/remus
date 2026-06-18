#pragma once

#include <QString>

namespace Remus {

/// Normalize base URL (trim trailing slashes).
QString normalizeHasheousBaseUrl(const QString &url);

/**
 * @brief Resolve Hasheous API base URL.
 *
 * Priority: explicit override → @c REMUS_HASHEOUS_BASE_URL → settings
 * @c hasheous/base_url → default @c https://hasheous.org .
 */
QString resolveHasheousBaseUrl(const QString &overrideUrl = QString());

} // namespace Remus
