#include <QtTest/QtTest>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>

class CliSmokeTest : public QObject {
    Q_OBJECT

private:
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
        f.write("PK\x03\x04"); // minimal signature
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
        f.write("REM dummy cue\n");
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
};

QTEST_MAIN(CliSmokeTest)
#include "test_cli_smoke.moc"
