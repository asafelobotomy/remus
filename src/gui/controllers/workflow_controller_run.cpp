// Run-all pipeline logic extracted from workflow_controller.cpp to keep each
// translation unit within the 400-line project baseline.
#include "workflow_controller.h"

#include "hash_controller.h"
#include "match_controller.h"
#include "artwork_controller.h"
#include "conversion_controller.h"
#include "organize_controller.h"
#include "export_controller.h"

namespace Remus {

void WorkflowController::advanceRunAll()
{
    if (!m_running) return;

    switch (m_runStep++) {

    case 0: {
        // Hash all files — guard ensures only one of the two connections fires
        setActiveStage(2);
        auto *guard = new QObject(this);
        connect(m_hashController, &HashController::hashCompleted, guard, [this, guard](int) {
            delete guard;
            advanceRunAll();
        });
        connect(m_hashController, &HashController::hashError, guard, [this, guard](const QString &) {
            delete guard;
            cancelRunAll();
        });
        m_hashController->startHashAll();
        break;
    }

    case 1: {
        // Match all files — guard QObject auto-disconnects on delete
        auto *guard = new QObject(this);
        connect(m_matchController, &MatchController::libraryChanged, guard, [this, guard]() {
            delete guard;
            advanceRunAll();
        });
        m_matchController->matchAll();
        break;
    }

    case 2:
        // Confirm all matches so artwork lookup finds them
        m_matchController->confirmAll();
        advanceRunAll();
        break;

    case 3:
        // Artwork (synchronous download loop)
        setActiveStage(3);
        m_artworkController->downloadAllMatched();
        advanceRunAll();
        break;

    case 4:  // Convert — synchronous loop; AUTO resolves format per file
        setActiveStage(4);
        if (m_conversionController)
            m_conversionController->convertAll(QStringLiteral("AUTO"), QString(), m_scanDir);
        advanceRunAll();
        break;

    case 5:  // Bundle confirmed ROMs (rejected and unconfirmed filtered by bundleAll)
        setActiveStage(5);
        if (m_exportController)
            m_exportController->bundleAll(m_scanDir, m_namingTemplate);
        advanceRunAll();
        break;

    case 6:  // Organize confirmed ROMs into the destination directory
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

void WorkflowController::cancelRunAll()
{
    m_running     = false;
    m_runStep     = 0;
    m_activeStage = 0;
    emit runningChanged();  // QML onRunningChanged handler collapses stages to 0
    refresh();
}

void WorkflowController::setActiveStage(int stage)
{
    if (m_activeStage == stage) return;
    m_activeStage = stage;
    emit activeStageChanged();
}

} // namespace Remus
