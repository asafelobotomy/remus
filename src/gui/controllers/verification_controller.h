#pragma once

#include <QObject>
#include <QVariantMap>

#include "../../core/verification_engine.h"

namespace Remus {

class AppController;
class VerificationResultModel;

class VerificationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool verifying READ isVerifying NOTIFY verifyingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int total READ total NOTIFY progressChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY currentFileChanged)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY summaryChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit VerificationController(AppController *appController, QObject *parent = nullptr);

    bool isVerifying() const { return m_verifying; }
    int progress() const { return m_progress; }
    int total() const { return m_total; }
    QString currentFile() const { return m_currentFile; }
    QVariantMap summary() const { return m_summary; }
    QString lastError() const { return m_lastError; }

    void setModel(VerificationResultModel *model) { m_model = model; }

    Q_INVOKABLE void verifyAll();
    Q_INVOKABLE void verifySelected();
    Q_INVOKABLE void clearResults();

signals:
    void verifyingChanged();
    void progressChanged();
    void currentFileChanged();
    void summaryChanged();
    void lastErrorChanged();

private:
    void populateResults(const QList<VerificationResult> &results);
    static QString statusToString(VerificationStatus status);
    void setLastError(const QString &message);

    AppController *m_appController;
    VerificationEngine m_engine;
    VerificationResultModel *m_model = nullptr;
    bool m_verifying = false;
    int m_progress = 0;
    int m_total = 0;
    QString m_currentFile;
    QVariantMap m_summary;
    QString m_lastError;
};

} // namespace Remus