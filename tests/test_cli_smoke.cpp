#include <QtTest/QtTest>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>

#include <memory>

#include "../src/core/database.h"
#include "../src/core/constants/systems.h"
#include "../src/core/compendium_manifest_parser.h"
#include "../src/core/verification_engine.h"
#include "../src/cli/cli_compendium_build_phases.h"

class CliSmokeTest : public QObject {
    Q_OBJECT

private:
    mutable std::unique_ptr<QTemporaryDir> m_isolatedDataDir;
    bool m_skipBundledCompendium = false;

    void ensureIsolatedDataRoot() {
        if (!m_isolatedDataDir) {
            auto dir = std::make_unique<QTemporaryDir>();
            QVERIFY2(dir->isValid(), "Failed to create isolated REMUS_DATA_DIR for compendium tests");
            QDir root(dir->path());
            const QStringList subdirs
                = { QStringLiteral("metadata"), QStringLiteral("gametdb"), QStringLiteral("openvgdb"),
                      QStringLiteral("mame"), QStringLiteral("launchbox"), QStringLiteral("hasheous/dumps"),
                      QStringLiteral("acquisition/libretro-thumbnails"), QStringLiteral("remus-thumbnails") };
            for (const QString &sub : subdirs) {
                QVERIFY2(root.mkpath(QStringLiteral("data/") + sub),
                    qPrintable(QStringLiteral("Failed to create data/%1").arg(sub)));
            }
            m_isolatedDataDir = std::move(dir);
        }
    }

    QStringList withCompendiumTestFlags(QStringList args) const {
        if (!args.contains(QStringLiteral("--offline-only-enrichment"))) {
            args << QStringLiteral("--offline-only-enrichment");
        }
        if (!args.contains(QStringLiteral("--skip-consolidate-thumbnails"))) {
            args << QStringLiteral("--skip-consolidate-thumbnails");
        }
        return args;
    }

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
        return { };
    }

    QString fixtureDatChecksum() const {
        const QString path = fixturePath(QStringLiteral("test_compendium_source.dat"));
        return path.isEmpty() ? QString() : Remus::fileSha256Hex(path);
    }

    QString cliPath() const {
        QFileInfo fi(QCoreApplication::applicationDirPath() + "/../remus-cli");
        if (!fi.exists()) {
            fi.setFile(QCoreApplication::applicationDirPath() + "/remus-cli");
        }
        if (!fi.exists()) {
            return { };
        }
        return fi.absoluteFilePath();
    }

    QProcessEnvironment cliEnvironment() const {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("QT_QPA_PLATFORM", "offscreen");
        env.insert("QT_FORCE_STDERR_LOGGING", "1");
        env.insert("QT_LOGGING_RULES", "remus.cli.info=true");
        env.insert("LANG", "en_US.UTF-8");
        env.insert("LC_ALL", "en_US.UTF-8");
        if (m_isolatedDataDir) {
            env.insert(QStringLiteral("REMUS_DATA_DIR"), m_isolatedDataDir->path());
        }
        if (m_skipBundledCompendium) {
            env.insert(QStringLiteral("REMUS_TEST_NO_BUNDLED_COMPENDIUM"), QStringLiteral("1"));
        }
        return env;
    }

    void runCli(QStringList extraArgs, int expectedExit = 0) {
        if (extraArgs.contains(QStringLiteral("--build-compendium"))) {
            ensureIsolatedDataRoot();
            extraArgs = withCompendiumTestFlags(std::move(extraArgs));
        }
        QProcess proc;
        proc.setProcessEnvironment(cliEnvironment());
        const QStringList args = extraArgs;
        const QString binary = cliPath();
        QVERIFY2(!binary.isEmpty(), "remus-cli binary not found next to tests");
        proc.start(binary, args);
        QVERIFY2(proc.waitForStarted(), "CLI failed to start");
        const int timeoutMs = args.contains(QStringLiteral("--build-compendium")) ? 300000 : 30000;
        bool finished = proc.waitForFinished(timeoutMs);
        QVERIFY2(finished, "CLI did not finish in time");
        QCOMPARE(proc.exitCode(), expectedExit);
    }

    void runCliCapture(QStringList extraArgs, QString &output, int expectedExit = 0) {
        if (extraArgs.contains(QStringLiteral("--build-compendium"))) {
            ensureIsolatedDataRoot();
            extraArgs = withCompendiumTestFlags(std::move(extraArgs));
        }
        QProcess proc;
        proc.setProcessEnvironment(cliEnvironment());

        const QStringList args = extraArgs;
        const QString binary = cliPath();
        QVERIFY2(!binary.isEmpty(), "remus-cli binary not found next to tests");
        proc.start(binary, args);
        QVERIFY2(proc.waitForStarted(), "CLI failed to start");
        const int timeoutMs = args.contains(QStringLiteral("--build-compendium")) ? 300000 : 30000;
        bool finished = proc.waitForFinished(timeoutMs);
        QVERIFY2(finished, "CLI did not finish in time");
        QCOMPARE(proc.exitCode(), expectedExit);

        output = QString::fromUtf8(proc.readAllStandardOutput());
        const QString standardError = QString::fromUtf8(proc.readAllStandardError());
        if (!standardError.isEmpty()) {
            if (!output.isEmpty() && !output.endsWith(QLatin1Char('\n'))) {
                output += QLatin1Char('\n');
            }
            output += standardError;
        }
    }

    void runCliJson(const QStringList &extraArgs, QJsonDocument &doc, int expectedExit = 0) {
        QString output;
        runCliCapture(extraArgs, output, expectedExit);

        QJsonParseError parseError;
        doc = QJsonDocument::fromJson(output.toUtf8(), &parseError);
        QVERIFY2(parseError.error == QJsonParseError::NoError, qPrintable(parseError.errorString()));
        QVERIFY2(!doc.isNull(), "Expected valid JSON output");
    }

    QJsonObject findModById(const QJsonArray &mods, const QString &id) const {
        for (const auto &value : mods) {
            const QJsonObject object = value.toObject();
            if (object.value("id").toString() == id) {
                return object;
            }
        }
        return { };
    }

    static QString uniqueConnectionName(const QString &prefix) {
        return prefix + QString::number(QDateTime::currentMSecsSinceEpoch());
    }

private slots:
    void init() {
        m_isolatedDataDir.reset();
        m_skipBundledCompendium = false;
    }

    void initTestCase() {
        QCoreApplication::setOrganizationName("Remus");
        QCoreApplication::setApplicationName("RemusTest");
    }

    void testHelp() {
        runCli({ "--help" });
    }

    void testStatsNonInteractive() {
        runCli({ "--stats" });
    }

    void testCheckToolsExitsZeroAndListsAllTools() {
        QString output;
        runCliCapture({ "--check-tools" }, output, 0);
        QVERIFY2(output.contains("chdman"), "output should mention chdman");
        QVERIFY2(output.contains("dolphin-tool"), "output should mention dolphin-tool");
        QVERIFY2(output.contains("maxcso"), "output should mention maxcso");
        QVERIFY2(
            !output.contains("not found") || output.contains("Tool availability"), "header line should always appear");
    }

    void testExportDryRun() {
        QTemporaryDir dir;
        QString outPath = dir.filePath("export.csv");
        runCli({ "--export", "csv", "--export-path", outPath, "--dry-run-all" });
    }

    void testArchiveDryRun() {
        QTemporaryDir dir;
        QString archive = dir.filePath("dummy.zip");
        QFile f(archive);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("PK\x03\x04") == 4); // minimal signature
        f.close();
        runCli({ "--extract-archive", archive, "--dry-run-all" });
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
        runCli({ "--convert-chd", cue, "--dry-run-all" });
    }

    void testConvertCsoDryRun() {
        if (QStandardPaths::findExecutable("maxcso").isEmpty()) {
            QSKIP("maxcso not available");
        }
        QTemporaryDir dir;
        QString iso = dir.filePath("sample.iso");
        QFile f(iso);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("dummy iso") == 9);
        f.close();
        runCli({ "--convert-cso", iso, "--dry-run-all" });
    }

    void testExtractCsoDryRun() {
        if (QStandardPaths::findExecutable("maxcso").isEmpty()) {
            QSKIP("maxcso not available");
        }
        QTemporaryDir dir;
        QString cso = dir.filePath("sample.cso");
        QFile f(cso);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("dummy cso") == 9);
        f.close();
        runCli({ "--cso-extract", cso, "--dry-run-all" });
    }

    void testOrganizeDryRun() {
        QTemporaryDir dir;
        runCli({ "--organize", dir.path(), "--dry-run-all" });
    }

    void testPatchCreateDryRun() {
        QTemporaryDir dir;
        QString original = dir.filePath("orig.bin");
        QString modified = dir.filePath("mod.bin");
        runCli({ "--patch-create", modified, "--patch-original", original, "--patch-format", "bps", "--dry-run-all" });
    }

    void testScanEmptyDir() {
        QTemporaryDir dir;
        runCli({ "--scan", dir.path() });
    }

    void testVerifyUsesNormalizedDatSystemName() {
        m_skipBundledCompendium = true;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dbPath = dir.filePath("test.db");
        const QString romPath = dir.filePath("Test Game.nds");
        {
            QFile romFile(romPath);
            QVERIFY(romFile.open(QIODevice::WriteOnly));
            QVERIFY(romFile.write("ABCD") == 4);
        }

        const QString datPath = dir.filePath("Nintendo - Nintendo DS (Community Set).dat");
        {
            QFile datFile(datPath);
            QVERIFY(datFile.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&datFile);
            out << "clrmamepro (\n";
            out << "  name \"Nintendo - Nintendo DS (Community Set)\"\n";
            out << "  description \"Normalization test DAT\"\n";
            out << "  version \"1.0\"\n";
            out << ")\n";
            out << "game (\n";
            out << "  name \"Test Game\"\n";
            out << "  rom ( name \"Test Game.nds\" size 4 crc DB1720A5 )\n";
            out << ")\n";
        }

        runCli({ "--db", dbPath, "--scan", dir.path(), "--hash" });

        QString output;
        runCliCapture({ "--db", dbPath, "--verify", datPath }, output);

        QVERIFY2(output.contains(QStringLiteral("System: \"Nintendo DS\"")),
            qPrintable(QStringLiteral("Captured output:\n%1").arg(output)));
        QVERIFY2(output.contains("Total files: 1"), qPrintable(QStringLiteral("Captured output:\n%1").arg(output)));
    }

    void testBuildCompendiumCreatesDatabaseAndReport() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString sourcePath = fixturePath("test_compendium_source.dat");
        QVERIFY2(!sourcePath.isEmpty(), "Fixture test_compendium_source.dat not found");

        const QString manifestPath = dir.filePath("manifest.json");
        {
            QFile manifestFile(manifestPath);
            QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));

            QJsonObject sourceObject;
            sourceObject.insert("source_id", "test-source");
            sourceObject.insert("display_name", "Test Source");
            sourceObject.insert("source_type", "dat");
            sourceObject.insert("snapshot_id", "snapshot-001");
            sourceObject.insert("snapshot_label", "Snapshot 001");
            sourceObject.insert("snapshot_ref", "test-ref");
            sourceObject.insert("path", sourcePath);
            sourceObject.insert("checksum_sha256", fixtureDatChecksum());
            sourceObject.insert("enabled", true);
            sourceObject.insert("priority", 10);

            QJsonObject manifestObject;
            manifestObject.insert("build_id", "test-build");
            manifestObject.insert("schema_version", 1);
            manifestObject.insert("sources", QJsonArray { sourceObject });

            const QByteArray manifestJson = QJsonDocument(manifestObject).toJson(QJsonDocument::Indented);
            QVERIFY(manifestFile.write(manifestJson) == manifestJson.size());
        }

        const QString outputDbPath = dir.filePath("remus_compendium_test.db");
        const QString reportPath = dir.filePath("remus_compendium_test.report.json");

        QString output;
        runCliCapture(
            { "--build-compendium", "--compendium-manifest", manifestPath, "--compendium-output", outputDbPath },
            output);

        QVERIFY2(QFile::exists(outputDbPath), qPrintable(output));
        QVERIFY2(QFile::exists(reportPath), qPrintable(output));
        QVERIFY2(output.contains("Build ID:"), qPrintable(output));
        QVERIFY2(output.contains("Sources recorded:"), qPrintable(output));

        QFile reportFile(reportPath);
        QVERIFY(reportFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QJsonDocument reportDoc = QJsonDocument::fromJson(reportFile.readAll());
        QVERIFY(reportDoc.isObject());
        QCOMPARE(reportDoc.object().value("build_id").toString(), QString("test-build"));
        QCOMPARE(reportDoc.object().value("schema_version").toInt(), 1);
        QVERIFY(reportDoc.object().value("records_ingested").toInt() > 0);
        QVERIFY(reportDoc.object().value("games_created").toInt() > 0);
        QVERIFY(reportDoc.object().value("signatures_created").toInt() > 0);
        QVERIFY(reportDoc.object().value("serials_created").toInt() > 0);
        QVERIFY(reportDoc.object().value("facts_created").toInt() > 0);
        QVERIFY(reportDoc.object().value("resolved_fields").toInt() > 0);
        QCOMPARE(reportDoc.object().value("unresolved_conflicts").toInt(), 0);
        QVERIFY(reportDoc.object().contains(QStringLiteral("enrichment_passes_executed")));
        QVERIFY(reportDoc.object().contains(QStringLiteral("post_enrich_fts_rows_indexed")));
        QVERIFY(reportDoc.object().value(QStringLiteral("post_enrich_fts_rows_indexed")).toInt() > 0);

        const QString connectionName
            = QStringLiteral("compendium_smoke_%1").arg(QString::number(QDateTime::currentMSecsSinceEpoch()));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
            db.setDatabaseName(outputDbPath);
            QVERIFY2(db.open(), qPrintable(db.lastError().text()));

            QSqlQuery countsQuery(db);
            QVERIFY2(
                countsQuery.exec("SELECT COUNT(*) FROM compendium_builds"), qPrintable(countsQuery.lastError().text()));
            QVERIFY(countsQuery.next());
            QCOMPARE(countsQuery.value(0).toInt(), 1);

            QVERIFY2(countsQuery.exec("SELECT COUNT(*) FROM sources"), qPrintable(countsQuery.lastError().text()));
            QVERIFY(countsQuery.next());
            QVERIFY(countsQuery.value(0).toInt() >= 1);

            QVERIFY2(
                countsQuery.exec("SELECT COUNT(*) FROM source_snapshots"), qPrintable(countsQuery.lastError().text()));
            QVERIFY(countsQuery.next());
            QVERIFY(countsQuery.value(0).toInt() >= 1);

            QVERIFY2(countsQuery.exec("SELECT COUNT(*) FROM systems"), qPrintable(countsQuery.lastError().text()));
            QVERIFY(countsQuery.next());
            const int expectedSystemCount = Remus::Constants::Systems::getSystemInternalNames().size();
            QCOMPARE(countsQuery.value(0).toInt(), expectedSystemCount);

            QVERIFY2(countsQuery.exec("SELECT COUNT(*) FROM games"), qPrintable(countsQuery.lastError().text()));
            QVERIFY(countsQuery.next());
            QVERIFY(countsQuery.value(0).toInt() > 0);

            QVERIFY2(
                countsQuery.exec("SELECT COUNT(*) FROM game_signatures"), qPrintable(countsQuery.lastError().text()));
            QVERIFY(countsQuery.next());
            QVERIFY(countsQuery.value(0).toInt() > 0);

            QVERIFY2(countsQuery.exec("SELECT COUNT(*) FROM source_items"), qPrintable(countsQuery.lastError().text()));
            QVERIFY(countsQuery.next());
            QVERIFY(countsQuery.value(0).toInt() > 0);

            QVERIFY2(countsQuery.exec("SELECT COUNT(*) FROM games_fts"), qPrintable(countsQuery.lastError().text()));
            QVERIFY(countsQuery.next());
            QVERIFY(countsQuery.value(0).toInt() > 0);

            QVERIFY2(countsQuery.exec("SELECT COUNT(*) FROM games_search"), qPrintable(countsQuery.lastError().text()));
            QVERIFY(countsQuery.next());
            QVERIFY(countsQuery.value(0).toInt() > 0);

            QSqlQuery sourceQuery(db);
            QVERIFY2(sourceQuery.exec("SELECT source_id, display_name, enabled, priority FROM sources "
                                      "WHERE source_id = 'test-source'"),
                qPrintable(sourceQuery.lastError().text()));
            QVERIFY(sourceQuery.next());
            QCOMPARE(sourceQuery.value(0).toString(), QString("test-source"));
            QCOMPARE(sourceQuery.value(1).toString(), QString("Test Source"));
            QCOMPARE(sourceQuery.value(2).toInt(), 1);
            QCOMPARE(sourceQuery.value(3).toInt(), 10);

            db.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }

    void testBuildCompendiumRejectsChecksumMismatch() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString sourcePath = fixturePath(QStringLiteral("test_compendium_source.dat"));
        QVERIFY2(!sourcePath.isEmpty(), "Fixture test_compendium_source.dat not found");

        const QString manifestPath = dir.filePath(QStringLiteral("manifest_bad_checksum.json"));
        {
            QFile manifestFile(manifestPath);
            QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));

            QJsonObject sourceObject;
            sourceObject.insert(QStringLiteral("source_id"), QStringLiteral("test-source"));
            sourceObject.insert(QStringLiteral("display_name"), QStringLiteral("Test Source"));
            sourceObject.insert(QStringLiteral("source_type"), QStringLiteral("dat"));
            sourceObject.insert(QStringLiteral("snapshot_id"), QStringLiteral("snapshot-001"));
            sourceObject.insert(QStringLiteral("snapshot_label"), QStringLiteral("Snapshot 001"));
            sourceObject.insert(QStringLiteral("path"), sourcePath);
            sourceObject.insert(QStringLiteral("checksum_sha256"), QStringLiteral("deadbeef"));
            sourceObject.insert(QStringLiteral("enabled"), true);
            sourceObject.insert(QStringLiteral("priority"), 10);

            QJsonObject manifestObject;
            manifestObject.insert(QStringLiteral("build_id"), QStringLiteral("test-build-bad-checksum"));
            manifestObject.insert(QStringLiteral("schema_version"), 1);
            manifestObject.insert(QStringLiteral("sources"), QJsonArray { sourceObject });

            const QByteArray manifestJson = QJsonDocument(manifestObject).toJson(QJsonDocument::Indented);
            QVERIFY(manifestFile.write(manifestJson) == manifestJson.size());
        }

        const QString outputDbPath = dir.filePath(QStringLiteral("remus_compendium_bad_checksum.db"));
        QString output;
        runCliCapture({ QStringLiteral("--build-compendium"), QStringLiteral("--compendium-manifest"), manifestPath,
                          QStringLiteral("--compendium-output"), outputDbPath },
            output, 1);
        QVERIFY2(output.contains(QStringLiteral("checksum mismatch")), qPrintable(output));
        QVERIFY2(!QFile::exists(outputDbPath), qPrintable(output));
    }

    void testBuildCompendiumFailurePreservesExistingDatabase() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString existingDbPath = dir.filePath("remus_compendium_test.db");
        const QString seedConnectionName = uniqueConnectionName(QStringLiteral("compendium_seed_"));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", seedConnectionName);
            db.setDatabaseName(existingDbPath);
            QVERIFY2(db.open(), qPrintable(db.lastError().text()));

            QSqlQuery query(db);
            QVERIFY2(query.exec("CREATE TABLE sentinel (value TEXT NOT NULL)"), qPrintable(query.lastError().text()));
            QVERIFY2(
                query.exec("INSERT INTO sentinel (value) VALUES ('keep-me')"), qPrintable(query.lastError().text()));
            db.close();
        }
        QSqlDatabase::removeDatabase(seedConnectionName);

        const QString sourcePath = fixturePath("test_compendium_source.dat");
        QVERIFY2(!sourcePath.isEmpty(), "Fixture test_compendium_source.dat not found");

        const QString manifestPath = dir.filePath("manifest_fail.json");
        {
            QFile manifestFile(manifestPath);
            QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));

            QJsonObject badSourceA;
            badSourceA.insert("source_id", "duplicate-source");
            badSourceA.insert("display_name", "Duplicate Source A");
            badSourceA.insert("source_type", "dat");
            badSourceA.insert("snapshot_id", "snapshot-001");
            badSourceA.insert("snapshot_label", "Snapshot 001");
            badSourceA.insert("path", sourcePath);
            badSourceA.insert("checksum_sha256", fixtureDatChecksum());
            badSourceA.insert("enabled", true);
            badSourceA.insert("priority", 10);

            QJsonObject badSourceB = badSourceA;
            badSourceB.insert("display_name", "Duplicate Source B");
            badSourceB.insert("snapshot_id", "snapshot-002");

            QJsonObject manifestObject;
            manifestObject.insert("build_id", "test-build-failure");
            manifestObject.insert("schema_version", 1);
            manifestObject.insert("sources", QJsonArray { badSourceA, badSourceB });

            const QByteArray manifestJson = QJsonDocument(manifestObject).toJson(QJsonDocument::Indented);
            QVERIFY(manifestFile.write(manifestJson) == manifestJson.size());
        }

        QString output;
        runCliCapture(
            { "--build-compendium", "--compendium-manifest", manifestPath, "--compendium-output", existingDbPath },
            output, 1);

        QVERIFY2(QFile::exists(existingDbPath), qPrintable(output));

        const QString verifyConnectionName = uniqueConnectionName(QStringLiteral("compendium_verify_"));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", verifyConnectionName);
            db.setDatabaseName(existingDbPath);
            QVERIFY2(db.open(), qPrintable(db.lastError().text()));

            QSqlQuery query(db);
            QVERIFY2(query.exec("SELECT value FROM sentinel"), qPrintable(query.lastError().text()));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toString(), QStringLiteral("keep-me"));
            db.close();
        }
        QSqlDatabase::removeDatabase(verifyConnectionName);
    }

    void testBuildCompendiumSkipsOnlyEquivalentManifest() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString sourcePath = fixturePath("test_compendium_source.dat");
        QVERIFY2(!sourcePath.isEmpty(), "Fixture test_compendium_source.dat not found");

        const QString manifestPath = dir.filePath("manifest_skip.json");
        const QString outputDbPath = dir.filePath("remus_compendium_skip_test.db");

        const auto writeManifest = [&](bool enabled) {
            QFile manifestFile(manifestPath);
            QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));

            QJsonObject sourceObject;
            sourceObject.insert("source_id", "test-source");
            sourceObject.insert("display_name", "Test Source");
            sourceObject.insert("source_type", "dat");
            sourceObject.insert("snapshot_id", "snapshot-001");
            sourceObject.insert("snapshot_label", "Snapshot 001");
            sourceObject.insert("snapshot_ref", "test-ref");
            sourceObject.insert("path", sourcePath);
            sourceObject.insert("checksum_sha256", fixtureDatChecksum());
            sourceObject.insert("enabled", enabled);
            sourceObject.insert("priority", 10);

            QJsonObject manifestObject;
            manifestObject.insert("build_id", "test-build-skip");
            manifestObject.insert("schema_version", 1);
            manifestObject.insert("sources", QJsonArray { sourceObject });

            const QByteArray manifestJson = QJsonDocument(manifestObject).toJson(QJsonDocument::Indented);
            QVERIFY(manifestFile.write(manifestJson) == manifestJson.size());
            manifestFile.close();
        };

        writeManifest(true);

        QString firstOutput;
        runCliCapture(
            { "--build-compendium", "--compendium-manifest", manifestPath, "--compendium-output", outputDbPath },
            firstOutput);
        QVERIFY2(QFile::exists(outputDbPath), qPrintable(firstOutput));
        QVERIFY2(!firstOutput.contains("skipping rebuild", Qt::CaseInsensitive), qPrintable(firstOutput));

        QString secondOutput;
        runCliCapture(
            { "--build-compendium", "--compendium-manifest", manifestPath, "--compendium-output", outputDbPath },
            secondOutput);
        QVERIFY2(secondOutput.contains("skipping rebuild", Qt::CaseInsensitive), qPrintable(secondOutput));

        writeManifest(false);

        QString thirdOutput;
        runCliCapture(
            { "--build-compendium", "--compendium-manifest", manifestPath, "--compendium-output", outputDbPath },
            thirdOutput);
        QVERIFY2(thirdOutput.contains("syncing manifest metadata", Qt::CaseInsensitive), qPrintable(thirdOutput));
        QVERIFY2(!thirdOutput.contains("skipping rebuild", Qt::CaseInsensitive), qPrintable(thirdOutput));

        const QString verifyConnectionName = uniqueConnectionName(QStringLiteral("compendium_skip_verify_"));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", verifyConnectionName);
            db.setDatabaseName(outputDbPath);
            QVERIFY2(db.open(), qPrintable(db.lastError().text()));

            QSqlQuery query(db);
            QVERIFY2(query.exec("SELECT enabled FROM sources WHERE source_id = 'test-source'"),
                qPrintable(query.lastError().text()));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 0);

            QVERIFY2(
                query.exec("SELECT source_manifest_json FROM compendium_builds WHERE build_id = 'test-build-skip'"),
                qPrintable(query.lastError().text()));
            QVERIFY(query.next());
            QVERIFY(query.value(0).toString().contains("\"enabled\":false"));
            db.close();
        }
        QSqlDatabase::removeDatabase(verifyConnectionName);
    }

    void testModSystemsFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QJsonDocument doc;
        runCliJson({ "--mod-catalog", catalog, "--mod-systems", "--json" }, doc);
        QVERIFY(doc.isArray());

        const QJsonArray systems = doc.array();
        bool foundSnes = false;
        bool foundGenesis = false;
        for (const auto &value : systems) {
            const QJsonObject object = value.toObject();
            if (object.value("system").toString() == "Super Nintendo") {
                foundSnes = true;
            }
            if (object.value("system").toString() == "Sega Genesis") {
                foundGenesis = true;
            }
        }
        QVERIFY(foundSnes);
        QVERIFY(foundGenesis);
    }

    void testModShowFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QJsonDocument doc;
        runCliJson({ "--mod-catalog", catalog, "--mod-show", "test-mod-alpha", "--json" }, doc);
        QVERIFY(doc.isObject());

        const QJsonObject mod = doc.object();
        QCOMPARE(mod.value("title").toString(), QString("Test Mod Alpha"));
        QCOMPARE(mod.value("author").toString(), QString("Test Author"));
        QCOMPARE(mod.value("patchUrl").toString(), QString("file:///tmp/test-patch.ips"));
    }

    void testModSystemFilterFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QJsonDocument doc;
        runCliJson({ "--mod-catalog", catalog, "--mod-system", "Super Nintendo", "--json" }, doc);
        QVERIFY(doc.isArray());

        const QJsonArray mods = doc.array();
        QVERIFY(!findModById(mods, "test-mod-alpha").isEmpty());
        QVERIFY(!findModById(mods, "test-mod-beta").isEmpty());
        QVERIFY(findModById(mods, "test-mod-gamma").isEmpty());
    }

    void testModAuthorFilterFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QJsonDocument doc;
        runCliJson({ "--mod-catalog", catalog, "--mod-author", "Test", "--json" }, doc);
        QVERIFY(doc.isArray());

        const QJsonArray mods = doc.array();
        QVERIFY(!findModById(mods, "test-mod-alpha").isEmpty());
        QVERIFY(findModById(mods, "test-mod-beta").isEmpty());
        QVERIFY(findModById(mods, "test-mod-gamma").isEmpty());
    }

    void testModTypeAndRatingFiltersFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QJsonDocument doc;
        runCliJson({ "--mod-catalog", catalog, "--mod-type", "translation", "--mod-min-rating", "3.5", "--json" }, doc);
        QVERIFY(doc.isArray());

        const QJsonArray mods = doc.array();
        QVERIFY(!findModById(mods, "test-mod-beta").isEmpty());
        QVERIFY(findModById(mods, "test-mod-alpha").isEmpty());
        QVERIFY(findModById(mods, "test-mod-gamma").isEmpty());
    }

    void testModRatingFilterRejectsInvalidInput() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        runCli({ "--mod-catalog", catalog, "--mod-min-rating", "9.0" }, 1);
    }

    void testModJsonSystemsOutput() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QJsonDocument doc;
        runCliJson({ "--mod-catalog", catalog, "--mod-systems", "--json" }, doc);
        QVERIFY(doc.isArray());
        QVERIFY(doc.array().size() >= 2);
        QVERIFY(doc.array().first().toObject().contains("system"));
    }

    void testModSortByDownloads() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QJsonDocument doc;
        runCliJson(
            { "--mod-catalog", catalog, "--mod-system", "Super Nintendo", "--mod-sort", "downloads", "--json" }, doc);
        QVERIFY(doc.isArray());

        const QJsonArray mods = doc.array();
        QCOMPARE(mods.size(), 2);
        QCOMPARE(mods.at(0).toObject().value("id").toString(), QString("test-mod-alpha"));
        QCOMPARE(mods.at(1).toObject().value("id").toString(), QString("test-mod-beta"));
    }

    void testModFormatAndDownloadsFiltersFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QJsonDocument doc;
        runCliJson({ "--mod-catalog", catalog, "--mod-format", "ips", "--mod-min-downloads", "800", "--json" }, doc);
        QVERIFY(doc.isArray());

        const QJsonArray mods = doc.array();
        QVERIFY(!findModById(mods, "test-mod-alpha").isEmpty());
        QVERIFY(findModById(mods, "test-mod-beta").isEmpty());
        QVERIFY(findModById(mods, "test-mod-gamma").isEmpty());
    }

    void testModSourceUrlFilterFromCatalog() {
        const QString catalog = fixturePath("test_mod_catalog.json");
        QVERIFY2(!catalog.isEmpty(), "Fixture test_mod_catalog.json not found");

        QJsonDocument doc;
        runCliJson({ "--mod-catalog", catalog, "--mod-source-url", "beta", "--json" }, doc);
        QVERIFY(doc.isArray());

        const QJsonArray mods = doc.array();
        QVERIFY(!findModById(mods, "test-mod-beta").isEmpty());
        QVERIFY(findModById(mods, "test-mod-alpha").isEmpty());
        QVERIFY(findModById(mods, "test-mod-gamma").isEmpty());
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

        runCli({ "--db", dbPath, "--scan", dir.path() });

        QJsonDocument doc;
        runCliJson(
            { "--db", dbPath, "--mod-catalog", catalog, "--mod-list", "1", "--mod-no-system-fallback", "--json" }, doc);
        QVERIFY(doc.isArray());
        QVERIFY(doc.array().isEmpty());
    }

    void testScanCombinedWithGenerateM3uUsesWholeDatabase() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dbPath = dir.filePath("cli.db");
        const QString libraryA = dir.filePath("library-a");
        const QString libraryB = dir.filePath("library-b");
        const QString outputDir = dir.filePath("m3u-out");
        QVERIFY(QDir().mkpath(libraryA));
        QVERIFY(QDir().mkpath(libraryB));
        QVERIFY(QDir().mkpath(outputDir));

        QFile disc1(libraryA + "/Metal Gear Solid (USA) (Disc 1).chd");
        QVERIFY(disc1.open(QIODevice::WriteOnly));
        QVERIFY(disc1.write("disc1") == 5);
        disc1.close();

        QFile disc2(libraryA + "/Metal Gear Solid (USA) (Disc 2).chd");
        QVERIFY(disc2.open(QIODevice::WriteOnly));
        QVERIFY(disc2.write("disc2") == 5);
        disc2.close();

        QFile other(libraryB + "/Other Game.chd");
        QVERIFY(other.open(QIODevice::WriteOnly));
        QVERIFY(other.write("other") == 5);
        other.close();

        runCli({ "--db", dbPath, "--scan", libraryA });
        runCli({ "--db", dbPath, "--scan", libraryB, "--generate-m3u", "--m3u-dir", outputDir });

        QVERIFY(QFile::exists(outputDir + "/Metal Gear Solid.m3u"));
    }

    void testVerifyXmlDatUsesHeaderSystemName() {
        m_skipBundledCompendium = true;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dbPath = dir.filePath("test.db");
        const QString romPath = dir.filePath("Test Game.nds");
        {
            QFile romFile(romPath);
            QVERIFY(romFile.open(QIODevice::WriteOnly));
            QVERIFY(romFile.write("ABCD") == 4);
        }

        const QString datPath = dir.filePath("test.dat");
        {
            QFile datFile(datPath);
            QVERIFY(datFile.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&datFile);
            out << "<?xml version=\"1.0\"?>\n";
            out << "<datafile>\n";
            out << "  <header>\n";
            out << "    <name>Nintendo - Nintendo DS</name>\n";
            out << "    <description>Normalization test DAT</description>\n";
            out << "    <version>1.0</version>\n";
            out << "  </header>\n";
            out << "  <game name=\"Test Game\">\n";
            out << "    <description>Test Game</description>\n";
            out << "    <rom name=\"Test Game.nds\" size=\"4\" crc=\"db1720a5\"/>\n";
            out << "  </game>\n";
            out << "</datafile>\n";
        }

        runCli({ "--db", dbPath, "--scan", dir.path(), "--hash" });
        runCli({ "--db", dbPath, "--verify", datPath });

        Remus::Database db;
        QVERIFY(db.initialize(dbPath));
        Remus::VerificationEngine engine(&db);
        const QMap<QString, Remus::DatHeader> imported = engine.getImportedDats();
        QVERIFY(imported.contains("Nintendo DS"));
        QVERIFY(!imported.contains("test"));
    }

    void testBuildCompendiumSkipRequiresReportPresent() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString sourcePath = fixturePath("test_compendium_source.dat");
        QVERIFY2(!sourcePath.isEmpty(), "Fixture test_compendium_source.dat not found");

        const QString manifestPath = dir.filePath("manifest_sidecar.json");
        {
            QFile manifestFile(manifestPath);
            QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));

            QJsonObject sourceObject;
            sourceObject.insert("source_id", "test-source");
            sourceObject.insert("display_name", "Test Source");
            sourceObject.insert("source_type", "dat");
            sourceObject.insert("snapshot_id", "snapshot-001");
            sourceObject.insert("snapshot_label", "Snapshot 001");
            sourceObject.insert("snapshot_ref", "test-ref");
            sourceObject.insert("path", sourcePath);
            sourceObject.insert("checksum_sha256", fixtureDatChecksum());
            sourceObject.insert("enabled", true);
            sourceObject.insert("priority", 10);

            QJsonObject manifestObject;
            manifestObject.insert("build_id", "test-build-sidecar");
            manifestObject.insert("schema_version", 1);
            manifestObject.insert("sources", QJsonArray { sourceObject });

            const QByteArray manifestJson = QJsonDocument(manifestObject).toJson(QJsonDocument::Indented);
            QVERIFY(manifestFile.write(manifestJson) == manifestJson.size());
        }

        const QString outputDbPath = dir.filePath("remus_compendium_sidecar.db");
        const QString reportPath = dir.filePath("remus_compendium_sidecar.report.json");

        // First build — must produce both DB and report.
        runCli({ "--build-compendium", "--compendium-manifest", manifestPath, "--compendium-output", outputDbPath });
        QVERIFY2(QFile::exists(outputDbPath), "First build did not create the DB");
        QVERIFY2(QFile::exists(reportPath), "First build did not create the report");

        // Delete only the report sidecar.
        QVERIFY(QFile::remove(reportPath));

        // Second build — must NOT skip (report missing), and must regenerate the report.
        QString secondOutput;
        runCliCapture(
            { "--build-compendium", "--compendium-manifest", manifestPath, "--compendium-output", outputDbPath },
            secondOutput);
        QVERIFY2(!secondOutput.contains("skipping rebuild", Qt::CaseInsensitive),
            qPrintable(QStringLiteral("Expected rebuild but got skip: %1").arg(secondOutput)));
        QVERIFY2(QFile::exists(reportPath), qPrintable(QStringLiteral("Report not regenerated: %1").arg(secondOutput)));
    }

    void testBuildCompendiumRejectsInvalidSourceType() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString manifestPath = dir.filePath("manifest_invalid_type.json");
        {
            QFile manifestFile(manifestPath);
            QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));

            QJsonObject sourceObject;
            sourceObject.insert("source_id", "bad-source");
            sourceObject.insert("display_name", "Bad Source");
            sourceObject.insert("source_type", "csv"); // not in the allowed list
            sourceObject.insert("snapshot_id", "snapshot-001");
            sourceObject.insert("snapshot_label", "Snapshot 001");
            sourceObject.insert("path", "/nonexistent/path.csv");
            sourceObject.insert("enabled", true);
            sourceObject.insert("priority", 5);

            QJsonObject manifestObject;
            manifestObject.insert("build_id", "test-invalid-type");
            manifestObject.insert("schema_version", 1);
            manifestObject.insert("sources", QJsonArray { sourceObject });

            const QByteArray manifestJson = QJsonDocument(manifestObject).toJson(QJsonDocument::Indented);
            QVERIFY(manifestFile.write(manifestJson) == manifestJson.size());
        }

        const QString outputDbPath = dir.filePath("remus_compendium_invalid.db");

        // Parser must reject the unrecognised source_type before any DB is created.
        runCli({ "--build-compendium", "--compendium-manifest", manifestPath, "--compendium-output", outputDbPath }, 1);
        QVERIFY2(!QFile::exists(outputDbPath), "DB should not be created when source_type is invalid");
    }

    void testBuildCompendiumRejectsEmptyEnabledSource() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString emptyDatPath = dir.filePath("empty.dat");
        {
            QFile emptyDat(emptyDatPath);
            QVERIFY(emptyDat.open(QIODevice::WriteOnly | QIODevice::Text));
            QVERIFY(emptyDat.write("clrmamepro (\n    name \"Empty\"\n)\n") > 0);
        }

        const QString manifestPath = dir.filePath("manifest_empty_source.json");
        {
            QFile manifestFile(manifestPath);
            QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));

            QJsonObject sourceObject;
            sourceObject.insert("source_id", "empty-source");
            sourceObject.insert("display_name", "Empty Source");
            sourceObject.insert("source_type", "dat");
            sourceObject.insert("snapshot_id", "snapshot-empty");
            sourceObject.insert("snapshot_label", "Empty");
            sourceObject.insert("path", emptyDatPath);
            sourceObject.insert("checksum_sha256", Remus::fileSha256Hex(emptyDatPath));
            sourceObject.insert("enabled", true);
            sourceObject.insert("priority", 10);

            QJsonObject manifestObject;
            manifestObject.insert("build_id", "test-empty-source");
            manifestObject.insert("schema_version", 1);
            manifestObject.insert("sources", QJsonArray { sourceObject });

            const QByteArray manifestJson = QJsonDocument(manifestObject).toJson(QJsonDocument::Indented);
            QVERIFY(manifestFile.write(manifestJson) == manifestJson.size());
        }

        const QString outputDbPath = dir.filePath("remus_compendium_empty.db");
        runCli({ "--build-compendium", "--compendium-manifest", manifestPath, "--compendium-output", outputDbPath }, 1);
        QVERIFY2(!QFile::exists(outputDbPath), "DB should not be promoted when an enabled source yields zero records");
    }

    void testBuildCompendium_enrichmentOnlyWhenFingerprintChanges() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString sourcePath = fixturePath(QStringLiteral("test_compendium_source.dat"));
        QVERIFY2(!sourcePath.isEmpty(), "Fixture test_compendium_source.dat not found");

        const QString manifestPath = dir.filePath(QStringLiteral("manifest_enrich_only.json"));
        const QString outputDbPath = dir.filePath(QStringLiteral("remus_compendium_enrich_only.db"));
        const QString reportPath = dir.filePath(QStringLiteral("remus_compendium_enrich_only.report.json"));

        {
            QFile manifestFile(manifestPath);
            QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));
            QJsonObject sourceObject;
            sourceObject.insert(QStringLiteral("source_id"), QStringLiteral("test-source"));
            sourceObject.insert(QStringLiteral("display_name"), QStringLiteral("Test Source"));
            sourceObject.insert(QStringLiteral("source_type"), QStringLiteral("dat"));
            sourceObject.insert(QStringLiteral("snapshot_id"), QStringLiteral("snapshot-001"));
            sourceObject.insert(QStringLiteral("snapshot_label"), QStringLiteral("Snapshot 001"));
            sourceObject.insert(QStringLiteral("snapshot_ref"), QStringLiteral("test-ref"));
            sourceObject.insert(QStringLiteral("path"), sourcePath);
            sourceObject.insert(QStringLiteral("checksum_sha256"), fixtureDatChecksum());
            sourceObject.insert(QStringLiteral("enabled"), true);
            sourceObject.insert(QStringLiteral("priority"), 10);

            QJsonObject manifestObject;
            manifestObject.insert(QStringLiteral("build_id"), QStringLiteral("test-enrich-only"));
            manifestObject.insert(QStringLiteral("schema_version"), 1);
            manifestObject.insert(QStringLiteral("sources"), QJsonArray { sourceObject });
            const QByteArray manifestJson = QJsonDocument(manifestObject).toJson(QJsonDocument::Indented);
            QVERIFY(manifestFile.write(manifestJson) == manifestJson.size());
        }

        runCli({ QStringLiteral("--build-compendium"), QStringLiteral("--compendium-manifest"), manifestPath,
            QStringLiteral("--compendium-output"), outputDbPath });
        QVERIFY2(QFile::exists(outputDbPath), "Initial build failed");
        QVERIFY2(QFile::exists(reportPath), "Initial report missing");

        const QString credPath = dir.filePath(QStringLiteral("enrichment-credentials.json"));
        {
            QFile credFile(credPath);
            QVERIFY(credFile.open(QIODevice::WriteOnly | QIODevice::Text));
            credFile.write("{\"providers\":{\"hasheous\":{\"client_api_key\":\"test-key\"}}}\n");
        }

        QString secondOutput;
        runCliCapture({ QStringLiteral("--build-compendium"), QStringLiteral("--compendium-manifest"), manifestPath,
                          QStringLiteral("--compendium-output"), outputDbPath },
            secondOutput);
        QVERIFY2(secondOutput.contains(QStringLiteral("enrichment-only refresh"), Qt::CaseInsensitive),
            qPrintable(secondOutput));
        QVERIFY2(!secondOutput.contains(QStringLiteral("Incremental ingest"), Qt::CaseInsensitive),
            qPrintable(secondOutput));

        QFile reportFile(reportPath);
        QVERIFY(reportFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QJsonObject report = QJsonDocument::fromJson(reportFile.readAll()).object();
        QCOMPARE(report.value(QStringLiteral("build_mode")).toString(), QStringLiteral("enrichment_only"));
    }

    void testBuildCompendium_incrementalIngestOnChecksumChange() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString sourcePath = dir.filePath(QStringLiteral("test_source_v1.dat"));
        {
            QFile src(fixturePath(QStringLiteral("test_compendium_source.dat")));
            QVERIFY(src.open(QIODevice::ReadOnly));
            const QByteArray bytes = src.readAll();
            src.close();
            QFile dest(sourcePath);
            QVERIFY(dest.open(QIODevice::WriteOnly));
            QVERIFY(dest.write(bytes) == bytes.size());
        }

        const QString manifestPath = dir.filePath(QStringLiteral("manifest_incremental.json"));
        const QString outputDbPath = dir.filePath(QStringLiteral("remus_compendium_incremental.db"));

        const auto writeManifest = [&](const QString &datPath, const QString &snapshotId) {
            QFile manifestFile(manifestPath);
            QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));
            QJsonObject sourceObject;
            sourceObject.insert(QStringLiteral("source_id"), QStringLiteral("test-source"));
            sourceObject.insert(QStringLiteral("display_name"), QStringLiteral("Test Source"));
            sourceObject.insert(QStringLiteral("source_type"), QStringLiteral("dat"));
            sourceObject.insert(QStringLiteral("snapshot_id"), snapshotId);
            sourceObject.insert(QStringLiteral("snapshot_label"), QStringLiteral("Snapshot 001"));
            sourceObject.insert(QStringLiteral("snapshot_ref"), QStringLiteral("test-ref"));
            sourceObject.insert(QStringLiteral("path"), datPath);
            sourceObject.insert(QStringLiteral("checksum_sha256"), Remus::fileSha256Hex(datPath));
            sourceObject.insert(QStringLiteral("enabled"), true);
            sourceObject.insert(QStringLiteral("priority"), 10);

            QJsonObject manifestObject;
            manifestObject.insert(QStringLiteral("build_id"), QStringLiteral("test-incremental"));
            manifestObject.insert(QStringLiteral("schema_version"), 1);
            manifestObject.insert(QStringLiteral("sources"), QJsonArray { sourceObject });
            const QByteArray manifestJson = QJsonDocument(manifestObject).toJson(QJsonDocument::Indented);
            QVERIFY(manifestFile.write(manifestJson) == manifestJson.size());
        };

        writeManifest(sourcePath, QStringLiteral("snapshot-001"));
        runCli({ QStringLiteral("--build-compendium"), QStringLiteral("--compendium-manifest"), manifestPath,
            QStringLiteral("--compendium-output"), outputDbPath });
        QVERIFY2(QFile::exists(outputDbPath), "Initial build failed");

        const QString changedDatPath = dir.filePath(QStringLiteral("test_source_v2.dat"));
        {
            QFile src(sourcePath);
            QVERIFY(src.open(QIODevice::ReadOnly));
            QByteArray bytes = src.readAll();
            src.close();
            bytes.append("\n// incremental-ingest checksum bump\n");
            QFile dest(changedDatPath);
            QVERIFY(dest.open(QIODevice::WriteOnly));
            QVERIFY(dest.write(bytes) == bytes.size());
        }

        writeManifest(changedDatPath, QStringLiteral("snapshot-002"));

        QString secondOutput;
        runCliCapture({ QStringLiteral("--build-compendium"), QStringLiteral("--compendium-manifest"), manifestPath,
                          QStringLiteral("--compendium-output"), outputDbPath },
            secondOutput);
        QVERIFY2(
            secondOutput.contains(QStringLiteral("Incremental ingest"), Qt::CaseInsensitive), qPrintable(secondOutput));

        QFile reportFile(dir.filePath(QStringLiteral("remus_compendium_incremental.report.json")));
        QVERIFY(reportFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QJsonObject report = QJsonDocument::fromJson(reportFile.readAll()).object();
        QCOMPARE(report.value(QStringLiteral("build_mode")).toString(), QStringLiteral("incremental_ingest"));
    }

    // X1: passing a directory to --export-path must create a file inside it
    void testExportPathDirectoryProducesFileInsideDirectory() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dbPath = dir.filePath("test.db");
        const QString romDir = dir.filePath("roms");
        QVERIFY(QDir().mkpath(romDir));
        {
            QFile f(romDir + "/TestGame (USA).nes");
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("NESDATA");
        }

        runCli({ "--db", dbPath, "--scan", romDir });

        // Insert a game and a match directly so the exporter has something to write.
        {
            Remus::Database db;
            QVERIFY(db.initialize(dbPath));
            const QList<Remus::FileRecord> files = db.getExistingFiles();
            QVERIFY(!files.isEmpty());
            const int sysId = db.getSystemId("NES");
            const int gameId = db.insertGame("TestGame", sysId, "USA");
            QVERIFY(gameId > 0);
            QVERIFY(db.insertMatch(files.first().id, gameId, 100.0f, "test"));
        }

        QTemporaryDir exportDir;
        QVERIFY(exportDir.isValid());
        runCli({ "--db", dbPath, "--export", "csv", "--export-path", exportDir.path() });

        const QStringList created = QDir(exportDir.path()).entryList(QDir::Files);
        QVERIFY2(!created.isEmpty(), "Expected a file to be created inside the export directory");
    }

    void testEsExportIncludesPublisherAndReleasedate() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dbPath = dir.filePath("test.db");
        const QString romDir = dir.filePath("roms");
        QVERIFY(QDir().mkpath(romDir));
        {
            QFile f(romDir + "/TestGame (USA).nes");
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("NESDATA");
        }

        runCli({ "--db", dbPath, "--scan", romDir });

        {
            Remus::Database db;
            QVERIFY(db.initialize(dbPath));
            const QList<Remus::FileRecord> files = db.getExistingFiles();
            QVERIFY(!files.isEmpty());
            const int sysId = db.getSystemId("NES");
            const int gameId = db.insertGame("TestGame", sysId, "USA", "Acme Corp", QString(), "2005-04-26");
            QVERIFY(gameId > 0);
            QVERIFY(db.insertMatch(files.first().id, gameId, 100.0f, "test"));
        }

        const QString outFile = dir.filePath("gamelist.xml");
        runCli({ "--db", dbPath, "--export", "emustation", "--export-path", outFile });

        QFile f(outFile);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString xml = QString::fromUtf8(f.readAll());
        QVERIFY2(xml.contains("<publisher>Acme Corp</publisher>"), "ES export should include <publisher>");
        QVERIFY2(xml.contains("<releasedate>20050426T000000</releasedate>"),
            "ES export should include <releasedate> in YYYYMMDDTXXXXXX format");
    }

    void testIngestSource_extractionFailure_noOrphanRows() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        // 1. Build a minimal compendium to obtain an initialised schema.
        const QString sourcePath = fixturePath("test_compendium_source.dat");
        QVERIFY2(!sourcePath.isEmpty(), "Fixture test_compendium_source.dat not found");

        const QString manifestPath = dir.filePath("manifest.json");
        {
            QFile manifestFile(manifestPath);
            QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));
            QJsonObject sourceObject;
            sourceObject.insert("source_id", "seed-source");
            sourceObject.insert("display_name", "Seed Source");
            sourceObject.insert("source_type", "dat");
            sourceObject.insert("snapshot_id", "snapshot-001");
            sourceObject.insert("snapshot_label", "Snapshot 001");
            sourceObject.insert("snapshot_ref", "seed-ref");
            sourceObject.insert("path", sourcePath);
            sourceObject.insert("checksum_sha256", fixtureDatChecksum());
            sourceObject.insert("enabled", true);
            sourceObject.insert("priority", 10);
            QJsonObject manifestObject;
            manifestObject.insert("build_id", "seed-build");
            manifestObject.insert("schema_version", 1);
            manifestObject.insert("sources", QJsonArray { sourceObject });
            const QByteArray json = QJsonDocument(manifestObject).toJson(QJsonDocument::Indented);
            QVERIFY(manifestFile.write(json) == json.size());
        }

        const QString outputDbPath = dir.filePath("remus_compendium_test.db");
        runCli({ "--build-compendium", "--compendium-manifest", manifestPath, "--compendium-output", outputDbPath });
        QVERIFY2(QFile::exists(outputDbPath), "Seed --build-compendium step failed");

        // 2. Write a zero-record DAT so DatExtractor produces no records.
        const QString emptyDatPath = dir.filePath("empty.dat");
        {
            QFile datFile(emptyDatPath);
            QVERIFY(datFile.open(QIODevice::WriteOnly | QIODevice::Text));
            const QByteArray content = QByteArrayLiteral("<?xml version=\"1.0\"?><datafile></datafile>");
            QVERIFY(datFile.write(content) == content.size());
        }

        // 3. Attempt ingest — must fail (no records extracted).
        const QString sourceId = QStringLiteral("test-ingest-rollback");
        runCli({ "--ingest-source", emptyDatPath, "--compendium-output", outputDbPath, "--source-id", sourceId }, 1);

        // 4. Verify the failed ingest left no orphan rows.
        const QString connName = uniqueConnectionName(QStringLiteral("ingest_rollback_"));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
            db.setDatabaseName(outputDbPath);
            QVERIFY2(db.open(), qPrintable(db.lastError().text()));

            QSqlQuery q(db);
            q.prepare(QStringLiteral("SELECT COUNT(*) FROM sources WHERE source_id = ?"));
            q.addBindValue(sourceId);
            QVERIFY2(q.exec(), qPrintable(q.lastError().text()));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 0);

            q.prepare(QStringLiteral("SELECT COUNT(*) FROM source_snapshots WHERE source_id = ?"));
            q.addBindValue(sourceId);
            QVERIFY2(q.exec(), qPrintable(q.lastError().text()));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 0);

            db.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }

    // ── --enrich-source tests ─────────────────────────────────────────────────

    void testEnrichSourceFlagAppearsInHelp() {
        // --help must list --enrich-source so users can discover the option.
        QString output;
        runCliCapture({ "--help" }, output, 0);
        QVERIFY2(output.contains(QStringLiteral("enrich-source")),
            qPrintable(QStringLiteral("--help output did not mention --enrich-source:\n%1").arg(output)));
    }

    void testEnrichSourceHelpMentionsAllSourceKeys() {
        // --help should document every known source key so users know what to pass.
        QString output;
        runCliCapture({ "--help" }, output, 0);
        const QStringList keys = knownEnrichmentSourceKeys();
        for (const QString &key : keys) {
            QVERIFY2(output.contains(key),
                qPrintable(QStringLiteral("--help output is missing source key '%1':\n%2").arg(key, output)));
        }
    }

    void testEnrichSourceUnknownKeyEmitsWarning() {
        // --enrich-compendium on a nonexistent DB always returns exit 1, but
        // the warning about the unknown key must appear before the DB check.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString missingDb = dir.filePath("nonexistent.db");

        QString output;
        runCliCapture({ "--enrich-compendium", "--compendium-output", missingDb, "--enrich-source",
                          "totally-unknown-source-key" },
            output, 1);

        QVERIFY2(output.contains(QStringLiteral("totally-unknown-source-key")),
            qPrintable(QStringLiteral("Expected a warning mentioning the unknown key, got:\n%1").arg(output)));
    }

    void testMatchOnEmptyLibrary() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        m_skipBundledCompendium = true;
        const QString dbPath = dir.filePath(QStringLiteral("match-empty.db"));
        QString output;
        runCliCapture(
            { QStringLiteral("--db"), dbPath, QStringLiteral("--scan"), dir.path(), QStringLiteral("--match") },
            output);
        QVERIFY2(output.contains(QStringLiteral("Matching")), qPrintable(output));
        QVERIFY2(output.contains(QStringLiteral("0 files")) || output.contains(QStringLiteral("Matching 0")),
            qPrintable(output));
    }

    void testBundleDryRunAll() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dbPath = dir.filePath(QStringLiteral("bundle.db"));
        runCli({ QStringLiteral("--db"), dbPath, QStringLiteral("--scan"), dir.path(), QStringLiteral("--bundle"),
            dir.filePath(QStringLiteral("bundles")), QStringLiteral("--dry-run-all") });
    }

    void testCoverageReportRequiresCompendiumOutput() {
        runCli({ QStringLiteral("--coverage-report"), QStringLiteral("--compendium-output"),
                   QStringLiteral("/nonexistent/remus_compendium.db") },
            1);
    }

    void testCoverageReportEmitsSummaryRow() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString sourcePath = fixturePath(QStringLiteral("test_compendium_source.dat"));
        QVERIFY2(!sourcePath.isEmpty(), "Fixture test_compendium_source.dat not found");

        const QString manifestPath = dir.filePath(QStringLiteral("manifest.json"));
        {
            QFile manifestFile(manifestPath);
            QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));

            QJsonObject sourceObject;
            sourceObject.insert(QStringLiteral("source_id"), QStringLiteral("test-source"));
            sourceObject.insert(QStringLiteral("display_name"), QStringLiteral("Test Source"));
            sourceObject.insert(QStringLiteral("source_type"), QStringLiteral("dat"));
            sourceObject.insert(QStringLiteral("snapshot_id"), QStringLiteral("snapshot-001"));
            sourceObject.insert(QStringLiteral("snapshot_label"), QStringLiteral("Snapshot 001"));
            sourceObject.insert(QStringLiteral("snapshot_ref"), QStringLiteral("test-ref"));
            sourceObject.insert(QStringLiteral("path"), sourcePath);
            sourceObject.insert(QStringLiteral("checksum_sha256"), fixtureDatChecksum());
            sourceObject.insert(QStringLiteral("enabled"), true);
            sourceObject.insert(QStringLiteral("priority"), 10);

            QJsonObject manifestObject;
            manifestObject.insert(QStringLiteral("build_id"), QStringLiteral("coverage-smoke"));
            manifestObject.insert(QStringLiteral("schema_version"), 1);
            manifestObject.insert(QStringLiteral("sources"), QJsonArray { sourceObject });

            const QByteArray manifestJson = QJsonDocument(manifestObject).toJson(QJsonDocument::Indented);
            QVERIFY(manifestFile.write(manifestJson) == manifestJson.size());
        }

        const QString outputDbPath = dir.filePath(QStringLiteral("remus_compendium_test.db"));
        runCli({ QStringLiteral("--build-compendium"), QStringLiteral("--compendium-manifest"), manifestPath,
            QStringLiteral("--compendium-output"), outputDbPath });

        QString output;
        runCliCapture(
            { QStringLiteral("--coverage-report"), QStringLiteral("--compendium-output"), outputDbPath }, output);
        QVERIFY2(output.contains(QStringLiteral("# games=")), qPrintable(output));
        QVERIFY2(output.contains(QStringLiteral("source_id")), qPrintable(output));
    }

    void testDatCoverageExitsZero() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        runCli({ QStringLiteral("--db"), dir.filePath(QStringLiteral("dat-coverage.db")),
            QStringLiteral("--dat-coverage") });
    }

    void testLibraryPipelineDryRun() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString romDir = dir.filePath(QStringLiteral("roms"));
        QVERIFY(QDir().mkpath(romDir));
        runCli({ QStringLiteral("--library"), romDir, QStringLiteral("--output"), dir.filePath(QStringLiteral("out")),
            QStringLiteral("--dry-run-all") });
    }

    void testProcessPresetDryRun() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString romDir = dir.filePath(QStringLiteral("roms"));
        QVERIFY(QDir().mkpath(romDir));
        runCli({ QStringLiteral("--process"), romDir, QStringLiteral("--process-preset"), QStringLiteral("es-de"),
            QStringLiteral("--dry-run-all") });
    }

    void testRetroArchExportCreatesFile() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("test.db"));
        const QString romDir = dir.filePath(QStringLiteral("roms"));
        QVERIFY(QDir().mkpath(romDir));
        {
            QFile f(romDir + QStringLiteral("/TestGame (USA).nes"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("NESDATA");
        }
        runCli({ QStringLiteral("--db"), dbPath, QStringLiteral("--scan"), romDir });
        {
            Remus::Database db;
            QVERIFY(db.initialize(dbPath));
            const QList<Remus::FileRecord> files = db.getExistingFiles();
            QVERIFY(!files.isEmpty());
            const int sysId = db.getSystemId(QStringLiteral("NES"));
            const int gameId = db.insertGame(QStringLiteral("TestGame"), sysId, QStringLiteral("USA"));
            QVERIFY(db.insertMatch(files.first().id, gameId, 100.0f, QStringLiteral("test")));
        }
        const QString outFile = dir.filePath(QStringLiteral("remus.lpl"));
        runCli({ QStringLiteral("--db"), dbPath, QStringLiteral("--export"), QStringLiteral("retroarch"),
            QStringLiteral("--export-path"), outFile });
        QVERIFY(QFileInfo(outFile).exists());
    }

    void testLaunchBoxAndJsonExport() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("test.db"));
        const QString romDir = dir.filePath(QStringLiteral("roms"));
        QVERIFY(QDir().mkpath(romDir));
        {
            QFile f(romDir + QStringLiteral("/TestGame (USA).nes"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("NESDATA");
        }
        runCli({ QStringLiteral("--db"), dbPath, QStringLiteral("--scan"), romDir });
        {
            Remus::Database db;
            QVERIFY(db.initialize(dbPath));
            const QList<Remus::FileRecord> files = db.getExistingFiles();
            const int sysId = db.getSystemId(QStringLiteral("NES"));
            const int gameId = db.insertGame(QStringLiteral("TestGame"), sysId, QStringLiteral("USA"));
            QVERIFY(db.insertMatch(files.first().id, gameId, 100.0f, QStringLiteral("test")));
        }
        const QString launchbox = dir.filePath(QStringLiteral("launchbox.xml"));
        const QString json = dir.filePath(QStringLiteral("export.json"));
        runCli({ QStringLiteral("--db"), dbPath, QStringLiteral("--export"), QStringLiteral("launchbox"),
            QStringLiteral("--export-path"), launchbox });
        runCli({ QStringLiteral("--db"), dbPath, QStringLiteral("--export"), QStringLiteral("json"),
            QStringLiteral("--export-path"), json });
        QVERIFY(QFileInfo(launchbox).exists());
        QVERIFY(QFileInfo(json).exists());
    }

    void testModifierOnlyShowsHelp() {
        QProcess proc;
        proc.setProcessEnvironment(cliEnvironment());
        proc.start(cliPath(), { QStringLiteral("--force-full-rebuild") });
        QVERIFY(proc.waitForStarted());
        QVERIFY(proc.waitForFinished(30000));
        QCOMPARE(proc.exitCode(), 0);
        const QString output = QString::fromUtf8(proc.readAllStandardOutput() + proc.readAllStandardError());
        QVERIFY2(output.contains(QStringLiteral("Usage:")) || output.contains(QStringLiteral("Options:")),
            qPrintable(output));
    }

    void testMultiActionConflictExitsUsageError() {
        runCli({ QStringLiteral("--scan"), QStringLiteral("/tmp"), QStringLiteral("--verify"),
                   QStringLiteral("missing.dat") },
            2);
    }

    void testEditMetadataWithoutFieldsFails() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("test.db"));
        runCli({ QStringLiteral("--db"), dbPath, QStringLiteral("--edit-metadata"), QStringLiteral("1") }, 1);
    }

    void testVerifyMismatchExitsFailure() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("test.db"));
        const QString romPath = dir.filePath(QStringLiteral("Test Game.nds"));
        {
            QFile romFile(romPath);
            QVERIFY(romFile.open(QIODevice::WriteOnly));
            QVERIFY(romFile.write("ABCD") == 4);
        }
        const QString datPath = dir.filePath(QStringLiteral("bad.dat"));
        {
            QFile datFile(datPath);
            QVERIFY(datFile.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&datFile);
            out << "clrmamepro (\n";
            out << "  name \"Nintendo - Nintendo DS\"\n";
            out << "  description \"Mismatch test\"\n";
            out << "  version \"1.0\"\n";
            out << ")\n";
            out << "game (\n";
            out << "  name \"Test Game\"\n";
            out << "  rom ( name \"Test Game.nds\" size 4 crc 00000000 )\n";
            out << ")\n";
        }
        runCli({ QStringLiteral("--db"), dbPath, QStringLiteral("--scan"), dir.path(), QStringLiteral("--hash") });
        runCli({ QStringLiteral("--db"), dbPath, QStringLiteral("--verify"), datPath }, 1);
    }
};

QTEST_MAIN(CliSmokeTest)
#include "test_cli_smoke.moc"
