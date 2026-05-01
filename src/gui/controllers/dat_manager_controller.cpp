#include "dat_manager_controller.h"

#include "app_controller.h"

namespace Remus {

DatManagerController::DatManagerController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
    , m_engine(appController->database(), this)
{
    connect(&m_engine, &VerificationEngine::datImportProgress, this, [this](int current, int total) {
        m_progress = current;
        m_total = total;
        emit progressChanged();
    });
    connect(&m_engine, &VerificationEngine::error, this, &DatManagerController::setLastError);
    connect(appController, &AppController::libraryOpened, &m_engine, &VerificationEngine::createVerificationSchema);
}

bool DatManagerController::importDat(const QString &path, const QString &systemName)
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastError(QStringLiteral("Open a library before importing DAT files."));
        return false;
    }

    m_importing = true;
    m_progress = 0;
    m_total = 0;
    emit importingChanged();
    emit progressChanged();

    const int count = m_engine.importDat(path.trimmed(), systemName.trimmed());
    m_importing = false;
    emit importingChanged();

    if (count < 0) {
        setLastError(QStringLiteral("Failed to import DAT file."));
        return false;
    }

    rebuildLoadedDats();
    m_appController->setStatusMessage(QStringLiteral("Imported %1 DAT entries.").arg(count));
    return true;
}

void DatManagerController::removeDat(const QString &systemName)
{
    if (!m_engine.removeDat(systemName.trimmed())) {
        setLastError(QStringLiteral("Failed to remove DAT for %1.").arg(systemName));
        return;
    }

    rebuildLoadedDats();
}

void DatManagerController::refresh()
{
    rebuildLoadedDats();
}

void DatManagerController::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
}

void DatManagerController::rebuildLoadedDats()
{
    QVariantList items;
    const auto dats = m_engine.getImportedDats();
    for (auto it = dats.constBegin(); it != dats.constEnd(); ++it) {
        QVariantMap item;
        item.insert(QStringLiteral("system"), it.key());
        item.insert(QStringLiteral("name"), it.value().name);
        item.insert(QStringLiteral("version"), it.value().version);
        item.insert(QStringLiteral("description"), it.value().description);
        items.append(item);
    }

    m_loadedDats = items;
    emit loadedDatsChanged();
}

} // namespace Remus