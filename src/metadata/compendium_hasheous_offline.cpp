#include "compendium_hasheous_offline.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace Remus {
namespace Compendium {

    namespace {

        QString normalizedHashKey(const QString &type, const QString &value) {
            const QString trimmed = value.trimmed().toLower();
            if (trimmed.isEmpty())
                return { };
            return type + QLatin1Char(':') + trimmed;
        }

        QString findDataSubdirLocal(const QString &subdir) {
            const QString appDir = QCoreApplication::applicationDirPath();
            const QString cwd = QDir::currentPath();
            const QString seg = QStringLiteral("data/") + subdir;
            const QStringList candidates = { cwd + "/" + seg, appDir + "/" + seg, appDir + "/../" + seg,
                appDir + "/../../" + seg, appDir + "/../../../" + seg, cwd + "/../" + seg, cwd + "/../../" + seg };
            for (const QString &dir : candidates) {
                if (QDir(dir).exists())
                    return QDir::cleanPath(dir);
            }
            return { };
        }

        QString stringFromJsonValue(const QJsonValue &value) {
            if (value.isString())
                return value.toString().trimmed();
            if (value.isDouble())
                return QString::number(static_cast<qint64>(value.toDouble()));
            return { };
        }

        QString firstGenreFromAttributes(const QJsonObject &attributes) {
            const QJsonValue tagsValue = attributes.value(QStringLiteral("Tags"));
            if (!tagsValue.isObject())
                return { };
            const QJsonValue genreValue = tagsValue.toObject().value(QStringLiteral("GameGenre"));
            if (genreValue.isArray()) {
                const QJsonArray genres = genreValue.toArray();
                for (const QJsonValue &entry : genres) {
                    if (entry.isObject()) {
                        const QString text = entry.toObject().value(QStringLiteral("Text")).toString().trimmed();
                        if (!text.isEmpty())
                            return text;
                    } else if (entry.isString() && !entry.toString().trimmed().isEmpty()) {
                        return entry.toString().trimmed();
                    }
                }
            }
            return { };
        }

        void indexRomHashes(
            const QJsonArray &roms, const HasheousOfflineMatch &match, QHash<QString, HasheousOfflineMatch> &index) {
            for (const QJsonValue &romValue : roms) {
                if (!romValue.isObject())
                    continue;
                const QJsonObject rom = romValue.toObject();
                const struct {
                    const char *jsonKey;
                    const char *type;
                } hashKeys[] = {
                    { "Crc", "crc32" },
                    { "Md5", "md5" },
                    { "Sha1", "sha1" },
                    { "Sha256", "sha256" },
                };
                for (const auto &hk : hashKeys) {
                    const QString key = normalizedHashKey(
                        QString::fromLatin1(hk.type), rom.value(QString::fromLatin1(hk.jsonKey)).toString());
                    if (!key.isEmpty())
                        index.insert(key, match);
                }
            }
        }

        void indexGameJsonObject(const QJsonObject &root, QHash<QString, HasheousOfflineMatch> &index) {
            HasheousOfflineMatch match;
            const QJsonObject attributes = root.value(QStringLiteral("Attributes")).toObject();
            match.description = stringFromJsonValue(attributes.value(QStringLiteral("AIDescription")));
            match.genre = firstGenreFromAttributes(attributes);

            const QJsonArray metadata = root.value(QStringLiteral("Metadata")).toArray();
            for (const QJsonValue &metaValue : metadata) {
                if (!metaValue.isObject())
                    continue;
                const QJsonObject meta = metaValue.toObject();
                const QString source = meta.value(QStringLiteral("Source")).toString();
                const QString id = stringFromJsonValue(meta.value(QStringLiteral("Id")));
                if (id.isEmpty())
                    continue;
                if (source.compare(QStringLiteral("IGDB"), Qt::CaseInsensitive) == 0)
                    match.igdbId = id;
                else if (source.compare(QStringLiteral("RetroAchievements"), Qt::CaseInsensitive) == 0)
                    match.raGameId = id;
            }

            if (match.igdbId.isEmpty() && match.description.isEmpty() && match.genre.isEmpty()
                && match.raGameId.isEmpty())
                return;

            const QJsonArray roms = root.value(QStringLiteral("ROMs")).toArray();
            if (!roms.isEmpty()) {
                indexRomHashes(roms, match, index);
                return;
            }

            const QJsonArray signatureObjects = root.value(QStringLiteral("SignatureDataObjects")).toArray();
            for (const QJsonValue &sigValue : signatureObjects) {
                if (!sigValue.isObject())
                    continue;
                const QJsonObject sig = sigValue.toObject();
                const struct {
                    const char *jsonKey;
                    const char *type;
                } hashKeys[] = {
                    { "Crc", "crc32" },
                    { "Md5", "md5" },
                    { "Sha1", "sha1" },
                    { "Sha256", "sha256" },
                };
                for (const auto &hk : hashKeys) {
                    const QString key = normalizedHashKey(
                        QString::fromLatin1(hk.type), sig.value(QString::fromLatin1(hk.jsonKey)).toString());
                    if (!key.isEmpty())
                        index.insert(key, match);
                }
            }
        }

        QString cacheDbPathForDumpDir(const QString &dumpDir) {
            const QString base = findDataSubdirLocal(QStringLiteral("hasheous"));
            const QString cacheRoot = base.isEmpty() ? dumpDir : base;
            return QDir(cacheRoot).filePath(QStringLiteral("hasheous_offline_index.sqlite"));
        }

        QString dumpDirFingerprint(const QString &dumpDir) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(dumpDir.toUtf8());
            qint64 fileCount = 0;
            QDirIterator it(dumpDir, { QStringLiteral("*.json") }, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QFileInfo info(it.next());
                hash.addData(info.absoluteFilePath().toUtf8());
                hash.addData(":");
                hash.addData(QByteArray::number(info.lastModified().toSecsSinceEpoch()));
                hash.addData("\n");
                ++fileCount;
            }
            hash.addData(QByteArray::number(fileCount));
            return QString::fromLatin1(hash.result().toHex());
        }

        bool loadIndexFromCache(const QString &cachePath, const QString &fingerprint,
            QHash<QString, HasheousOfflineMatch> &index, QString &error) {
            if (!QFileInfo::exists(cachePath))
                return false;

            const QString conn = QStringLiteral("hasheous-offline-cache-") + fingerprint.left(8);
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
            db.setDatabaseName(cachePath);
            if (!db.open()) {
                error = db.lastError().text();
                return false;
            }

            QSqlQuery metaQ(db);
            if (!metaQ.exec(QStringLiteral("SELECT value FROM cache_meta WHERE key = 'dump_fingerprint'"))
                || !metaQ.next() || metaQ.value(0).toString() != fingerprint) {
                db.close();
                QSqlDatabase::removeDatabase(conn);
                return false;
            }

            QSqlQuery q(db);
            if (!q.exec(QStringLiteral(
                    "SELECT hash_key, igdb_id, description, genre, ra_game_id FROM offline_hash_lookup"))) {
                error = q.lastError().text();
                db.close();
                QSqlDatabase::removeDatabase(conn);
                return false;
            }

            while (q.next()) {
                HasheousOfflineMatch match;
                match.igdbId = q.value(1).toString();
                match.description = q.value(2).toString();
                match.genre = q.value(3).toString();
                match.raGameId = q.value(4).toString();
                index.insert(q.value(0).toString(), match);
            }

            db.close();
            QSqlDatabase::removeDatabase(conn);
            return !index.isEmpty();
        }

        bool saveIndexToCache(const QString &cachePath, const QString &fingerprint,
            const QHash<QString, HasheousOfflineMatch> &index, QString &error) {
            if (QFileInfo::exists(cachePath))
                QFile::remove(cachePath);

            const QString conn = QStringLiteral("hasheous-offline-write-") + fingerprint.left(8);
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
            db.setDatabaseName(cachePath);
            if (!db.open()) {
                error = db.lastError().text();
                return false;
            }

            QSqlQuery q(db);
            const QStringList ddl = {
                QStringLiteral("CREATE TABLE cache_meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)"),
                QStringLiteral(
                    "CREATE TABLE offline_hash_lookup ("
                    "hash_key TEXT PRIMARY KEY, igdb_id TEXT, description TEXT, genre TEXT, ra_game_id TEXT)"),
            };
            for (const QString &sql : ddl) {
                if (!q.exec(sql)) {
                    error = q.lastError().text();
                    db.close();
                    QSqlDatabase::removeDatabase(conn);
                    return false;
                }
            }

            if (!db.transaction()) {
                error = db.lastError().text();
                db.close();
                QSqlDatabase::removeDatabase(conn);
                return false;
            }

            q.prepare(QStringLiteral("INSERT INTO cache_meta (key, value) VALUES (?, ?)"));
            q.addBindValue(QStringLiteral("dump_fingerprint"));
            q.addBindValue(fingerprint);
            if (!q.exec()) {
                error = q.lastError().text();
                db.rollback();
                db.close();
                QSqlDatabase::removeDatabase(conn);
                return false;
            }

            q.prepare(QStringLiteral("INSERT INTO offline_hash_lookup "
                                     "(hash_key, igdb_id, description, genre, ra_game_id) VALUES (?, ?, ?, ?, ?)"));
            for (auto it = index.constBegin(); it != index.constEnd(); ++it) {
                q.addBindValue(it.key());
                q.addBindValue(it.value().igdbId);
                q.addBindValue(it.value().description);
                q.addBindValue(it.value().genre);
                q.addBindValue(it.value().raGameId);
                if (!q.exec()) {
                    error = q.lastError().text();
                    db.rollback();
                    db.close();
                    QSqlDatabase::removeDatabase(conn);
                    return false;
                }
            }

            if (!db.commit()) {
                error = db.lastError().text();
                db.close();
                QSqlDatabase::removeDatabase(conn);
                return false;
            }

            db.close();
            QSqlDatabase::removeDatabase(conn);
            return true;
        }

        bool buildIndexFromDumpDir(
            const QString &dumpDir, QHash<QString, HasheousOfflineMatch> &index, QString &error) {
            index.clear();
            int parsedFiles = 0;
            QDirIterator it(dumpDir, { QStringLiteral("*.json") }, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly))
                    continue;
                const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                if (!doc.isObject())
                    continue;
                indexGameJsonObject(doc.object(), index);
                ++parsedFiles;
                if (parsedFiles % 5000 == 0) {
                    qInfo().noquote() << QStringLiteral("[Hasheous offline] indexed %1 dump files (%2 hash keys)...")
                                             .arg(parsedFiles)
                                             .arg(index.size());
                }
            }
            if (index.isEmpty()) {
                error = QStringLiteral("No hash keys indexed from Hasheous dump directory: %1").arg(dumpDir);
                return false;
            }
            qInfo().noquote() << QStringLiteral("[Hasheous offline] Built index from %1 files (%2 hash keys)")
                                     .arg(parsedFiles)
                                     .arg(index.size());
            return true;
        }

    } // namespace

    QString findHasheousDumpDir() {
        const QString dir = findDataSubdirLocal(QStringLiteral("hasheous/dumps"));
        if (dir.isEmpty())
            return { };
        QDirIterator it(dir, { QStringLiteral("*.json") }, QDir::Files, QDirIterator::Subdirectories);
        return it.hasNext() ? dir : QString();
    }

    bool hasHasheousOfflineDumpFiles(const QString &dumpDir) {
        const QString root = dumpDir.isEmpty() ? findHasheousDumpDir() : dumpDir;
        if (root.isEmpty())
            return false;
        QDirIterator it(root, { QStringLiteral("*.json") }, QDir::Files, QDirIterator::Subdirectories);
        return it.hasNext();
    }

    bool loadHasheousOfflineIndex(
        const QString &dumpDir, QHash<QString, HasheousOfflineMatch> &indexByHash, QString &error) {
        indexByHash.clear();
        if (dumpDir.isEmpty())
            return false;

        const QString fingerprint = dumpDirFingerprint(dumpDir);
        const QString cachePath = cacheDbPathForDumpDir(dumpDir);
        if (loadIndexFromCache(cachePath, fingerprint, indexByHash, error))
            return true;

        if (!buildIndexFromDumpDir(dumpDir, indexByHash, error))
            return false;

        QString cacheError;
        if (!saveIndexToCache(cachePath, fingerprint, indexByHash, cacheError))
            qWarning().noquote()
                << QStringLiteral("[Hasheous offline] Cache write failed (non-fatal): %1").arg(cacheError);
        return true;
    }

    bool lookupHasheousOfflineMatch(const QHash<QString, HasheousOfflineMatch> &indexByHash, const QString &crc32,
        const QString &md5, const QString &sha1, const QString &sha256, HasheousOfflineMatch &out) {
        const QStringList keys = {
            normalizedHashKey(QStringLiteral("sha256"), sha256),
            normalizedHashKey(QStringLiteral("sha1"), sha1),
            normalizedHashKey(QStringLiteral("md5"), md5),
            normalizedHashKey(QStringLiteral("crc32"), crc32),
        };
        for (const QString &key : keys) {
            if (key.isEmpty())
                continue;
            const auto it = indexByHash.constFind(key);
            if (it != indexByHash.constEnd()) {
                out = it.value();
                return true;
            }
        }
        return false;
    }

} // namespace Compendium
} // namespace Remus
