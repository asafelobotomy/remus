// Run-all pipeline logic extracted from workflow_controller.cpp to keep each
// translation unit within the 400-line project baseline.
#include "workflow_controller.h"

#include "app_controller.h"
#include "hash_controller.h"
#include "match_controller.h"
#include "artwork_controller.h"
#include "conversion_controller.h"
#include "organize_controller.h"
#include "export_controller.h"

namespace Remus {

namespace {

    void reportRunAllError(AppController *appController, const QString &stage, const QString &message) {
        if (appController != nullptr) {
            appController->showError(QStringLiteral("%1 failed: %2").arg(stage, message));
            appController->setStatusMessage(QStringLiteral("Run all stopped at %1.").arg(stage));
        }
    }

} // namespace

void WorkflowController::advanceRunAll() {
    if (!m_running)
        return;

    switch (m_runStep++) {

    case 0: {
        // Hash all files — guard ensures only one of the two connections fires
        setActiveStage(2);
        auto *guard = new QObject(this);
        connect(m_hashController, &HashController::hashCompleted, guard, [this, guard](int) {
            delete guard;
            advanceRunAll();
        });
        connect(m_hashController, &HashController::hashError, guard, [this, guard](const QString &message) {
            reportRunAllError(m_appController, QStringLiteral("Hash"), message);
            delete guard;
            cancelRunAll();
        });
        m_hashController->startHashAll();
        break;
    }

    case 1: {
        // Match all files — guard QObject auto-disconnects on delete
        auto *guard = new QObject(this);
        connect(m_matchController, &MatchController::matchAllFinished, guard, [this, guard]() {
            delete guard;
            advanceRunAll();
        });
        connect(m_matchController, &MatchController::matchError, guard, [this, guard](const QString &message) {
            reportRunAllError(m_appController, QStringLiteral("Match"), message);
            delete guard;
            cancelRunAll();
        });
        m_matchController->matchAll();
        break;
    }

    case 2:
        // Confirm all matches so artwork lookup finds them
        m_matchController->confirmAll();
        advanceRunAll();
        break;

    case 3: {
        // Artwork batch download
        setActiveStage(3);
        auto *guard = new QObject(this);
        connect(m_artworkController, &ArtworkController::batchDownloadFinished, guard, [this, guard]() {
            delete guard;
            advanceRunAll();
        });
        m_artworkController->downloadAllMatched();
        break;
    }

    case 4: {
        setActiveStage(4);
        if (!m_conversionController) {
            advanceRunAll();
            break;
        }
        auto *guard = new QObject(this);
        connect(m_conversionController, &ConversionController::conversionFinished, guard, [this, guard]() {
            delete guard;
            advanceRunAll();
        });
        m_conversionController->convertAll(QStringLiteral("AUTO"), QString(), m_scanDir);
        break;
    }

    case 5: // Bundle confirmed ROMs (rejected and unconfirmed filtered by bundleAll)
        setActiveStage(5);
        if (m_exportController)
            m_exportController->bundleAll(m_scanDir, m_namingTemplate);
        advanceRunAll();
        break;

    case 6: // Organize confirmed ROMs into the destination directory
        setActiveStage(6);
        if (!m_destDir.isEmpty())
            m_organizeController->applyOrganize(m_destDir);
        advanceRunAll();
        break;

    default:
        cancelRunAll();
        break;
    }
}

void WorkflowController::cancelRunAll() {
    m_running = false;
    m_runStep = 0;
    m_activeStage = 0;
    emit runningChanged(); // QML onRunningChanged handler collapses stages to 0
    refresh();
}

void WorkflowController::setActiveStage(int stage) {
    if (m_activeStage == stage)
        return;
    m_activeStage = stage;
    emit activeStageChanged();
}

} // namespace Remus
