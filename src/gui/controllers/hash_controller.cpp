#include "hash_controller.h"

#include "app_controller.h"

namespace Remus {

HashController::HashController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
{
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
    emit hashingChanged();
    emit progressChanged();

    const int hashed = m_hashService.hashAll(
        m_appController->database(),
        [this](int done, int total, const QString &) {
            m_hashedFiles = done;
            m_totalFiles = total;
            emit progressChanged();
        });

    m_hashing = false;
    emit hashingChanged();
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

    if (!m_hashService.hashFile(m_appController->database(), selectedFileId)) {
        emit hashError(QStringLiteral("Failed to hash the selected file."));
        return;
    }

    m_appController->setStatusMessage(QStringLiteral("Selected file hashed."));
    emit hashCompleted(1);
    emit libraryChanged();
}

} // namespace Remus