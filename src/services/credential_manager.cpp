#include "credential_manager.h"

#include "secret_store.h"
#include "../core/constants/constants.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace Remus {
namespace CredentialManager {

    namespace {

        // Maps QSettings key → environment variable name.
        // Keys match Constants::Settings::Providers::* values.
        struct EnvMapping {
            const char *settingsKey;
            const char *envVar;
        };

        static constexpr EnvMapping kEnvTable[] = {
            { "screenscraper/username", "REMUS_SS_USER" },
            { "screenscraper/password", "REMUS_SS_PASS" },
            { "screenscraper/devid", "REMUS_SS_DEVID" },
            { "screenscraper/devpassword", "REMUS_SS_DEVPASS" },
            { "thegamesdb/api_key", "REMUS_TGDB_API_KEY" },
            { "igdb/client_id", "REMUS_IGDB_CLIENT_ID" },
            { "igdb/client_secret", "REMUS_IGDB_CLIENT_SECRET" },
            { "hasheous/client_api_key", "REMUS_HASHEOUS_API_KEY" },
            { "retroachievements/username", "REMUS_RA_USERNAME" },
            { "retroachievements/api_key", "REMUS_RA_API_KEY" },
            { "steamgriddb/api_key", "REMUS_SGDB_API_KEY" },
        };

        static QString fromJsonFile(const QString &key, const QString &jsonFilePath) {
            if (jsonFilePath.isEmpty() || !QFile::exists(jsonFilePath))
                return { };

            QFile f(jsonFilePath);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                return { };

            QJsonParseError err;
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
            f.close();
            if (err.error != QJsonParseError::NoError || !doc.isObject())
                return { };

            // Key is "outer/inner" — split and traverse two levels.
            const int slash = key.indexOf(QLatin1Char('/'));
            if (slash < 0)
                return { };

            const QString outer = key.left(slash);
            const QString inner = key.mid(slash + 1);
            return doc.object().value(outer).toObject().value(inner).toString().trimmed();
        }

        static QString fromEnvVar(const QString &key) {
            for (const auto &m : kEnvTable) {
                if (key == QLatin1String(m.settingsKey)) {
                    const QByteArray val = qgetenv(m.envVar);
                    if (!val.isEmpty())
                        return QString::fromLocal8Bit(val).trimmed();
                }
            }
            // Legacy alias documented in .env.local — prefer REMUS_RA_USERNAME when both set.
            if (key == QLatin1String("retroachievements/username")) {
                const QByteArray legacy = qgetenv("REMUS_RA_USER");
                if (!legacy.isEmpty())
                    return QString::fromLocal8Bit(legacy).trimmed();
            }
            return { };
        }

        static QString fromSettings(const QString &key) {
            QSettings settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
                QString::fromLatin1(Constants::SETTINGS_APPLICATION));
            return settings.value(key).toString().trimmed();
        }

    } // namespace

    QString get(const QString &key, const QString &jsonFilePath) {
        // 1. JSON credentials file (automation / CI override)
        const QString fromJson = fromJsonFile(key, jsonFilePath);
        if (!fromJson.isEmpty())
            return fromJson;

        // 2. Environment variable
        const QString fromEnv = fromEnvVar(key);
        if (!fromEnv.isEmpty())
            return fromEnv;

        // 3. OS keychain (written by GUI settings page or SecretStore::write)
        //    Only fall through to legacy QSettings on explicit NotFound;
        //    a BackendError is already logged by SecretStore::read() and should
        //    not silently downgrade to plaintext storage.
        const SecretStore::ReadResult kr = SecretStore::readWithStatus(key);
        if (kr.status == SecretStore::ReadResult::Status::Found)
            return kr.value;
        if (kr.status == SecretStore::ReadResult::Status::BackendError)
            return { }; // logged by SecretStore; do NOT fall back to plaintext

        // 4. Legacy plain QSettings (last resort — written by older app versions)
        return fromSettings(key);
    }

    QString getFromFile(const QString &key, const QString &jsonFilePath) {
        return fromJsonFile(key, jsonFilePath);
    }

} // namespace CredentialManager
} // namespace Remus
