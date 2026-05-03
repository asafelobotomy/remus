#include "hash_controller.h"

#include "app_controller.h"

namespace Remus {

HashController::HashController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
{
    connect(this, &HashController::libraryChanged, m_appController, &AppController::refreshSelectedFile);
}

void HashController::startHashAll()
{
    if (m_hashing) {
        emit hashError(QStringLiteral("Hashing is already running."));
        return;
    }

    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        emit hashError(QStringLiteral("Open a library before hashing files."));
        return;
    }

    m_hashing = true;
    m_hashedFiles = 0;
    m_totalFiles = m_appController->database()->getFilesWithoutHashes().size();
    m_progressMessage = QStringLiteral("Hashing files\u2026 0 / %1").arg(m_totalFiles);
    emit hashingChanged();
    emit progressChanged();
    emit progressMessageChanged();

    const int hashed = m_hashService.hashAll(
        m_appController->database(),
        [this](int done, int total, const QString &) {
            m_hashedFiles = done;
            m_totalFiles  = total;
            m_progressMessage = QStringLiteral("Hashing files\u2026 %1 / %2").arg(done).arg(total);
            emit progressChanged();
            emit progressMessageChanged();
        });

    m_hashing = false;
    m_progressMessage = QStringLiteral("Hashed %1 file(s).").arg(hashed);
    emit hashingChanged();
    emit progressMessageChanged();
    m_appController->setStatusMessage(QStringLiteral("Hashed %1 file(s).").arg(hashed));
    emit hashCompleted(hashed);
    emit libraryChanged();
}

void HashController::hashSelected()
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        emit hashError(QStringLiteral("Open a library before hashing files."));
        return;
    }

    const int selectedFileId = m_appController->selectedFileId();
    if (selectedFileId <= 0) {
        emit hashError(QStringLiteral("Select a file first."));
        return;
    }

    m_progressMessage = QStringLiteral("Hashing selected file\u2026");
    emit progressMessageChanged();

    if (!m_hashService.hashFile(m_appController->database(), selectedFileId)) {
        m_progressMessage = QStringLiteral("Failed to hash the selected file.");
        emit progressMessageChanged();
        emit hashError(QStringLiteral("Failed to hash the selected file."));
        return;
    }

    m_progressMessage = QStringLiteral("File hashed.");
    emit progressMessageChanged();
    m_appController->setStatusMessage(QStringLiteral("Selected file hashed."));
    emit hashCompleted(1);
    emit libraryChanged();
}

} // namespace Remus