#pragma once

#include <functional>
#include <string>

#include "background_task.h"

namespace Remus { class Database; class LibraryService; class HashService; class MatchService; }

struct PipelineProgress {
    enum Stage { Idle, Scanning, Hashing, Matching } stage = Idle;
    int done = 0;
    int total = 0;
    std::string path;
};

class TuiPipeline {
public:
    using ProgressCallback = std::function<void(const PipelineProgress&)>;
    using LogCallback = std::function<void(const std::string&)>;

    bool start(const std::string& libraryPath,
               ProgressCallback progressCb,
               LogCallback logCb,
               Remus::Database *db = nullptr);
    void stop();
    bool running() const { return m_task.running(); }

    /// Access the background task (e.g. to check cancelled())
    BackgroundTask &task() { return m_task; }

private:
    void run(const std::string& libraryPath,
             ProgressCallback progressCb,
             LogCallback logCb,
             Remus::Database *db);

    BackgroundTask m_task;
};
