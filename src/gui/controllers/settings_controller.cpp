#include "settings_controller.h"

#include "../../core/constants/constants.h"

namespace Remus {

SettingsController::SettingsController(QObject *parent)
    : QObject(parent)
    , m_settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
                 QString::fromLatin1(Constants::SETTINGS_APPLICATION))
{
}

QVariantList SettingsController::providerFields() const
{
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

QVariantList SettingsController::toolFields() const
{
    const struct ToolField {
        const char *key;
        const char *label;
    } toolFields[] = {
        {GuiSettings::CHDMAN_PATH, "chdman Path"},
        {GuiSettings::DOLPHIN_TOOL_PATH, "dolphin-tool Path"},
        {GuiSettings::MAXCSO_PATH, "maxcso Path"},
        {GuiSettings::WIT_PATH, "wit Path"},
        {GuiSettings::PSXPACKAGER_PATH, "PSXPackager Path"},
        {GuiSettings::FLIPS_PATH, "flips Path"},
        {GuiSettings::XDELTA3_PATH, "xdelta3 Path"},
        {GuiSettings::PPF_PATH, "PPF Tool Path"},
        {GuiSettings::DEFAULT_LIBRARY_PATH, "Default Library Database"},
        {GuiSettings::ARTWORK_CACHE_DIR, "Artwork Cache Directory"},
        {GuiSettings::MOD_CATALOG_URL, "Mod Catalog URL"},
        {Constants::Settings::Organize::NAMING_TEMPLATE, "Naming Template"},
    };

    QVariantList fields;
    for (const ToolField &field : toolFields) {
        QVariantMap item;
        item.insert(QStringLiteral("key"), QString::fromLatin1(field.key));
        item.insert(QStringLiteral("label"), QString::fromLatin1(field.label));
        fields.append(item);
    }
    return fields;
}

QString SettingsController::stringValue(const QString &key, const QString &defaultValue) const
{
    return m_settings.value(key, defaultValue).toString();
}

QVariant SettingsController::value(const QString &key, const QVariant &defaultValue) const
{
    return m_settings.value(key, defaultValue);
}

void SettingsController::setValue(const QString &key, const QVariant &value)
{
    m_settings.setValue(key, value);
    m_settings.sync();
    emit settingsChanged();
}

void SettingsController::resetToDefaults()
{
    const QStringList keys = {
        QString::fromLatin1(Constants::Settings::Providers::SCREENSCRAPER_USERNAME),
        QString::fromLatin1(Constants::Settings::Providers::SCREENSCRAPER_PASSWORD),
        QString::fromLatin1(Constants::Settings::Providers::THEGAMESDB_API_KEY),
        QString::fromLatin1(Constants::Settings::Providers::IGDB_CLIENT_ID),
        QString::fromLatin1(Constants::Settings::Providers::IGDB_CLIENT_SECRET),
        QString::fromLatin1(Constants::Settings::Providers::HASHEOUS_CLIENT_API_KEY),
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
    };

    for (const QString &key : keys) {
        m_settings.remove(key);
    }

    m_settings.setValue(QString::fromLatin1(Constants::Settings::Organize::NAMING_TEMPLATE),
                        Constants::Settings::Defaults::NAMING_TEMPLATE);
    m_settings.sync();
    emit settingsChanged();
}

} // namespace Remus