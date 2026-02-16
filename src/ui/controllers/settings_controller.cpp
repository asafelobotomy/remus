#include "settings_controller.h"
#include "../../core/constants/constants.h"

namespace Remus {

SettingsController::SettingsController(QObject *parent)
    : QObject(parent)
    , m_settings(Constants::SETTINGS_ORGANIZATION, Constants::SETTINGS_APPLICATION)
{
}

QString SettingsController::getSetting(const QString &key, const QString &defaultValue)
{
    return m_settings.value(key, defaultValue).toString();
}

void SettingsController::setSetting(const QString &key, const QString &value)
{
    m_settings.setValue(key, value);
    emit settingsChanged();
}

void SettingsController::setValue(const QString &key, const QVariant &value)
{
    m_settings.setValue(key, value);
    m_settings.sync();
    emit settingsChanged();
}

QVariant SettingsController::getValue(const QString &key, const QVariant &defaultValue)
{
    return m_settings.value(key, defaultValue);
}

bool SettingsController::isFirstRun()
{
    return !m_settings.value("app/first_run_complete", false).toBool();
}

void SettingsController::markFirstRunComplete()
{
    m_settings.setValue("app/first_run_complete", true);
    m_settings.sync();
    emit settingsChanged();
}

QVariantMap SettingsController::getAllSettings()
{
    QVariantMap map;
    for (const QString &key : m_settings.allKeys()) {
        map[key] = m_settings.value(key);
    }
    return map;
}

QVariantMap SettingsController::keys() const
{
    QVariantMap map;
    map["screenscraperUsername"] = Constants::Settings::Providers::SCREENSCRAPER_USERNAME;
    map["screenscraperPassword"] = Constants::Settings::Providers::SCREENSCRAPER_PASSWORD;
    map["screenscraperDevId"] = Constants::Settings::Providers::SCREENSCRAPER_DEVID;
    map["screenscraperDevPassword"] = Constants::Settings::Providers::SCREENSCRAPER_DEVPASSWORD;
    map["thegamesdbApiKey"] = Constants::Settings::Providers::THEGAMESDB_API_KEY;
    map["igdbClientId"] = Constants::Settings::Providers::IGDB_CLIENT_ID;
    map["igdbClientSecret"] = Constants::Settings::Providers::IGDB_CLIENT_SECRET;
    map["metadataProviderPriority"] = Constants::Settings::Metadata::PROVIDER_PRIORITY;
    map["organizeNamingTemplate"] = Constants::Settings::Organize::NAMING_TEMPLATE;
    map["organizeBySystem"] = Constants::Settings::Organize::BY_SYSTEM;
    map["organizePreserveOriginals"] = Constants::Settings::Organize::PRESERVE_ORIGINALS;
    map["performanceHashAlgorithm"] = Constants::Settings::Performance::HASH_ALGORITHM;
    map["performanceParallelHashing"] = Constants::Settings::Performance::PARALLEL_HASHING;
    return map;
}

QVariantMap SettingsController::defaults() const
{
    QVariantMap map;
    map["providerPriority"] = Constants::Settings::Defaults::PROVIDER_PRIORITY;
    map["namingTemplate"] = Constants::Settings::Defaults::NAMING_TEMPLATE;
    map["hashAlgorithm"] = Constants::Settings::Defaults::HASH_ALGORITHM;
    map["organizeBySystem"] = Constants::Settings::Defaults::ORGANIZE_BY_SYSTEM;
    map["preserveOriginals"] = Constants::Settings::Defaults::PRESERVE_ORIGINALS;
    map["parallelHashing"] = Constants::Settings::Defaults::PARALLEL_HASHING;
    map["templateVariableHint"] = Constants::Settings::Defaults::TEMPLATE_VARIABLE_HINT;
    return map;
}

} // namespace Remus
