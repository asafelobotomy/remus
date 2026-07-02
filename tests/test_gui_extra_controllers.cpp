#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "controllers/app_controller.h"
#include "controllers/dat_manager_controller.h"
#include "controllers/export_controller.h"
#include "controllers/metadata_editor_controller.h"
#include "controllers/mod_controller.h"
#include "controllers/organize_controller.h"
#include "controllers/patch_controller.h"
#include "../src/core/database_types.h"
#include "../src/core/constants/folder_naming.h"

using namespace Remus;

class GuiExtraControllersTest : public QObject {
    Q_OBJECT

private slots:
    void datManager_requiresOpenLibrary();
    void metadataEditor_tracksDirtyState();
    void exportController_previewRequiresLibrary();
    void exportController_frontendExportWritesCsv();
    void exportController_generateM3uRequiresLibrary();
    void organizeController_exposesFolderSchemes();
    void patchController_checkToolsPopulatesStatus();
    void patchController_lastErrorOnInvalidPatch();
    void appController_showErrorAndDismiss();
    void modController_loadCatalogFromFixture();
};

void GuiExtraControllersTest::datManager_requiresOpenLibrary() {
    AppController app;
    DatManagerController dat(&app);

    QSignalSpy errorSpy(&dat, &DatManagerController::lastErrorChanged);
    const bool ok = dat.importDat(QStringLiteral("/tmp/test.dat"), QStringLiteral("NES"));
    QVERIFY(!ok);
    QCOMPARE(errorSpy.count(), 1);
    QVERIFY(dat.lastError().contains(QStringLiteral("Open a library")));
}

void GuiExtraControllersTest::metadataEditor_tracksDirtyState() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    AppController app;
    QVERIFY(app.openLibrary(tempDir.filePath(QStringLiteral("library.db"))));

    const int libraryId = app.database()->insertLibrary(tempDir.filePath(QStringLiteral("roms")));
    const QString romPath = tempDir.filePath(QStringLiteral("roms/Game.nes"));
    QVERIFY(QDir().mkpath(QFileInfo(romPath).absolutePath()));
    {
        QFile romFile(romPath);
        QVERIFY(romFile.open(QIODevice::WriteOnly));
        romFile.write("DATA");
    }

    FileRecord file;
    file.libraryId = libraryId;
    file.filename = QStringLiteral("Game.nes");
    file.originalPath = romPath;
    file.currentPath = romPath;
    file.extension = QStringLiteral(".nes");
    file.fileSize = 4;
    file.systemId = app.database()->getSystemId(QStringLiteral("NES"));
    const int fileId = app.database()->insertFile(file);
    const int gameId = app.database()->insertGame(QStringLiteral("Game"), file.systemId, QStringLiteral("USA"));
    QVERIFY(app.database()->insertMatch(fileId, gameId, 100.0f, QStringLiteral("manual")));

    app.setSelectedFileId(fileId);
    app.refreshSelectedMatch();
    MetadataEditorController editor(&app);
    editor.loadForSelectedFile();
    QVERIFY(editor.currentGame().value(QStringLiteral("gameId")).toInt() > 0);
    QVERIFY(!editor.isDirty());

    editor.setField(QStringLiteral("publisher"), QStringLiteral("Acme"));
    QVERIFY(editor.isDirty());
    QVERIFY(editor.save());
    QVERIFY(!editor.isDirty());
}

void GuiExtraControllersTest::exportController_previewRequiresLibrary() {
    AppController app;
    ExportController exportCtl(&app);
    QCOMPARE(exportCtl.exportPreview().value(QStringLiteral("totalGames")).toInt(), 0);
}

void GuiExtraControllersTest::exportController_frontendExportWritesCsv() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    AppController app;
    QVERIFY(app.openLibrary(tempDir.filePath(QStringLiteral("library.db"))));

    const int libraryId = app.database()->insertLibrary(tempDir.filePath(QStringLiteral("roms")));
    const QString romPath = tempDir.filePath(QStringLiteral("roms/Game.nes"));
    QVERIFY(QDir().mkpath(QFileInfo(romPath).absolutePath()));
    {
        QFile romFile(romPath);
        QVERIFY(romFile.open(QIODevice::WriteOnly));
        romFile.write("DATA");
    }

    FileRecord file;
    file.libraryId = libraryId;
    file.filename = QStringLiteral("Game.nes");
    file.originalPath = romPath;
    file.currentPath = romPath;
    file.extension = QStringLiteral(".nes");
    file.fileSize = 4;
    file.systemId = app.database()->getSystemId(QStringLiteral("NES"));
    const int fileId = app.database()->insertFile(file);
    const int gameId = app.database()->insertGame(QStringLiteral("Game"), file.systemId, QStringLiteral("USA"));
    QVERIFY(app.database()->insertMatch(fileId, gameId, 100.0f, QStringLiteral("hash")));

    ExportController exportCtl(&app);
    const QString csvPath = tempDir.filePath(QStringLiteral("export.csv"));
    QVERIFY(exportCtl.exportFrontend(QStringLiteral("csv"), csvPath));
    QVERIFY(QFileInfo(csvPath).exists());
}

void GuiExtraControllersTest::exportController_generateM3uRequiresLibrary() {
    AppController app;
    ExportController exportCtl(&app);
    QCOMPARE(exportCtl.generateM3uPlaylists(QStringLiteral("/tmp/out")), 0);
}

void GuiExtraControllersTest::organizeController_exposesFolderSchemes() {
    AppController app;
    OrganizeController organize(&app);
    QVERIFY(organize.folderSchemeChoices().size() >= static_cast<int>(Constants::FolderNaming::SCHEME_NAMES.size()));
}

void GuiExtraControllersTest::patchController_checkToolsPopulatesStatus() {
    AppController app;
    PatchController patch(&app);
    patch.checkTools();
    QVERIFY(!patch.toolStatus().isEmpty());
}

void GuiExtraControllersTest::patchController_lastErrorOnInvalidPatch() {
    AppController app;
    PatchController patch(&app);
    QVERIFY(!patch.applyPatch(QString(), QStringLiteral("/nonexistent/patch.bps"), QString()));
    QVERIFY(!patch.lastError().isEmpty());
}

void GuiExtraControllersTest::appController_showErrorAndDismiss() {
    AppController app;
    QVERIFY(app.errorMessage().isEmpty());
    app.showError(QStringLiteral("Test error"));
    QCOMPARE(app.errorMessage(), QStringLiteral("Test error"));
    app.dismissError();
    QVERIFY(app.errorMessage().isEmpty());
}

void GuiExtraControllersTest::modController_loadCatalogFromFixture() {
    AppController app;
    ModController mod(&app);

    const QString catalog = QString(REMUS_SOURCE_DIR) + QStringLiteral("/tests/fixtures/test_mod_catalog.json");
    if (!QFileInfo::exists(catalog)) {
        QSKIP("Mod catalog fixture not found");
    }

    mod.loadCatalog(catalog);
    QVERIFY(mod.lastError().isEmpty());
}

QTEST_MAIN(GuiExtraControllersTest)
#include "test_gui_extra_controllers.moc"
