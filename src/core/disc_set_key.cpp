#include "disc_set_key.h"

#include "disc_title_parser.h"

#include <QCryptographicHash>

namespace Remus {

QString DiscSetKey::computeFromParsed(int systemId, const QString &identityBase, const QString &regionCode) {
    const QString base = identityBase.trimmed().toLower();
    const QString region = regionCode.trimmed().toLower();
    const QString seed = QString::number(systemId) + QLatin1Char('|') + base + QLatin1Char('|') + region;
    return QString::fromLatin1(QCryptographicHash::hash(seed.toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
}

QString DiscSetKey::compute(int systemId, const QString &title, const QString &regionCode) {
    const DiscTitleInfo parsed = DiscTitleParser::parseTitle(title);
    const QString identityBase = parsed.identityBase.isEmpty() ? parsed.baseTitle : parsed.identityBase;
    return computeFromParsed(systemId, identityBase, regionCode);
}

QString DiscSetKey::legacyLibraryGroupKey(const QString &labelPath, const QString &systemDisplayName) {
    return DiscTitleParser::extractBaseTitle(labelPath) + QLatin1Char('|') + systemDisplayName.trimmed();
}

} // namespace Remus
