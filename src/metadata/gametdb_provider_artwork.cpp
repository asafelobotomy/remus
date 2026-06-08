#include "gametdb_provider.h"

#include <QUrl>

namespace Remus {

ArtworkUrls GameTDBProvider::getArtwork(const QString &id) {
    QMutexLocker locker(&m_mutex);
    if (!m_idIndex.contains(id)) {
        return ArtworkUrls();
    }

    const GameTDBEntry &entry = m_idIndex[id];
    const QString platform = cdnPlatformCode(entry.type);
    const QString regionCdn = cdnRegionCode(entry.region);
    if (platform.isEmpty()) {
        return ArtworkUrls();
    }

    ArtworkUrls artwork;
    const bool isPng = (platform == QLatin1String("wii"));
    const QString ext = isPng ? QStringLiteral("png") : QStringLiteral("jpg");
    const QString coverType = isPng ? QStringLiteral("cover") : QStringLiteral("coverHQ");

    artwork.boxFront = QUrl(buildArtworkUrl(platform, coverType, regionCdn, entry.id, ext));
    if (isPng) {
        artwork.boxFull = QUrl(buildArtworkUrl(platform, QStringLiteral("cover3D"), regionCdn, entry.id, ext));
    }
    if (platform == QLatin1String("wii") || platform == QLatin1String("ps3")) {
        const QString discType = isPng ? QStringLiteral("disc") : QStringLiteral("discHQ");
        artwork.boxBack = QUrl(buildArtworkUrl(platform, discType, regionCdn, entry.id, ext));
    }

    return artwork;
}

QString GameTDBProvider::cdnPlatformCode(const QString &gameType) {
    if (gameType == QLatin1String("Wii") || gameType == QLatin1String("WiiWare")
        || gameType == QLatin1String("GameCube") || gameType == QLatin1String("Channel")) {
        return QStringLiteral("wii");
    }
    if (gameType == QLatin1String("DS") || gameType == QLatin1String("DSiWare")) {
        return QStringLiteral("ds");
    }
    if (gameType == QLatin1String("3DS") || gameType == QLatin1String("3DSWare")) {
        return QStringLiteral("3ds");
    }
    if (gameType == QLatin1String("WiiU") || gameType == QLatin1String("WiiUWare")) {
        return QStringLiteral("wiiu");
    }
    if (gameType == QLatin1String("Switch")) {
        return QStringLiteral("switch");
    }
    if (gameType == QLatin1String("PS3")) {
        return QStringLiteral("ps3");
    }
    return QString();
}

QString GameTDBProvider::cdnRegionCode(const QString &gameRegion) {
    if (gameRegion.contains(QLatin1String("NTSC-U")) || gameRegion.contains(QLatin1String("USA"))
        || gameRegion == QLatin1String("ALL")) {
        return QStringLiteral("US");
    }
    if (gameRegion.contains(QLatin1String("NTSC-J")) || gameRegion.contains(QLatin1String("JPN"))
        || gameRegion == QLatin1String("NTSC-J")) {
        return QStringLiteral("JA");
    }
    if (gameRegion.contains(QLatin1String("PAL"))) {
        return QStringLiteral("EN");
    }
    if (gameRegion.contains(QLatin1String("KOR"))) {
        return QStringLiteral("KO");
    }
    return QStringLiteral("US");
}

QString GameTDBProvider::buildArtworkUrl(const QString &platformCode, const QString &artType, const QString &regionCode,
    const QString &gameId, const QString &extension) {
    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setHost(QStringLiteral("art.gametdb.com"));
    url.setPath(QLatin1Char('/') + platformCode + QLatin1Char('/') + artType + QLatin1Char('/') + regionCode
        + QLatin1Char('/') + gameId + QLatin1Char('.') + extension);
    return url.toString(QUrl::FullyEncoded);
}

GameMetadata GameTDBProvider::entryToMetadata(const GameTDBEntry &entry) const {
    GameMetadata metadata;
    metadata.id = entry.id;
    metadata.title = entry.title;
    metadata.system = entry.type;
    metadata.region = entry.region;
    metadata.publisher = entry.publisher;
    metadata.developer = entry.developer;
    metadata.description = entry.synopsis;
    metadata.players = entry.players;
    metadata.providerId = QStringLiteral("gametdb");
    metadata.matchMethod = QStringLiteral("hash");

    if (!entry.genre.isEmpty()) {
        metadata.genres = entry.genre.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (QString &genre : metadata.genres) {
            genre = genre.trimmed();
        }
    }

    if (entry.year > 0) {
        if (entry.month > 0 && entry.day > 0) {
            metadata.releaseDate = QStringLiteral("%1-%2-%3")
                                       .arg(entry.year, 4, 10, QLatin1Char('0'))
                                       .arg(entry.month, 2, 10, QLatin1Char('0'))
                                       .arg(entry.day, 2, 10, QLatin1Char('0'));
        } else {
            metadata.releaseDate = QString::number(entry.year);
        }
    }

    metadata.matchScore = 1.0f;
    metadata.externalIds.insert(QStringLiteral("gametdb"), entry.id);

    const QString platform = cdnPlatformCode(entry.type);
    const QString regionCdn = cdnRegionCode(entry.region);
    if (!platform.isEmpty()) {
        const bool isPng = (platform == QLatin1String("wii"));
        const QString ext = isPng ? QStringLiteral("png") : QStringLiteral("jpg");
        const QString coverType = isPng ? QStringLiteral("cover") : QStringLiteral("coverHQ");
        metadata.boxArtUrl = buildArtworkUrl(platform, coverType, regionCdn, entry.id, ext);
    }

    return metadata;
}

} // namespace Remus