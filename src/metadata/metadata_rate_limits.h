#pragma once

#include <QString>

namespace Remus {

/**
 * @brief Resolve per-provider HTTP rate-limit intervals from QSettings.
 *
 * Keys (optional):
 *   metadata/rate_limit_ms          — global override for all providers
 *   metadata/rate_limit/<provider>  — per-provider override (e.g. hasheous)
 */
int configuredRateLimitMs(const QString &providerKey, int defaultMs);

} // namespace Remus
