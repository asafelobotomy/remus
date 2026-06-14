#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "../src/core/database.h"
#include "../src/core/library_exporter.h"
#include "../src/core/constants/constants.h"

using namespace Remus;

class LibraryExporterTest : public QObject {
    Q_OBJECT

private slots:
    void exportCsvAndJson();
    void exportRetroArchAndLaunchBox();
};

void LibraryExporterTest::exportCsvAndJson() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Database db;
    QVERIFY(db.initialize(tempDir.filePath(QStringLiteral("library.db"))));

    const int libraryId = db.insertLibrary(tempDir.filePath(QStringLiteral("roms")));
    QVERIFY(libraryId > 0);

    const QString romPath = tempDir.filePath(QStringLiteral("roms/Game (USA).nes"));
    QVERIFY(QDir().mkpath(QFileInfo(romPath).absolutePath()));
    {
        QFile romFile(romPath);
        QVERIFY(romFile.open(QIODevice::WriteOnly));
        romFile.write("NESDATA");
    }

    FileRecord file;
    file.libraryId = libraryId;
    file.filename = QStringLiteral("Game (USA).nes");
    file.originalPath = romPath;
    file.currentPath = romPath;
    file.extension = QStringLiteral(".nes");
    file.fileSize = 7;
    file.systemId = db.getSystemId(QStringLiteral("NES"));
    file.crc32 = QStringLiteral("01234567");
    const int fileId = db.insertFile(file);
    QVERIFY(fileId > 0);

    const int gameId = db.insertGame(QStringLiteral("Game"), file.systemId, QStringLiteral("USA"));
    QVERIFY(gameId > 0);
    QVERIFY(db.insertMatch(fileId, gameId, 100.0f, QStringLiteral("hash")));

    const QString csvPath = tempDir.filePath(QStringLiteral("export.csv"));
    QString error;
    QVERIFY(LibraryExporter::exportToFile(db, Constants::Exports::Formats::CSV, csvPath, { }, &error));

    QFile csvFile(csvPath);
    QVERIFY(csvFile.open(QIODevice::ReadOnly));
    const QString csv = QString::fromUtf8(csvFile.readAll());
    QVERIFY(csv.contains(QStringLiteral("Game")));
    QVERIFY(csv.contains(db.getSystemDisplayName(file.systemId)));

    const QString jsonPath = tempDir.filePath(QStringLiteral("export.json"));
    QVERIFY(LibraryExporter::exportToFile(db, Constants::Exports::Formats::JSON, jsonPath, { }, &error));
    QVERIFY(QFileInfo(jsonPath).exists());
}

void LibraryExporterTest::exportRetroArchAndLaunchBox() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Database db;
    QVERIFY(db.initialize(tempDir.filePath(QStringLiteral("library.db"))));

    const int libraryId = db.insertLibrary(tempDir.filePath(QStringLiteral("roms")));
    const QString romPath = tempDir.filePath(QStringLiteral("roms/Sonic (World).md"));
    QVERIFY(QDir().mkpath(QFileInfo(romPath).absolutePath()));
    {
        QFile romFile(romPath);
        QVERIFY(romFile.open(QIODevice::WriteOnly));
        romFile.write("SONICROM");
    }

    FileRecord file;
    file.libraryId = libraryId;
    file.filename = QStringLiteral("Sonic (World).md");
    file.originalPath = romPath;
    file.currentPath = romPath;
    file.extension = QStringLiteral(".md");
    file.fileSize = 8;
    file.systemId = db.getSystemId(QStringLiteral("Genesis"));
    const int fileId = db.insertFile(file);
    const int gameId = db.insertGame(QStringLiteral("Sonic"), file.systemId, QStringLiteral("World"));
    QVERIFY(db.insertMatch(fileId, gameId, 100.0f, QStringLiteral("hash")));

    QString error;
    const QString retroPath = tempDir.filePath(QStringLiteral("remus.lpl"));
    QVERIFY(LibraryExporter::exportToFile(db, Constants::Exports::Formats::RETROARCH, retroPath, { }, &error));

    const QString launchboxPath = tempDir.filePath(QStringLiteral("launchbox.xml"));
    QVERIFY(LibraryExporter::exportToFile(db, Constants::Exports::Formats::LAUNCHBOX, launchboxPath, { }, &error));
    QFile launchbox(launchboxPath);
    QVERIFY(launchbox.open(QIODevice::ReadOnly));
    QVERIFY(QString::fromUtf8(launchbox.readAll()).contains(QStringLiteral("<LaunchBox>")));
}

QTEST_MAIN(LibraryExporterTest)
#include "test_library_exporter.moc"
