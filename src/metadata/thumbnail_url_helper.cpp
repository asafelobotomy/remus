#include "thumbnail_url_helper.h"

#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace Remus {
namespace Metadata {

    QString ThumbnailUrlHelper::sanitizeThumbnailName(const QString &name) {
        // Per libretro convention: &*/:\<>?|" are replaced with _
        QString sanitized = name;
        static const QString invalidChars = QStringLiteral("&*/:\\<>?|\"");
        for (QChar ch : invalidChars) {
            sanitized.replace(ch, QLatin1Char('_'));
        }
        return sanitized;
    }

    QString ThumbnailUrlHelper::stripLanguageTags(const QString &name) {
        // Matches parenthetical groups that contain only ISO 639-1 language codes
        // e.g. (En), (En,Ja), (En,Fr,De,Es,It), (Ja)
        static const QRegularExpression langTagRe(
            QStringLiteral("\\s*\\(\\s*(?:[A-Z][a-z])(?:,\\s*[A-Z][a-z])*\\s*\\)"));

        QString result = name;
        result.remove(langTagRe);
        return result.trimmed();
    }

    QString ThumbnailUrlHelper::buildThumbnailUrl(
        const QString &systemName, const QString &gameName, const QString &type) {
        // URL: https://thumbnails.libretro.com/{System}/{Type}/{SanitizedName}.png
        // Path components are percent-encoded (spaces → %20, etc.)
        const QString sanitized = sanitizeThumbnailName(gameName);
        const QString path
            = systemName + QLatin1Char('/') + type + QLatin1Char('/') + sanitized + QStringLiteral(".png");

        QUrl url;
        url.setScheme(QStringLiteral("https"));
        url.setHost(QStringLiteral("thumbnails.libretro.com"));
        url.setPath(QLatin1Char('/') + path, QUrl::DecodedMode);
        return url.toString(QUrl::FullyEncoded);
    }

    QStringList ThumbnailUrlHelper::generateThumbnailCandidates(
        const QString &systemName, const QString &gameName, const QString &type) {
        QStringList candidates;
        QSet<QString> seen;

        auto addCandidate = [&](const QString &name) {
            const QString url = buildThumbnailUrl(systemName, name, type);
            if (!seen.contains(url)) {
                seen.insert(url);
                candidates.append(url);
            }
        };

        // 1. Exact DAT name (most specific)
        addCandidate(gameName);

        // 2. Language tags stripped — CDN often omits (En), (En,Ja) etc.
        const QString stripped = stripLanguageTags(gameName);
        if (stripped != gameName) {
            addCandidate(stripped);
        }

        return candidates;
    }

} // namespace Metadata
} // namespace Remus
