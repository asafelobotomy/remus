#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include "../../core/database.h"
#include "../../core/verification_engine.h"

namespace Remus {

/**
 * @brief Controller for ROM verification operations
 * 
 * Handles DAT file import, verification, and result reporting.
 * Exposed to QML as a context property.
 */
class VerificationController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool verifying READ isVerifying NOTIFY verifyingChanged)
    Q_PROPERTY(bool importing READ isImporting NOTIFY importingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(int total READ total NOTIFY totalChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY currentFileChanged)
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY summaryChanged)
    
public:
    explicit VerificationController(Database *db, QObject *parent = nullptr);
    
    bool isVerifying() const { return m_verifying; }
    bool isImporting() const { return m_importing; }
    int progress() const { return m_progress; }
    int total() const { return m_total; }
    QString currentFile() const { return m_currentFile; }
    QVariantList results() const { return m_results; }
    QVariantMap summary() const { return m_summary; }
    
    // DAT file operations
    Q_INVOKABLE bool importDatFile(const QString &filePath, const QString &systemName);
    Q_INVOKABLE void removeDat(const QString &systemName);
    Q_INVOKABLE QVariantList getImportedDats();
    Q_INVOKABLE bool hasDatForSystem(const QString &systemName);
    
    // Verification operations
    Q_INVOKABLE void verifyAll();
    Q_INVOKABLE void verifySystem(const QString &systemName);
    Q_INVOKABLE void verifyFiles(const QVariantList &fileIds);
    Q_INVOKABLE void cancelVerification();
    
    // Results
    Q_INVOKABLE QVariantList getMissingGames(const QString &systemName);
    Q_INVOKABLE bool exportResults(const QString &outputPath, const QString &format);
    Q_INVOKABLE void clearResults();
    
signals:
    void verifyingChanged();
    void importingChanged();
    void progressChanged();
    void totalChanged();
    void currentFileChanged();
    void resultsChanged();
    void summaryChanged();
    
    void importStarted();
    void importCompleted(int entryCount);
    void importError(const QString &error);
    
    void verificationStarted();
    void verificationCompleted();
    void verificationError(const QString &error);
    
private slots:
    void onVerificationProgress(int current, int total, const QString &file);
    void onImportProgress(int current, int total);
    void onVerificationComplete(const VerificationSummary &summary);
    void onError(const QString &message);
    
private:
    Database *m_db;
    VerificationEngine *m_engine;
    
    bool m_verifying = false;
    bool m_importing = false;
    int m_progress = 0;
    int m_total = 0;
    QString m_currentFile;
    QVariantList m_results;
    QVariantMap m_summary;
    bool m_cancelRequested = false;
    
    QVariantMap resultToVariant(const VerificationResult &result);
    QString statusToString(VerificationStatus status);
};

} // namespace Remus
