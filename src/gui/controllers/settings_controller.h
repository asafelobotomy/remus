#pragma once

#include <QObject>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantList>

namespace Remus {

namespace GuiSettings {
    inline constexpr const char *DEFAULT_LIBRARY_PATH = "gui/default_library_path";
    inline constexpr const char *ROM_SOURCE_DIRECTORY = "gui/rom_source_directory";
    inline constexpr const char *ORGANIZE_DESTINATION = "gui/organize_destination";
    inline constexpr const char *ARTWORK_CACHE_DIR = "gui/artwork_cache_dir";
    inline constexpr const char *MOD_CATALOG_URL = "mods/catalog_url";
    inline constexpr const char *CHDMAN_PATH = "tools/chdman_path";
    inline constexpr const char *DOLPHIN_TOOL_PATH = "tools/dolphin_tool_path";
    inline constexpr const char *MAXCSO_PATH = "tools/maxcso_path";
    inline constexpr const char *WIT_PATH = "tools/wit_path";
    inline constexpr const char *PSXPACKAGER_PATH = "tools/psxpackager_path";
    inline constexpr const char *FLIPS_PATH = "tools/flips_path";
    inline constexpr const char *XDELTA3_PATH = "tools/xdelta3_path";
    inline constexpr const char *PPF_PATH = "tools/ppf_path";
} // namespace GuiSettings

class SettingsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList providerFields READ providerFields CONSTANT)
    Q_PROPERTY(QVariantList providerGroups READ providerGroups CONSTANT)
    Q_PROPERTY(QVariantList toolFields READ toolFields CONSTANT)

public:
    explicit SettingsController(QObject *parent = nullptr);

    QVariantList providerFields() const;
    QVariantList providerGroups() const;
    QVariantList toolFields() const;

    Q_INVOKABLE QString stringValue(const QString &key, const QString &defaultValue = QString()) const;
    Q_INVOKABLE bool boolValue(const QString &key, bool defaultValue = false) const;
    Q_INVOKABLE QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);
    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE void autoDetectTools();
    Q_INVOKABLE QString authenticateProvider(const QString &groupKey);

signals:
    void settingsChanged();
    void settingsError(const QString &message);

private:
    void migrateLegacySecrets();

    QSettings m_settings;
};

} // namespace Remus