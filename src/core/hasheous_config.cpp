#include "hasheous_config.h"

#include "constants/api.h"
#include "constants/constants.h"
#include "constants/settings.h"

#include <QSettings>

namespace Remus {

QString normalizeHasheousBaseUrl(const QString &url) {
    QString normalized = url.trimmed();
    while (normalized.endsWith(QLatin1Char('/')))
        normalized.chop(1);
    return normalized;
}

QString resolveHasheousBaseUrl(const QString &overrideUrl) {
    if (!overrideUrl.trimmed().isEmpty())
        return normalizeHasheousBaseUrl(overrideUrl);

    const QByteArray env = qgetenv("REMUS_HASHEOUS_BASE_URL");
    if (!env.isEmpty())
        return normalizeHasheousBaseUrl(QString::fromUtf8(env));

    QSettings settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
        QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    const QString configured
        = settings.value(QString::fromLatin1(Constants::Settings::Providers::HASHEOUS_BASE_URL)).toString().trimmed();
    if (!configured.isEmpty())
        return normalizeHasheousBaseUrl(configured);

    return QString::fromLatin1(Constants::API::HASHEOUS_BASE_URL);
}

} // namespace Remus
