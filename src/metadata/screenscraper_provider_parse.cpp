#include "screenscraper_provider.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

namespace Remus {

GameMetadata ScreenScraperProvider::parseGameJson(const QByteArray &json)
{
    GameMetadata metadata;

    const QJsonDocument doc = QJsonDocument::fromJson(json);
    const QJsonObject root = doc.object();

    if (!root.contains("response")) {
        return metadata;
    }

    const QJsonObject response = root["response"].toObject();
    const QJsonObject game = response["jeu"].toObject();

    metadata.id = QString::number(game["id"].toInt());
    metadata.providerId = Constants::Providers::SCREENSCRAPER;
    metadata.fetchedAt = QDateTime::currentDateTime();

    if (game.contains("noms")) {
        const QJsonArray names = game["noms"].toArray();
        for (const QJsonValue &nameVal : names) {
            const QJsonObject nameObj = nameVal.toObject();
            const QString region = nameObj["region"].toString();
            if (region == "us" || region == "wor") {
                metadata.title = nameObj["text"].toString();
                break;
            }
        }
        if (metadata.title.isEmpty() && !names.isEmpty()) {
            metadata.title = names[0].toObject()["text"].toString();
        }
    }

    if (game.contains("systeme")) {
        metadata.system = game["systeme"].toObject()["text"].toString();
    }

    if (game.contains("dates")) {
        const QJsonArray dates = game["dates"].toArray();
        if (!dates.isEmpty()) {
            metadata.releaseDate = dates[0].toObject()["text"].toString();
        }
    }

    if (game.contains("developpeur")) {
        metadata.developer = game["developpeur"].toObject()["text"].toString();
    }
    if (game.contains("editeur")) {
        metadata.publisher = game["editeur"].toObject()["text"].toString();
    }

    if (game.contains("genres")) {
        const QJsonArray genres = game["genres"].toArray();
        for (const QJsonValue &genreVal : genres) {
            metadata.genres.append(genreVal.toObject()["text"].toString());
        }
    }

    if (game.contains("joueurs")) {
        metadata.players = game["joueurs"].toObject()["text"].toString().toInt();
    }

    if (game.contains("note")) {
        metadata.rating = game["note"].toObject()["text"].toString().toFloat();
    }

    if (game.contains("synopsis")) {
        const QJsonArray synopsis = game["synopsis"].toArray();
        for (const QJsonValue &synVal : synopsis) {
            const QJsonObject synObj = synVal.toObject();
            if (synObj["langue"].toString() == "en") {
                metadata.description = synObj["text"].toString();
                break;
            }
        }
    }

    const ArtworkUrls artwork = parseArtworkFromGameObject(game);
    if (!artwork.boxFront.isEmpty()) {
        metadata.boxArtUrl = artwork.boxFront.toString();
    }

    return metadata;
}

ArtworkUrls ScreenScraperProvider::parseArtworkJson(const QByteArray &json) const
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    const QJsonObject root = doc.object();

    if (!root.contains("response")) {
        return {};
    }

    const QJsonObject response = root["response"].toObject();
    const QJsonObject game = response["jeu"].toObject();

    return parseArtworkFromGameObject(game);
}

ArtworkUrls ScreenScraperProvider::parseArtworkFromGameObject(const QJsonObject &game) const
{
    ArtworkUrls artwork;

    QJsonArray mediaArray;
    if (game.contains("medias")) {
        if (game["medias"].isArray()) {
            mediaArray = game["medias"].toArray();
        } else if (game["medias"].isObject()) {
            const QJsonObject mediasObj = game["medias"].toObject();
            if (mediasObj.contains("media") && mediasObj["media"].isArray()) {
                mediaArray = mediasObj["media"].toArray();
            }
        }
    }

    for (const QJsonValue &mediaVal : mediaArray) {
        const QJsonObject media = mediaVal.toObject();
        const QString type = media["type"].toString().toLower();
        const QString url = pickArtworkUrl(media);

        if (type.isEmpty() || url.isEmpty()) {
            continue;
        }

        if ((type.contains("box-2d") || type.contains("box2d") || type == "box") && type.contains("back")) {
            if (artwork.boxBack.isEmpty()) {
                artwork.boxBack = QUrl(url);
            }
        } else if (type.contains("box-2d") || type.contains("box2d") || type == "box") {
            if (artwork.boxFront.isEmpty()) {
                artwork.boxFront = QUrl(url);
            }
        } else if (type.contains("box-3d") || type.contains("box3d")) {
            if (artwork.boxFull.isEmpty()) {
                artwork.boxFull = QUrl(url);
            }
        } else if (type.contains("screenshot") || type == "ss" || type.contains("screen")) {
            if (artwork.screenshot.isEmpty()) {
                artwork.screenshot = QUrl(url);
            }
        } else if (type.contains("title")) {
            if (artwork.titleScreen.isEmpty()) {
                artwork.titleScreen = QUrl(url);
            }
        } else if (type.contains("clearlogo")) {
            if (artwork.clearLogo.isEmpty()) {
                artwork.clearLogo = QUrl(url);
            }
        } else if (type.contains("logo") || type.contains("wheel")) {
            if (artwork.logo.isEmpty()) {
                artwork.logo = QUrl(url);
            }
        } else if (type.contains("marquee") || type.contains("banner")) {
            if (artwork.banner.isEmpty()) {
                artwork.banner = QUrl(url);
            }
        }
    }

    return artwork;
}

QString ScreenScraperProvider::pickArtworkUrl(const QJsonObject &media) const
{
    QString url = media["url"].toString();
    if (!url.isEmpty()) {
        return url;
    }

    url = media["url_ori"].toString();
    if (!url.isEmpty()) {
        return url;
    }

    url = media["url_original"].toString();
    if (!url.isEmpty()) {
        return url;
    }

    url = media["url_thumb"].toString();
    if (!url.isEmpty()) {
        return url;
    }

    return media["url_small"].toString();
}

} // namespace Remus