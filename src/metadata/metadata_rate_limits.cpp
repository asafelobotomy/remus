#include "metadata_rate_limits.h"

#include <QSettings>

#include "../core/constants/constants.h"

namespace Remus {

int configuredRateLimitMs(const QString &providerKey, int defaultMs) {
    QSettings settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
        QString::fromLatin1(Constants::SETTINGS_APPLICATION));

    const QString perProviderKey = QStringLiteral("metadata/rate_limit/") + providerKey.trimmed().toLower();
    if (settings.contains(perProviderKey) && !settings.value(perProviderKey).toString().trimmed().isEmpty()) {
        bool ok = false;
        const int value = settings.value(perProviderKey).toInt(&ok);
        if (ok && value >= 0)
            return value;
    }

    if (settings.contains(QStringLiteral("metadata/rate_limit_ms"))
        && !settings.value(QStringLiteral("metadata/rate_limit_ms")).toString().trimmed().isEmpty()) {
        bool ok = false;
        const int global = settings.value(QStringLiteral("metadata/rate_limit_ms")).toInt(&ok);
        if (ok && global >= 0)
            return global;
    }

    return defaultMs;
}

} // namespace Remus
