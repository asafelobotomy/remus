#ifndef REMUS_MOD_CATALOG_PROVIDER_H
#define REMUS_MOD_CATALOG_PROVIDER_H

#include <optional>
#include <QString>
#include <QList>
#include <QUrl>

namespace Remus {

struct ModEntry {
    QString id;
    QString title;
    QString author;
    QString version;
    QString description;
    QString type;         // "translation", "hack", "improvement", "homebrew"
    QString system;
    QString format;       // "ips", "bps", "ups", "xdelta", "ppf"
    QString patchUrl;
    QString patchSha1;
    qint64  patchSize = 0;

    // Base ROM identification (hash-first)
    QString baseCrc32;
    QString baseMd5;
    QString baseSha1;

    // Display metadata
    QString sourceUrl;
    double  rating = 0.0;
    int     downloads = 0;
};

class ModCatalogProvider {
public:
    bool loadFromFile(const QString &path);

    bool loadFromUrl(const QUrl &url, bool forceRefresh = false);

    QList<ModEntry> findModsForRom(const QString &crc32,
                                   const QString &md5,
                                   const QString &sha1) const;

    QList<ModEntry> findModsBySystem(const QString &system) const;

    std::optional<ModEntry> getModById(const QString &id) const;

    const QList<ModEntry> &allMods() const;

    QString lastError() const;

    static QString cacheDir();
    static QString cacheFileForUrl(const QUrl &url);

    static constexpr int kCacheTtlSeconds = 24 * 3600; // 24 hours

private:
    bool loadFromJson(const QByteArray &data);

    QList<ModEntry> m_mods;
    QString m_lastError;
};

} // namespace Remus

#endif // REMUS_MOD_CATALOG_PROVIDER_H
