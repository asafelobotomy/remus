#pragma once

#include <QObject>
#include <QVariantList>

#include "../../core/verification_engine.h"

namespace Remus {

class AppController;

class DatManagerController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool importing READ isImporting NOTIFY importingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int total READ total NOTIFY progressChanged)
    Q_PROPERTY(QVariantList loadedDats READ loadedDats NOTIFY loadedDatsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit DatManagerController(AppController *appController, QObject *parent = nullptr);

    bool isImporting() const {
        return m_importing;
    }
    int progress() const {
        return m_progress;
    }
    int total() const {
        return m_total;
    }
    QVariantList loadedDats() const {
        return m_loadedDats;
    }
    QString lastError() const {
        return m_lastError;
    }

    Q_INVOKABLE bool importDat(const QString &path, const QString &systemName);
    Q_INVOKABLE void removeDat(const QString &systemName);
    void refresh();

signals:
    void importingChanged();
    void progressChanged();
    void loadedDatsChanged();
    void lastErrorChanged();

private:
    void setLastError(const QString &message);
    void rebuildLoadedDats();

    AppController *m_appController;
    VerificationEngine m_engine;
    bool m_importing = false;
    int m_progress = 0;
    int m_total = 0;
    QVariantList m_loadedDats;
    QString m_lastError;
};

} // namespace Remus