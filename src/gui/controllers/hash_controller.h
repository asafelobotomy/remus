#pragma once

#include <QObject>

#include "../../services/hash_service.h"

namespace Remus {

class AppController;

class HashController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hashing READ isHashing NOTIFY hashingChanged)
    Q_PROPERTY(int hashedFiles READ hashedFiles NOTIFY progressChanged)
    Q_PROPERTY(int totalFiles READ totalFiles NOTIFY progressChanged)
    Q_PROPERTY(QString progressMessage READ progressMessage NOTIFY progressMessageChanged)

public:
    explicit HashController(AppController *appController, QObject *parent = nullptr);

    bool isHashing() const {
        return m_hashing;
    }
    int hashedFiles() const {
        return m_hashedFiles;
    }
    int totalFiles() const {
        return m_totalFiles;
    }
    QString progressMessage() const {
        return m_progressMessage;
    }

    Q_INVOKABLE void startHashAll();
    Q_INVOKABLE void hashSelected();

signals:
    void hashingChanged();
    void progressChanged();
    void progressMessageChanged();
    void hashCompleted(int hashedCount);
    void hashError(const QString &message);
    void libraryChanged();

private:
    AppController *m_appController;
    HashService m_hashService;
    bool m_hashing = false;
    int m_hashedFiles = 0;
    int m_totalFiles = 0;
    QString m_progressMessage;
};

} // namespace Remus