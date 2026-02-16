#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QStandardPaths>
#include <QSettings>
#include <QDir>
#include <QDebug>
#include "../core/database.h"
#include "models/file_list_model.h"
#include "models/match_list_model.h"
#include "controllers/library_controller.h"
#include "controllers/match_controller.h"
#include "controllers/conversion_controller.h"
#include "controllers/settings_controller.h"
#include "controllers/dat_manager_controller.h"
#include "controllers/artwork_controller.h"
#include "controllers/metadata_editor_controller.h"
#include "controllers/export_controller.h"
#include "controllers/verification_controller.h"
#include "controllers/patch_controller.h"
#include "controllers/processing_controller.h"
#include "theme_constants.h"
#include "../core/constants/constants.h"
#include "../metadata/provider_orchestrator.h"
#include "../metadata/local_database_provider.h"
#include "../metadata/hasheous_provider.h"
#include "../metadata/screenscraper_provider.h"
#include "../metadata/thegamesdb_provider.h"
#include "../metadata/igdb_provider.h"

using namespace Remus;

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    // Set application metadata
    app.setOrganizationName(Constants::SETTINGS_ORGANIZATION);
    app.setOrganizationDomain("remus.app");
    app.setApplicationName(Constants::SETTINGS_APPLICATION);
    app.setApplicationVersion(Constants::APP_VERSION);
    
    // Set icon
    app.setWindowIcon(QIcon(":/icons/remus.png"));
    
    // Initialize database
    Database db;
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/" + Constants::DATABASE_FILENAME;
    if (!db.initialize(dbPath)) {
        qCritical() << "Failed to initialize database at:" << dbPath;
        return 1;
    }
    
    // Create QML engine
    QQmlApplicationEngine engine;
    
    // Create controllers (singletons accessible from QML)
    LibraryController libraryController(&db);
    MatchController matchController(&db);
    ConversionController conversionController(&db);
    SettingsController settingsController;
    ThemeConstants themeConstants;
    
    // Create models with database access
    FileListModel fileListModel(&db);
    MatchListModel matchListModel(&db);
    
    // Create provider orchestrator for metadata operations
    ProviderOrchestrator orchestrator;
    
    // Add Local Database provider (HIGHEST priority - offline hash-based matching)
    auto localDbProvider = new LocalDatabaseProvider();
    QString databaseDir = QCoreApplication::applicationDirPath() + "/data/databases";
    
    // Try application directory first, then fallback to source directory (development)
    if (!QDir(databaseDir).exists()) {
        // Development build: executable is in build/src/ui/, data is at project root
        databaseDir = QCoreApplication::applicationDirPath() + "/../../../data/databases";
    }
    
    if (QDir(databaseDir).exists()) {
        int entriesLoaded = localDbProvider->loadDatabases(databaseDir);
        qDebug() << "LocalDatabase: Loaded" << entriesLoaded << "entries from" << databaseDir;
    } else {
        qWarning() << "LocalDatabase: Database directory not found:" << databaseDir;
        qWarning() << "LocalDatabase: Offline hash matching will be unavailable";
    }
    
    orchestrator.addProvider("localdatabase", localDbProvider, 110); // Highest priority
    qDebug() << "Initialized LocalDatabase provider (priority: 110)";
    
    // Create DAT manager controller for UI
    DatManagerController datManagerController(localDbProvider);
    
    // Add Hasheous provider (high priority, FREE, no auth required)
    auto hasheousProvider = new HasheousProvider();
    const auto hasheousInfo = Constants::Providers::getProviderInfo(Constants::Providers::HASHEOUS);
    const int hasheousPriority = hasheousInfo ? hasheousInfo->priority : 100;
    orchestrator.addProvider(Constants::Providers::HASHEOUS, hasheousProvider, hasheousPriority);
    qDebug() << "Initialized Hasheous provider (priority:" << hasheousPriority << ")";

    
    // Add ScreenScraper provider (if credentials are configured)
    QSettings settings;
    QString ssUser = settings.value(Constants::Settings::Providers::SCREENSCRAPER_USERNAME).toString();
    QString ssPass = settings.value(Constants::Settings::Providers::SCREENSCRAPER_PASSWORD).toString();
    if (!ssUser.isEmpty() && !ssPass.isEmpty()) {
        auto ssProvider = new ScreenScraperProvider();
        ssProvider->setCredentials(ssUser, ssPass);
        
        QString ssDevId = settings.value(Constants::Settings::Providers::SCREENSCRAPER_DEVID).toString();
        QString ssDevPass = settings.value(Constants::Settings::Providers::SCREENSCRAPER_DEVPASSWORD).toString();
        if (!ssDevId.isEmpty() && !ssDevPass.isEmpty()) {
            ssProvider->setDeveloperCredentials(ssDevId, ssDevPass);
        }
        
        const auto ssInfo = Constants::Providers::getProviderInfo(Constants::Providers::SCREENSCRAPER);
        const int ssPriority = ssInfo ? ssInfo->priority : 90;
        orchestrator.addProvider(Constants::Providers::SCREENSCRAPER, ssProvider, ssPriority);
        qDebug() << "Initialized ScreenScraper provider (priority:" << ssPriority << ")";
    } else {
        qDebug() << "ScreenScraper disabled (no credentials configured)";
    }
    
    // Add TheGamesDB provider (optional API key for better rate limits)
    auto tgdbProvider = new TheGamesDBProvider();
    QString tgdbApiKey = settings.value(Constants::Settings::Providers::THEGAMESDB_API_KEY).toString();
    if (!tgdbApiKey.isEmpty()) {
        tgdbProvider->setApiKey(tgdbApiKey);
        qDebug() << "TheGamesDB API key configured";
    }
    const auto tgdbInfo = Constants::Providers::getProviderInfo(Constants::Providers::THEGAMESDB);
    const int tgdbPriority = tgdbInfo ? tgdbInfo->priority : 50;
    orchestrator.addProvider(Constants::Providers::THEGAMESDB, tgdbProvider, tgdbPriority);
    qDebug() << "Initialized TheGamesDB provider (priority:" << tgdbPriority << ")";
    
    // Add IGDB provider (requires Twitch OAuth credentials)
    auto igdbProvider = new IGDBProvider();
    QString igdbClientId = settings.value(Constants::Settings::Providers::IGDB_CLIENT_ID).toString();
    QString igdbClientSecret = settings.value(Constants::Settings::Providers::IGDB_CLIENT_SECRET).toString();
    if (!igdbClientId.isEmpty() && !igdbClientSecret.isEmpty()) {
        igdbProvider->setCredentials(igdbClientId, igdbClientSecret);
        qDebug() << "IGDB credentials configured";
    } else {
        qDebug() << "IGDB disabled (no credentials configured)";
    }
    const auto igdbInfo = Constants::Providers::getProviderInfo(Constants::Providers::IGDB);
    const int igdbPriority = igdbInfo ? igdbInfo->priority : 40;
    orchestrator.addProvider(Constants::Providers::IGDB, igdbProvider, igdbPriority);
    qDebug() << "Initialized IGDB provider (priority:" << igdbPriority << ")";
    
    // Create M8 controllers (artwork, metadata editor, export)
    ArtworkController artworkController(&db, &orchestrator);
    MetadataEditorController metadataEditor(&db);
    ExportController exportController(&db);
    
    // Create M9 controllers (verification, patching)
    VerificationController verificationController(&db);
    PatchController patchController(&db);
    
    // Create processing controller for batch pipeline
    ProcessingController processingController(&db, &orchestrator);
    
    // Register FileRole enum for QML (allows FileListModel.IdRole, FilenameRole, etc.)
    qmlRegisterUncreatableMetaObject(
        FileListModel::staticMetaObject,
        "Remus",
        1, 0,
        "FileListModel",
        "FileListModel enum cannot be instantiated - access via FileListModel.IdRole"
    );
    
    // Expose controllers to QML context
    engine.rootContext()->setContextProperty("libraryController", &libraryController);
    engine.rootContext()->setContextProperty("matchController", &matchController);
    engine.rootContext()->setContextProperty("conversionController", &conversionController);
    engine.rootContext()->setContextProperty("settingsController", &settingsController);
    engine.rootContext()->setContextProperty("datManager", &datManagerController);
    engine.rootContext()->setContextProperty("theme", &themeConstants);
    engine.rootContext()->setContextProperty("artworkController", &artworkController);
    engine.rootContext()->setContextProperty("metadataEditor", &metadataEditor);
    engine.rootContext()->setContextProperty("processingController", &processingController);
    engine.rootContext()->setContextProperty("exportController", &exportController);
    engine.rootContext()->setContextProperty("verificationController", &verificationController);
    engine.rootContext()->setContextProperty("patchController", &patchController);
    
    // Expose models to QML context
    engine.rootContext()->setContextProperty("fileListModel", &fileListModel);
    engine.rootContext()->setContextProperty("matchListModel", &matchListModel);
    
    // Initialize models with data
    fileListModel.refresh();
    matchListModel.refresh();
    
    // Connect signals for model auto-refresh
    bool connected = QObject::connect(&libraryController, QOverload<int>::of(&LibraryController::scanCompleted),
        &fileListModel, &FileListModel::refresh);
    qDebug() << "Signal connection scanCompleted->refresh:" << (connected ? "SUCCESS" : "FAILED");
    
    // Load main QML file
    const QUrl url(QStringLiteral("qrc:/qml/MainWindow.qml"));
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
        &app, [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) {
                QCoreApplication::exit(-1);
            }
        }, Qt::QueuedConnection);
    
    engine.load(url);
    
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML";
        return -1;
    }
    
    qInfo() << "Remus GUI started successfully";
    qInfo() << "Database:" << dbPath;
    
    return app.exec();
}
