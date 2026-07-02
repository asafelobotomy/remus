#pragma once

#include <QHash>
#include <QList>
#include <QString>

namespace Remus {
namespace LaunchBoxPlatformMap {

/// Resolve LaunchBox &lt;Platform&gt; strings to Remus system_id values.
/// Returns empty list when no mapping exists (caller may fall back to heuristics).
QList<int> resolveSystemIds(const QString &launchBoxPlatform);

/// Canonical lookup key for platform strings (lowercase alnum).
QString normalizePlatformKey(const QString &platform);

/// Heuristic fallback when explicit mapping is missing.
bool platformKeysCompatible(const QString &remusPlatform, const QString &launchBoxPlatform);

} // namespace LaunchBoxPlatformMap
} // namespace Remus
