#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantMap>

namespace Remus {

class SettingsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap keys READ keys CONSTANT)
    Q_PROPERTY(QVariantMap defaults READ defaults CONSTANT)
    
public:
    explicit SettingsController(QObject *parent = nullptr);
    
    Q_INVOKABLE QString getSetting(const QString &key, const QString &defaultValue = QString());
    Q_INVOKABLE void setSetting(const QString &key, const QString &value);
    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);
    Q_INVOKABLE QVariant getValue(const QString &key, const QVariant &defaultValue = QVariant());
    Q_INVOKABLE bool isFirstRun();
    Q_INVOKABLE void markFirstRunComplete();
    Q_INVOKABLE QVariantMap getAllSettings();

    QVariantMap keys() const;
    QVariantMap defaults() const;
    
signals:
    void settingsChanged();
    
private:
    QSettings m_settings;
};

} // namespace Remus
