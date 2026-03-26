#include <QtTest/QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>

class CliSmokeTest : public QObject {
    Q_OBJECT

private:
    QString fixturePath(const QString &name) const {
        const QStringList candidates = {
            QString(REMUS_SOURCE_DIR) + "/tests/fixtures/" + name,
            QDir::currentPath() + "/tests/fixtures/" + name,
            QCoreApplication::applicationDirPath() + "/../../tests/fixtures/" + name,
            QCoreApplication::applicationDirPath() + "/../tests/fixtures/" + name,
        };
        for (const auto &path : candidates) {
            if (QFile::exists(path)) {
                return QDir::cleanPath(path);
            }
        }
        return {};
    }

    QString cliPath() const {
        QFileInfo fi(QCoreApplication::applicationDirPath() + "/../remus-cli");
        if (!fi.exists()) {
            fi.setFile(QCoreApplication::applicationDirPath() + "/remus-cli");
        }
        if (!fi.exists()) {
            return {};
        }
        return fi.absoluteFilePath();
    }

    void runCli(const QStringList &extraArgs, int expectedExit = 0) const {
        QProcess proc;
        QStringList args = extraArgs;
        if (!args.contains("--no-interactive")) {
            args.prepend("--no-interactive");
        }
        const QString binary = cliPath();
        QVERIFY2(!binary.isEmpty(), "remus-cli binary not found next to tests");
        proc.start(binary, args);
        bool finished = proc.waitForFinished(30000);
        QVERIFY2(finished, "CLI did not finish in time");
        QCOMPARE(proc.exitCode(), expectedExit);
    }

    void runCliCapture(const QStringList &extraArgs, QString &output, int expectedExit = 0) const {
        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);

        QStringList args = extraArgs;
        if (!args.contains("--no-interactive")) {
            args.prepend("--no-interactive");
        }
        const QString binary = cliPath();
        QVERIFY2(!binary.isEmpty(), "remus-cli binary not found next to tests");
        proc.start(binary, args);
        bool finished = proc.waitForFinished(30000);
        QVERIFY2(finished, "CLI did not finish in time");
        QCOMPARE(proc.exitCode(), expectedExit);
        output = QString::fromUtf8(proc.readAll());
    }

    void runCliJson(const QStringList &extraArgs, QJsonDocument &doc, int expectedExit = 0) const {
        QString output;
        runCliCapture(extraArgs, output, expectedExit);

        QJsonParseError parseError;
        doc = QJsonDocument::fromJson(output.toUtf8(), &parseError);
        QVERIFY2(parseError.error == QJsonParseError::NoError, qPrintable(parseError.errorString()));
        QVERIFY2(!doc.isNull(), "Expected valid JSON output");
    }

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("Remus");
        QCoreApplication::setApplicationName("RemusTest");
    }

    void testHelp() {
        runCli({"--help"});
    }

    void testStatsNonInteractive() {
        runCli({"--stats"});
    }

    void testExportDryRun() {
        QTemporaryDir dir;
        QString outPath = dir.filePath("export.csv");
        runCli({"--export", "csv", "--export-path", outPath, "--dry-run-all"});
    }

    void testArchiveDryRun() {
        QTemporaryDir dir;
        QString archive = dir.filePath("dummy.zip");
        QFile f(archive);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("PK\x03\x04") == 4); // minimal signature
        f.close();
        runCli({"--extract-archive", archive, "--dry-run-all"});
    }

    void testConvertChdDryRun() {
        if (QStandardPaths::findExecutable("chdman").isEmpty()) {
            QSKIP("chdman not available");
        }
        QTemporaryDir dir;
        QString cue = dir.filePath("sample.cue");
        QFile f(cue);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("REM dummy cue\n") == 14);
        f.close();
        runCli({"--convert-chd", cue, "--dry-run-all"});
    }

    void testOrganizeDryRun() {
        QTemporaryDir dir;
        runCli({"--organize", dir.path(), "--dry-run-all"});
    }

    void testPatchCreateDryRun() {
        QTemporaryDir dir;
        QString original = dir.filePath("orig.bin");
        QString modified = dir.filePath("mod.bin");
        runCli({"--patch-create", modified, "--patch-original", original, "--patch-format", "bps", "--dry-run-all"});
    }

    void testScanEmptyDir() {
        QTemporaryDir dir;
        runCli({"--scan", dir.path()});
    }

    void testModSystemsFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QString output;
        runCliCapture({"--mod-catalog", catalog, "--mod-systems"}, output);
        QVERIFY(output.contains("Super Nintendo"));
        QVERIFY(output.contains("Sega Genesis"));
    }

    void testModShowFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QString output;
        runCliCapture({"--mod-catalog", catalog, "--mod-show", "test-mod-alpha"}, output);
        QVERIFY(output.contains("Test Mod Alpha"));
        QVERIFY(output.contains("Test Author"));
        QVERIFY(output.contains("file:///tmp/test-patch.ips"));
    }

    void testModSystemFilterFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QString output;
        runCliCapture({"--mod-catalog", catalog, "--mod-system", "Super Nintendo"}, output);
        QVERIFY(output.contains("Test Mod Alpha"));
        QVERIFY(output.contains("Test Mod Beta"));
        QVERIFY(!output.contains("Test Mod Gamma"));
    }

    void testModAuthorFilterFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QString output;
        runCliCapture({"--mod-catalog", catalog, "--mod-author", "Test"}, output);
        QVERIFY(output.contains("Test Mod Alpha"));
        QVERIFY(!output.contains("Test Mod Beta"));
        QVERIFY(!output.contains("Test Mod Gamma"));
    }

    void testModTypeAndRatingFiltersFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QString output;
        runCliCapture({"--mod-catalog", catalog, "--mod-type", "translation", "--mod-min-rating", "3.5"}, output);
        QVERIFY(output.contains("Test Mod Beta"));
        QVERIFY(!output.contains("Test Mod Alpha"));
        QVERIFY(!output.contains("Test Mod Gamma"));
    }

    void testModRatingFilterRejectsInvalidInput() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QString output;
        runCliCapture({"--mod-catalog", catalog, "--mod-min-rating", "9.0"}, output, 1);
        QVERIFY(output.contains("Invalid rating"));
    }

    void testModJsonSystemsOutput() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QJsonDocument doc;
        runCliJson({"--mod-catalog", catalog, "--mod-systems", "--json"}, doc);
        QVERIFY(doc.isArray());
        QVERIFY(doc.array().size() >= 2);
        QVERIFY(doc.array().first().toObject().contains("system"));
    }

    void testModSortByDownloads() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QString output;
        runCliCapture({"--mod-catalog", catalog, "--mod-system", "Super Nintendo", "--mod-sort", "downloads"}, output);

        const int alphaPos = output.indexOf("Test Mod Alpha");
        const int betaPos = output.indexOf("Test Mod Beta");
        QVERIFY(alphaPos >= 0);
        QVERIFY(betaPos >= 0);
        QVERIFY(alphaPos < betaPos);
    }

    void testModFormatAndDownloadsFiltersFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QString output;
        runCliCapture({"--mod-catalog", catalog, "--mod-format", "ips", "--mod-min-downloads", "800"}, output);
        QVERIFY(output.contains("Test Mod Alpha"));
        QVERIFY(!output.contains("Test Mod Beta"));
        QVERIFY(!output.contains("Test Mod Gamma"));
    }

    void testModSourceUrlFilterFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QString output;
        runCliCapture({"--mod-catalog", catalog, "--mod-source-url", "beta"}, output);
        QVERIFY(output.contains("Test Mod Beta"));
        QVERIFY(!output.contains("Test Mod Alpha"));
        QVERIFY(!output.contains("Test Mod Gamma"));
    }

    void testModListNoSystemFallback() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dbPath = dir.filePath("mods.db");
        const QString romPath = dir.filePath("game.sfc");

        QFile rom(romPath);
        QVERIFY(rom.open(QIODevice::WriteOnly));
        QVERIFY(rom.write(QByteArray(1024, '\0')) == 1024);
        rom.close();

        runCli({"--db", dbPath, "--scan", dir.path()});

        QString output;
        runCliCapture({"--db", dbPath, "--mod-catalog", catalog, "--mod-list", "1", "--mod-no-system-fallback"}, output);
        QVERIFY(output.contains("No exact-hash mods available"));
    }
};

QTEST_MAIN(CliSmokeTest)
#include "test_cli_smoke.moc"
