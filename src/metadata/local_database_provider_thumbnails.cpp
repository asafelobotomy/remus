#include "local_database_provider.h"
#include <QRegularExpression>
#include <QUrl>

namespace Remus {

QString LocalDatabaseProvider::sanitizeThumbnailName(const QString &gameName)
{
    // Per libretro convention: &*/:\<>?\| are replaced with _
    QString sanitized = gameName;
    static const QString invalidChars = QStringLiteral("&*/:\\<>?|\"");
    for (QChar ch : invalidChars) {
        sanitized.replace(ch, QLatin1Char('_'));
    }
    return sanitized;
}

QString LocalDatabaseProvider::stripLanguageTags(const QString &gameName)
{
    // Matches parenthetical groups that contain only ISO 639-1 language codes
    // e.g. (En), (En,Ja), (En,Fr,De,Es,It), (Ja)
    // Pattern: ( OptWS  Code  {, Code}*  OptWS )
    static const QRegularExpression langTagRe(
        QStringLiteral("\\s*\\(\\s*(?:[A-Z][a-z])(?:,\\s*[A-Z][a-z])*\\s*\\)"));

    QString result = gameName;
    result.remove(langTagRe);
    return result.trimmed();
}

QStringList LocalDatabaseProvider::generateThumbnailCandidates(const QString &systemName,
                                                                const QString &gameName,
                                                                const QString &type)
{
    QStringList candidates;
    QSet<QString> seen;

    auto addCandidate = [&](const QString &name) {
        QString url = buildThumbnailUrl(systemName, name, type);
        if (!seen.contains(url)) {
            seen.insert(url);
            candidates.append(url);
        }
    };

    // 1. Exact DAT name (most specific)
    addCandidate(gameName);

    // 2. Language tags stripped — CDN often omits (En), (En,Ja) etc.
    QString stripped = stripLanguageTags(gameName);
    if (stripped != gameName) {
        addCandidate(stripped);
    }

    return candidates;
}

QString LocalDatabaseProvider::buildThumbnailUrl(const QString &systemName,
                                                  const QString &gameName,
                                                  const QString &type)
{
    // URL: https://thumbnails.libretro.com/{System}/{Type}/{SanitizedName}.png
    // Path components are percent-encoded (spaces → %20, etc.)
    QString sanitized = sanitizeThumbnailName(gameName);
    QString path = systemName + QLatin1Char('/') + type + QLatin1Char('/') + sanitized + QStringLiteral(".png");

    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setHost(QStringLiteral("thumbnails.libretro.com"));
    url.setPath(QLatin1Char('/') + path, QUrl::DecodedMode);
    return url.toString(QUrl::FullyEncoded);
}

} // namespace Remus
