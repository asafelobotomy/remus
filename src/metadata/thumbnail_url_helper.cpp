#include "thumbnail_url_helper.h"

#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
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

    QString ThumbnailUrlHelper::libretroFolderForAssetType(const QString &assetType) {
        if (assetType == QStringLiteral("box")) {
            return QStringLiteral("Named_Boxarts");
        }
        if (assetType == QStringLiteral("snap")) {
            return QStringLiteral("Named_Snaps");
        }
        if (assetType == QStringLiteral("title")) {
            return QStringLiteral("Named_Titles");
        }
        if (assetType == QStringLiteral("logo")) {
            return QStringLiteral("Named_Logos");
        }
        return { };
    }

    QString ThumbnailUrlHelper::resolveStoragePath(const QString &repoRoot, const QString &storagePath) {
        if (storagePath.isEmpty()) {
            return { };
        }
        QFileInfo info(storagePath);
        if (info.isAbsolute()) {
            return info.absoluteFilePath();
        }
        return QDir(repoRoot).filePath(storagePath);
    }

    QString ThumbnailUrlHelper::repoRootFromCompendiumDb(const QString &databasePath) {
        QFileInfo info(databasePath);
        return QDir::cleanPath(QDir(info.absolutePath()).absoluteFilePath(QStringLiteral("../..")));
    }

    QString ThumbnailUrlHelper::lookupGameAssetPath(QSqlDatabase &db, const QString &gameId, const QString &assetType) {
        if (!db.isOpen() || gameId.isEmpty()) {
            return { };
        }
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT storage_path FROM game_assets WHERE game_id = ? AND asset_type = ?"));
        q.addBindValue(gameId);
        q.addBindValue(assetType);
        if (!q.exec() || !q.next()) {
            return { };
        }
        return q.value(0).toString();
    }

    QString ThumbnailUrlHelper::resolveArtworkUrl(QSqlDatabase &db, const QString &repoRoot, const QString &gameId,
        const QString &systemName, const QString &canonicalTitle, const QString &assetType, bool strictOffline) {
        const QString folder = libretroFolderForAssetType(assetType);
        if (folder.isEmpty()) {
            return { };
        }

        const QString assetPath = lookupGameAssetPath(db, gameId, assetType);
        if (!assetPath.isEmpty()) {
            const QString abs = resolveStoragePath(repoRoot, assetPath);
            if (QFileInfo::exists(abs)) {
                return QUrl::fromLocalFile(abs).toString();
            }
        }

        if (assetType == QStringLiteral("box")) {
            QSqlQuery coverQ(db);
            coverQ.prepare(QStringLiteral("SELECT cover_url FROM games WHERE game_id = ?"));
            coverQ.addBindValue(gameId);
            if (coverQ.exec() && coverQ.next()) {
                const QString coverUrl = coverQ.value(0).toString();
                if (coverUrl.startsWith(QStringLiteral("data/remus-thumbnails/"))) {
                    const QString abs = resolveStoragePath(repoRoot, coverUrl);
                    if (QFileInfo::exists(abs)) {
                        return QUrl::fromLocalFile(abs).toString();
                    }
                }
            }
        }

        if (strictOffline) {
            return { };
        }

        const QStringList candidates = generateThumbnailCandidates(systemName, canonicalTitle, folder);
        return candidates.isEmpty() ? QString() : candidates.first();
    }

} // namespace Metadata
} // namespace Remus
