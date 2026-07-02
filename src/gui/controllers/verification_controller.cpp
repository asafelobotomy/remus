#include "verification_controller.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QThread>

#include "app_controller.h"
#include "../models/verification_result_model.h"
#include "../../core/database.h"

namespace Remus {

VerificationController::VerificationController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
    , m_engine(appController->database(), this) {
    connect(&m_engine, &VerificationEngine::verificationProgress, this,
        [this](int current, int total, const QString &file) {
            m_progress = current;
            m_total = total;
            m_currentFile = QFileInfo(file).fileName();
            emit progressChanged();
            emit currentFileChanged();
        });
    connect(&m_engine, &VerificationEngine::verificationComplete, this, [this](const VerificationSummary &summary) {
        m_summary.insert(QStringLiteral("totalFiles"), summary.totalFiles);
        m_summary.insert(QStringLiteral("verified"), summary.verified);
        m_summary.insert(QStringLiteral("mismatched"), summary.mismatched);
        m_summary.insert(QStringLiteral("notInDat"), summary.notInDat);
        m_summary.insert(QStringLiteral("noHash"), summary.noHash);
        m_summary.insert(QStringLiteral("corrupt"), summary.corrupt);
        m_summary.insert(QStringLiteral("datName"), summary.datName);
        emit summaryChanged();
    });
    connect(&m_engine, &VerificationEngine::error, this, &VerificationController::setLastError);
    connect(appController, &AppController::libraryOpened, &m_engine, &VerificationEngine::createVerificationSchema);
}

VerificationController::~VerificationController() {
    if (m_thread && m_thread->isRunning()) {
        m_thread->wait();
    }
}

void VerificationController::verifyAll() {
    if (m_verifying) {
        setLastError(QStringLiteral("Verification is already running."));
        return;
    }

    m_verifying = true;
    m_progress = 0;
    m_total = 0;
    m_currentFile.clear();
    m_summary.clear();
    m_pendingFileIds.clear();
    emit verifyingChanged();
    emit progressChanged();
    emit currentFileChanged();
    emit summaryChanged();

    const QString dbPath = m_appController ? m_appController->libraryPath() : QString();
    m_thread = QThread::create([this, dbPath]() {
        Database workerDb;
        const QString connectionName
            = QStringLiteral("remus-verify-%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
        QList<VerificationResult> results;
        if (workerDb.initialize(dbPath, connectionName)) {
            VerificationEngine workerEngine(&workerDb);
            QObject::connect(
                &workerEngine, &VerificationEngine::verificationProgress, this,
                [this](int current, int total, const QString &file) {
                    QMetaObject::invokeMethod(
                        this,
                        [this, current, total, file]() {
                            m_progress = current;
                            m_total = total;
                            m_currentFile = QFileInfo(file).fileName();
                            emit progressChanged();
                            emit currentFileChanged();
                        },
                        Qt::QueuedConnection);
                },
                Qt::DirectConnection);
            results = workerEngine.verifyLibrary();
            workerDb.close();
        }

        QMetaObject::invokeMethod(this, [this, results]() { finishVerification(results); }, Qt::QueuedConnection);
    });

    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    m_thread->start();
}

void VerificationController::verifySelected() {
    if (m_verifying) {
        setLastError(QStringLiteral("Verification is already running."));
        return;
    }

    const int fileId = m_appController ? m_appController->selectedFileId() : 0;
    if (fileId <= 0) {
        setLastError(QStringLiteral("Select a file first."));
        return;
    }

    m_verifying = true;
    m_progress = 0;
    m_total = 1;
    emit verifyingChanged();
    emit progressChanged();

    const QString dbPath = m_appController->libraryPath();
    m_thread = QThread::create([this, dbPath, fileId]() {
        Database workerDb;
        const QString connectionName
            = QStringLiteral("remus-verify-%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
        QList<VerificationResult> results;
        if (workerDb.initialize(dbPath, connectionName)) {
            VerificationEngine workerEngine(&workerDb);
            results = workerEngine.verifyFiles({ fileId });
            workerDb.close();
        }

        QMetaObject::invokeMethod(this, [this, results]() { finishVerification(results); }, Qt::QueuedConnection);
    });

    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    m_thread->start();
}

void VerificationController::finishVerification(const QList<VerificationResult> &results) {
    populateResults(results);
    m_verifying = false;
    m_thread = nullptr;
    emit verifyingChanged();
    emit verificationFinished();
}

void VerificationController::clearResults() {
    if (m_model != nullptr) {
        m_model->clear();
    }
    m_summary.clear();
    emit summaryChanged();
}

void VerificationController::populateResults(const QList<VerificationResult> &results) {
    QList<VerificationListEntry> entries;
    entries.reserve(results.size());
    for (const VerificationResult &result : results) {
        VerificationListEntry entry;
        entry.fileId = result.fileId;
        entry.filename = result.filename;
        entry.system = result.system;
        entry.status = statusToString(result.status);
        entry.fileHash = result.fileHash;
        entry.datHash = result.datHash;
        entry.hashType = result.hashType;
        entry.notes = result.notes;
        entries.append(entry);
    }

    if (m_model != nullptr) {
        m_model->setEntries(entries);
    }
}

QString VerificationController::statusToString(VerificationStatus status) {
    switch (status) {
    case VerificationStatus::Verified:
        return QStringLiteral("verified");
    case VerificationStatus::Mismatch:
        return QStringLiteral("mismatch");
    case VerificationStatus::NotInDat:
        return QStringLiteral("not_in_dat");
    case VerificationStatus::HashMissing:
        return QStringLiteral("hash_missing");
    case VerificationStatus::Corrupt:
        return QStringLiteral("corrupt");
    default:
        return QStringLiteral("unknown");
    }
}

void VerificationController::setLastError(const QString &message) {
    if (m_lastError == message) {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
}

} // namespace Remus
