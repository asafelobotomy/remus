#include "settings_controller.h"

#include "../../core/constants/constants.h"
#include "services/secret_store.h"

namespace Remus {

namespace {
    static bool isSecretKey(const QString &key) {
        for (const char *k : Constants::Settings::Providers::ALL_SECRET_KEYS) {
            if (key == QString::fromLatin1(k))
                return true;
        }
        return false;
    }
} // namespace

SettingsController::SettingsController(QObject *parent)
    : QObject(parent)
    , m_settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
          QString::fromLatin1(Constants::SETTINGS_APPLICATION)) { }

QVariantList SettingsController::providerFields() const {
    QVariantList fields;
    for (const auto &field : Constants::ALL_PROVIDER_FIELDS) {
        QVariantMap item;
        item.insert(QStringLiteral("key"), QString::fromLatin1(field.key));
        item.insert(QStringLiteral("label"), QString::fromLatin1(field.label));
        item.insert(QStringLiteral("password"), field.isPassword);
        fields.append(item);
    }
    return fields;
}

QVariantList SettingsController::toolFields() const {
    // browsable: show Browse button.  isDirectory: use FolderDialog instead of FileDialog.
    const struct ToolField {
        const char *key;
        const char *label;
        bool browsable;
        bool isDirectory;
    } toolFields[] = {
        { GuiSettings::CHDMAN_PATH, "chdman Path", true, false },
        { GuiSettings::DOLPHIN_TOOL_PATH, "dolphin-tool Path", true, false },
        { GuiSettings::MAXCSO_PATH, "maxcso Path", true, false },
        { GuiSettings::WIT_PATH, "wit Path", true, false },
        { GuiSettings::PSXPACKAGER_PATH, "PSXPackager Path", true, false },
        { GuiSettings::FLIPS_PATH, "flips Path", true, false },
        { GuiSettings::XDELTA3_PATH, "xdelta3 Path", true, false },
        { GuiSettings::PPF_PATH, "PPF Tool Path", true, false },
        { GuiSettings::DEFAULT_LIBRARY_PATH, "Default Library Database", true, false },
        { GuiSettings::ARTWORK_CACHE_DIR, "Artwork Cache Directory", true, true },
        { GuiSettings::MOD_CATALOG_URL, "Mod Catalog URL", false, false },
        { Constants::Settings::Organize::NAMING_TEMPLATE, "Naming Template", false, false },
    };

    QVariantList fields;
    for (const ToolField &field : toolFields) {
        QVariantMap item;
        item.insert(QStringLiteral("key"), QString::fromLatin1(field.key));
        item.insert(QStringLiteral("label"), QString::fromLatin1(field.label));
        item.insert(QStringLiteral("browsable"), field.browsable);
        item.insert(QStringLiteral("isDirectory"), field.isDirectory);
        fields.append(item);
    }
    return fields;
}

QVariantList SettingsController::providerGroups() const {
    // Each group has: groupKey, groupName, fields[]
    struct GroupDef {
        const char *groupKey;
        const char *groupName;
        // keys of the fields belonging to this group
        QList<int> fieldIndices; // indices into ALL_PROVIDER_FIELDS
    };

    // ALL_PROVIDER_FIELDS order:
    //  0 SCREENSCRAPER_USERNAME
    //  1 SCREENSCRAPER_PASSWORD
    //  2 THEGAMESDB_API_KEY
    //  3 IGDB_CLIENT_ID
    //  4 IGDB_CLIENT_SECRET
    //  5 HASHEOUS_CLIENT_API_KEY
    //  6 RETROACHIEVEMENTS_USERNAME
    //  7 RETROACHIEVEMENTS_API_KEY
    const struct {
        const char *groupKey;
        const char *groupName;
        int from;
        int to;
    } groups[] = {
        { "screenscraper", "ScreenScraper", 0, 1 },
        { "thegamesdb", "TheGamesDB", 2, 2 },
        { "igdb", "IGDB", 3, 4 },
        { "hasheous", "Hasheous", 5, 5 },
        { "retroachievements", "RetroAchievements", 6, 7 },
    };

    QVariantList result;
    const auto &all = Constants::ALL_PROVIDER_FIELDS;

    for (const auto &g : groups) {
        QVariantMap groupMap;
        groupMap.insert(QStringLiteral("groupKey"), QString::fromLatin1(g.groupKey));
        groupMap.insert(QStringLiteral("groupName"), QString::fromLatin1(g.groupName));

        QVariantList fields;
        for (int i = g.from; i <= g.to; ++i) {
            QVariantMap f;
            f.insert(QStringLiteral("key"), QString::fromLatin1(all[i].key));
            f.insert(QStringLiteral("label"), QString::fromLatin1(all[i].label));
            f.insert(QStringLiteral("password"), all[i].isPassword);
            fields.append(f);
        }
        groupMap.insert(QStringLiteral("fields"), fields);
        result.append(groupMap);
    }
    return result;
}

QString SettingsController::authenticateProvider(const QString &groupKey) {
    // Collect required keys per group and validate they are non-empty
    QStringList requiredKeys;
    if (groupKey == QLatin1String("screenscraper")) {
        requiredKeys << QString::fromLatin1(Constants::Settings::Providers::SCREENSCRAPER_USERNAME)
                     << QString::fromLatin1(Constants::Settings::Providers::SCREENSCRAPER_PASSWORD);
    } else if (groupKey == QLatin1String("thegamesdb")) {
        requiredKeys << QString::fromLatin1(Constants::Settings::Providers::THEGAMESDB_API_KEY);
    } else if (groupKey == QLatin1String("igdb")) {
        requiredKeys << QString::fromLatin1(Constants::Settings::Providers::IGDB_CLIENT_ID)
                     << QString::fromLatin1(Constants::Settings::Providers::IGDB_CLIENT_SECRET);
    } else if (groupKey == QLatin1String("hasheous")) {
        requiredKeys << QString::fromLatin1(Constants::Settings::Providers::HASHEOUS_CLIENT_API_KEY);
    } else if (groupKey == QLatin1String("retroachievements")) {
        requiredKeys << QString::fromLatin1(Constants::Settings::Providers::RETROACHIEVEMENTS_USERNAME)
                     << QString::fromLatin1(Constants::Settings::Providers::RETROACHIEVEMENTS_API_KEY);
    } else {
        return QStringLiteral("Unknown provider.");
    }

    for (const QString &key : requiredKeys) {
        if (stringValue(key).trimmed().isEmpty())
            return QStringLiteral("Missing credentials — fill in all fields first.");
    }

    return QStringLiteral("Credentials saved.");
}

QString SettingsController::stringValue(const QString &key, const QString &defaultValue) const {
    if (isSecretKey(key)) {
        const QString secret = SecretStore::read(key);
        // Fall back to legacy plain-settings value for backward compat
        return secret.isEmpty() ? m_settings.value(key, defaultValue).toString() : secret;
    }
    return m_settings.value(key, defaultValue).toString();
}

bool SettingsController::boolValue(const QString &key, bool defaultValue) const {
    const QVariant v = m_settings.value(key, defaultValue);
    if (v.typeId() == QMetaType::Bool)
        return v.toBool();
    // INI files may store as string "true"/"false"
    return v.toString().toLower() == QLatin1String("true");
}

QVariant SettingsController::value(const QString &key, const QVariant &defaultValue) const {
    if (isSecretKey(key)) {
        const QString secret = SecretStore::read(key);
        return secret.isEmpty() ? m_settings.value(key, defaultValue) : QVariant(secret);
    }
    return m_settings.value(key, defaultValue);
}

void SettingsController::setValue(const QString &key, const QVariant &value) {
    if (isSecretKey(key)) {
        const bool ok = SecretStore::write(key, value.toString());
        if (!ok) {
            // Keychain write failed — preserve the existing value and signal an
            // error to the caller.  Do NOT silently discard the credential.
            emit settingsError(QStringLiteral("Failed to save credential to OS keychain for key: ") + key);
            return;
        }
        // Remove any legacy plain-text copy that may have existed before migration
        if (m_settings.contains(key)) {
            m_settings.remove(key);
            m_settings.sync();
        }
        emit settingsChanged();
        return;
    }
    m_settings.setValue(key, value);
    m_settings.sync();
    emit settingsChanged();
}

void SettingsController::resetToDefaults() {
    // Build key list from the authoritative secret-keys array so no secret is silently omitted.
    QStringList keys;
    for (const char *k : Constants::Settings::Providers::ALL_SECRET_KEYS)
        keys.append(QString::fromLatin1(k));
    keys.append({
        QString::fromLatin1(GuiSettings::CHDMAN_PATH),
        QString::fromLatin1(GuiSettings::DOLPHIN_TOOL_PATH),
        QString::fromLatin1(GuiSettings::MAXCSO_PATH),
        QString::fromLatin1(GuiSettings::WIT_PATH),
        QString::fromLatin1(GuiSettings::PSXPACKAGER_PATH),
        QString::fromLatin1(GuiSettings::FLIPS_PATH),
        QString::fromLatin1(GuiSettings::XDELTA3_PATH),
        QString::fromLatin1(GuiSettings::PPF_PATH),
        QString::fromLatin1(GuiSettings::DEFAULT_LIBRARY_PATH),
        QString::fromLatin1(GuiSettings::ARTWORK_CACHE_DIR),
        QString::fromLatin1(GuiSettings::MOD_CATALOG_URL),
    });

    for (const QString &key : keys) {
        m_settings.remove(key);
    }

    // Also remove secrets from the OS keychain
    for (const char *k : Constants::Settings::Providers::ALL_SECRET_KEYS) {
        SecretStore::remove(QString::fromLatin1(k));
    }

    m_settings.setValue(QString::fromLatin1(Constants::Settings::Organize::NAMING_TEMPLATE),
        Constants::Settings::Defaults::NAMING_TEMPLATE);
    m_settings.sync();
    emit settingsChanged();
}

void SettingsController::autoDetectTools() {
    // Map of settings key → executable name to search on PATH
    const struct {
        const char *key;
        const char *exe;
    } tools[] = {
        { GuiSettings::CHDMAN_PATH, "chdman" },
        { GuiSettings::DOLPHIN_TOOL_PATH, "dolphin-tool" },
        { GuiSettings::MAXCSO_PATH, "maxcso" },
        { GuiSettings::WIT_PATH, "wit" },
        { GuiSettings::PSXPACKAGER_PATH, "psxpackager" },
        { GuiSettings::FLIPS_PATH, "flips" },
        { GuiSettings::XDELTA3_PATH, "xdelta3" },
        { GuiSettings::PPF_PATH, "ppf" },
    };

    bool anyFound = false;
    for (const auto &tool : tools) {
        const QString found = QStandardPaths::findExecutable(QString::fromLatin1(tool.exe));
        if (!found.isEmpty()) {
            m_settings.setValue(QString::fromLatin1(tool.key), found);
            anyFound = true;
        }
    }

    if (anyFound) {
        m_settings.sync();
        emit settingsChanged();
    }
}

} // namespace Remus