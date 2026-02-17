#include "pipeline.h"

#include "../services/library_service.h"
#include "../services/hash_service.h"
#include "../services/match_service.h"
#include "../core/database.h"

#include <QSqlDatabase>

bool TuiPipeline::start(const std::string& libraryPath,
                        ProgressCallback progressCb,
                        LogCallback logCb,
                        Remus::Database *db)
{
    return m_task.start([this, libraryPath, progressCb, logCb, db]() {
        run(libraryPath, progressCb, logCb, db);
    });
}

void TuiPipeline::stop()
{
    m_task.stop();
}

void TuiPipeline::run(const std::string& libraryPath,
                      ProgressCallback progressCb,
                      LogCallback logCb,
                      Remus::Database *db)
{
    auto makeProgress = [&](PipelineProgress::Stage stage, int done, int total, const std::string& path) {
        if (progressCb) {
            PipelineProgress p;
            p.stage = stage;
            p.done = done;
            p.total = total;
            p.path = path;
            progressCb(p);
        }
    };

    // Database: create a thread-local connection if db is provided
    // (Qt requires each thread to have its own DB connection)
    std::unique_ptr<Remus::Database> threadDb;
    if (db) {
        QString dbPath = db->database().databaseName();
        if (!dbPath.isEmpty()) {
            threadDb = std::make_unique<Remus::Database>();
            if (!threadDb->initialize(dbPath)) {
                if (logCb) logCb("Warning: failed to open DB in pipeline thread, results won't be persisted");
                threadDb.reset();
            }
        }
    }

    if (!threadDb) {
        if (logCb) logCb("No database available — pipeline cannot proceed");
        return;
    }

    // ── Scanning ──────────────────────────────────────────
    Remus::LibraryService libService;
    int inserted = libService.scan(
        QString::fromStdString(libraryPath), threadDb.get(),
        [&](int done, int total, const QString &path) {
            makeProgress(PipelineProgress::Scanning, done, total,
                         path.isEmpty() ? std::string{} : path.toStdString());
        },
        [&](const QString &msg) { if (logCb) logCb(msg.toStdString()); }
    );
    (void)inserted;

    if (m_task.cancelled()) return;

    // ── Hashing ───────────────────────────────────────────
    Remus::HashService hashService;
    hashService.hashAll(
        threadDb.get(),
        [&](int done, int total, const QString &path) {
            makeProgress(PipelineProgress::Hashing, done, total,
                         path.isEmpty() ? std::string{} : path.toStdString());
        },
        [&](const QString &msg) { if (logCb) logCb(msg.toStdString()); },
        &m_task.cancelledFlag()
    );

    if (m_task.cancelled()) return;

    // ── Matching ──────────────────────────────────────────
    Remus::MatchService matchService;
    auto stats = matchService.matchAll(
        threadDb.get(),
        [&](int done, int total, const QString &path) {
            makeProgress(PipelineProgress::Matching, done, total,
                         path.isEmpty() ? std::string{} : path.toStdString());
        },
        [&](const QString &msg) { if (logCb) logCb(msg.toStdString()); },
        &m_task.cancelledFlag()
    );

    if (logCb) logCb("Pipeline done");
    if (progressCb) {
        PipelineProgress done;
        done.stage = PipelineProgress::Idle;
        progressCb(done);
    }
}
