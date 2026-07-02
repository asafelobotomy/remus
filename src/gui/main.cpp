#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <csignal>

#include "controllers/app_controller.h"
#include "controllers/artwork_controller.h"
#include "controllers/conversion_controller.h"
#include "controllers/dat_manager_controller.h"
#include "controllers/export_controller.h"
#include "controllers/hash_controller.h"
#include "controllers/match_controller.h"
#include "controllers/metadata_editor_controller.h"
#include "controllers/mod_controller.h"
#include "controllers/organize_controller.h"
#include "controllers/patch_controller.h"
#include "controllers/scan_controller.h"
#include "controllers/settings_controller.h"
#include "controllers/verification_controller.h"
#include "controllers/workflow_controller.h"
#include "models/mod_list_model.h"
#include "models/verification_result_model.h"

#include "../core/constants/constants.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    struct sigaction sa { };
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGPIPE, &sa, nullptr);

    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::setOrganizationName(QString::fromLatin1(Remus::Constants::SETTINGS_ORGANIZATION));
    QCoreApplication::setApplicationName(QString::fromLatin1(Remus::Constants::SETTINGS_APPLICATION));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(Remus::Constants::APP_VERSION));

    // Tighten config-directory permissions (defence in depth — credentials live in
    // the OS keychain, but the config dir should not be world-readable).
    {
        const QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/") + QString::fromLatin1(Remus::Constants::SETTINGS_ORGANIZATION);
        QFile::setPermissions(cfgDir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }

    Remus::AppController appController;
    Remus::SettingsController settingsController;
    Remus::VerificationResultModel verificationResultModel;
    Remus::ModListModel modListModel;

    Remus::ScanController scanController(&appController);
    Remus::HashController hashController(&appController);
    Remus::MatchController matchController(&appController);
    Remus::MetadataEditorController metadataEditorController(&appController);
    Remus::ArtworkController artworkController(&appController);
    Remus::DatManagerController datManagerController(&appController);
    Remus::VerificationController verificationController(&appController);
    Remus::OrganizeController organizeController(&appController);
    Remus::ConversionController conversionController(&appController);
    Remus::ExportController exportController(&appController);
    Remus::PatchController patchController(&appController);
    Remus::ModController modController(&appController);

    Remus::WorkflowController workflowController(&appController, &hashController, &matchController, &artworkController,
        &conversionController, &organizeController, &exportController);

    // Note: exportController → workflowController::refresh is already wired
    // inside WorkflowController's constructor; no second connect needed here.

    QQmlApplicationEngine engine;

    verificationController.setModel(&verificationResultModel);
    modController.setModel(&modListModel);

    const auto refreshLibraryModels = [&]() {
        matchController.refreshModel();
        appController.setSelectedFileId(appController.selectedFileId());
    };

    QObject::connect(&scanController, &Remus::ScanController::libraryChanged, &app, refreshLibraryModels);
    QObject::connect(&scanController, &Remus::ScanController::libraryChanged, &workflowController,
        [&workflowController]() { workflowController.refresh(); });
    QObject::connect(&hashController, &Remus::HashController::libraryChanged, &app, refreshLibraryModels);
    // hashController.libraryChanged → workflowController.refresh() already wired in WorkflowController constructor
    QObject::connect(&matchController, &Remus::MatchController::libraryChanged, &app, refreshLibraryModels);
    // matchController.libraryChanged → workflowController.refresh() already wired in WorkflowController constructor
    QObject::connect(
        &metadataEditorController, &Remus::MetadataEditorController::libraryChanged, &app, refreshLibraryModels);
    QObject::connect(&organizeController, &Remus::OrganizeController::libraryChanged, &app, refreshLibraryModels);
    QObject::connect(&conversionController, &Remus::ConversionController::libraryChanged, &app, refreshLibraryModels);
    QObject::connect(&exportController, &Remus::ExportController::libraryChanged, &app, refreshLibraryModels);
    QObject::connect(&patchController, &Remus::PatchController::libraryChanged, &app, refreshLibraryModels);
    QObject::connect(&modController, &Remus::ModController::libraryChanged, &app, refreshLibraryModels);

    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &appController);
    engine.rootContext()->setContextProperty(QStringLiteral("settingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("verificationResultModel"), &verificationResultModel);
    engine.rootContext()->setContextProperty(QStringLiteral("modListModel"), &modListModel);
    engine.rootContext()->setContextProperty(QStringLiteral("scanController"), &scanController);
    engine.rootContext()->setContextProperty(QStringLiteral("hashController"), &hashController);
    engine.rootContext()->setContextProperty(QStringLiteral("matchController"), &matchController);
    engine.rootContext()->setContextProperty(QStringLiteral("metadataEditorController"), &metadataEditorController);
    engine.rootContext()->setContextProperty(QStringLiteral("artworkController"), &artworkController);
    engine.rootContext()->setContextProperty(QStringLiteral("datManagerController"), &datManagerController);
    engine.rootContext()->setContextProperty(QStringLiteral("verificationController"), &verificationController);
    engine.rootContext()->setContextProperty(QStringLiteral("organizeController"), &organizeController);
    engine.rootContext()->setContextProperty(QStringLiteral("conversionController"), &conversionController);
    engine.rootContext()->setContextProperty(QStringLiteral("exportController"), &exportController);
    engine.rootContext()->setContextProperty(QStringLiteral("patchController"), &patchController);
    engine.rootContext()->setContextProperty(QStringLiteral("modController"), &modController);
    engine.rootContext()->setContextProperty(QStringLiteral("workflowController"), &workflowController);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    engine.loadFromModule("Remus.Gui", "Main");
#else
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Remus/Gui/Main.qml")));
#endif

    return app.exec();
}
