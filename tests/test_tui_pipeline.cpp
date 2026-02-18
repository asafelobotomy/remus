/**
 * @file test_tui_pipeline.cpp
 * @brief Unit tests for TuiPipeline (stage transitions + DB persistence)
 *
 * Uses a temp directory with stub ROM files and a file-backed database
 * (the pipeline creates its own thread-local DB connection from the path).
 */

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <mutex>
#include <vector>

#include "../src/tui/pipeline.h"
#include "../src/core/database.h"

using namespace Remus;

class TestTuiPipeline : public QObject
{
    Q_OBJECT

private:
    /// Create stub ROM files in a temp directory.
    void createStubRoms(const QString &dir)
    {
        // NES stub with valid header
        QByteArray nesHeader("NES\x1A");
        nesHeader.append(QByteArray(12, '\x00'));
        nesHeader.append(QByteArray(32, '\xAA'));

        QFile nes(dir + "/TestGame.nes");
        QVERIFY(nes.open(QIODevice::WriteOnly));
        nes.write(nesHeader);
        nes.close();

        // A second ROM to verify bulk processing
        QFile nes2(dir + "/AnotherGame.nes");
        QVERIFY(nes2.open(QIODevice::WriteOnly));
        nes2.write(nesHeader);
        nes2.close();
    }

private slots:

    void testStageTransitions()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createStubRoms(tmp.path());

        QString dbPath = tmp.path() + "/pipeline_test.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        // Track stages observed
        std::mutex mu;
        std::vector<PipelineProgress::Stage> stages;

        TuiPipeline pipeline;
        bool started = pipeline.start(
            tmp.path().toStdString(),
            [&](const PipelineProgress &p) {
                std::lock_guard<std::mutex> lock(mu);
                if (stages.empty() || stages.back() != p.stage)
                    stages.push_back(p.stage);
            },
            [](const std::string &msg) {
                // Optional: uncomment for debugging
                // qDebug() << QString::fromStdString(msg);
                (void)msg;
            },
            &db
        );
        QVERIFY(started);

        // Wait for pipeline to finish (with timeout)
        int waited = 0;
        while (pipeline.running() && waited < 30000) {
            QThread::msleep(100);
            waited += 100;
        }
        QVERIFY2(!pipeline.running(), "Pipeline did not finish within timeout");

        // Verify stage transitions occurred
        std::lock_guard<std::mutex> lock(mu);
        QVERIFY2(!stages.empty(), "No stage transitions observed");

        // At minimum we should see Scanning
        bool sawScanning = false;
        bool sawHashing = false;
        bool sawIdle = false;
        for (auto s : stages) {
            if (s == PipelineProgress::Scanning) sawScanning = true;
            if (s == PipelineProgress::Hashing) sawHashing = true;
            if (s == PipelineProgress::Idle) sawIdle = true;
        }
        QVERIFY2(sawScanning, "Scanning stage not observed");
        QVERIFY2(sawHashing, "Hashing stage not observed");
        QVERIFY2(sawIdle, "Pipeline did not return to Idle");
    }

    void testDbPersistence()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createStubRoms(tmp.path());

        QString dbPath = tmp.path() + "/pipeline_persist.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        TuiPipeline pipeline;
        bool started = pipeline.start(
            tmp.path().toStdString(),
            nullptr,  // no progress callback
            nullptr,  // no log callback
            &db
        );
        QVERIFY(started);

        int waited = 0;
        while (pipeline.running() && waited < 30000) {
            QThread::msleep(100);
            waited += 100;
        }
        QVERIFY(!pipeline.running());

        // Re-open DB and verify files were persisted
        // (pipeline uses thread-local connection so re-read from main)
        Database verifyDb;
        QVERIFY(verifyDb.initialize(dbPath));

        auto files = verifyDb.getAllFiles();
        QVERIFY2(files.size() >= 2,
                 qPrintable(QString("Expected ≥2 files, got %1").arg(files.size())));

        // At least one file should have a hash computed
        bool anyHashed = false;
        for (const auto &f : files) {
            if (f.hashCalculated) {
                anyHashed = true;
                break;
            }
        }
        QVERIFY2(anyHashed, "No files received hashes");
    }

    void testDoubleStartRejected()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        createStubRoms(tmp.path());

        QString dbPath = tmp.path() + "/pipeline_double.db";
        Database db;
        QVERIFY(db.initialize(dbPath));

        TuiPipeline pipeline;
        bool first = pipeline.start(tmp.path().toStdString(), nullptr, nullptr, &db);
        QVERIFY(first);

        // Second start while running should be rejected
        bool second = pipeline.start(tmp.path().toStdString(), nullptr, nullptr, &db);
        QVERIFY2(!second, "Double start should return false");

        // Wait for completion
        int waited = 0;
        while (pipeline.running() && waited < 30000) {
            QThread::msleep(100);
            waited += 100;
        }
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    TestTuiPipeline t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_tui_pipeline.moc"
