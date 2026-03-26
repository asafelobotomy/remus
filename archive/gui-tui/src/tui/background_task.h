#pragma once

#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

class TuiApp;

/**
 * @brief Reusable background task wrapper.
 *
 * Encapsulates the std::thread + std::atomic<bool> + std::mutex pattern
 * used by Pipeline, CompressorScreen, and PatchScreen.
 *
 * Usage:
 *   BackgroundTask task;
 *   task.start([&]() { ... long work ... });
 *   if (task.running()) { ... }
 *   task.cancel();  // sets cancelled flag, does NOT force-stop
 *
 * The destructor auto-joins if a thread is still running.
 * The worker should periodically check cancelled() to exit early.
 */
class BackgroundTask {
public:
    BackgroundTask() = default;
    ~BackgroundTask() { join(); }

    // Non-copyable, non-movable (owns a thread)
    BackgroundTask(const BackgroundTask &) = delete;
    BackgroundTask &operator=(const BackgroundTask &) = delete;

    /// Launch a new background task. Returns false if already running.
    bool start(std::function<void()> work)
    {
        if (m_running.load())
            return false;

        // Join any previous thread before starting a new one
        join();

        m_cancelled = false;
        m_running = true;
        m_thread = std::thread([this, fn = std::move(work)]() {
            fn();
            m_running = false;
        });
        return true;
    }

    /// Signal the worker to stop. Does NOT block.
    void cancel()
    {
        m_cancelled = true;
    }

    /// Block until the worker thread finishes.
    void join()
    {
        if (m_thread.joinable())
            m_thread.join();
    }

    /// Cancel and join.
    void stop()
    {
        cancel();
        join();
    }

    /// True while the worker function is executing.
    bool running() const { return m_running.load(); }

    /// True if cancel() has been called. Workers should check this.
    bool cancelled() const { return m_cancelled.load(); }

    /// Access the raw cancelled flag (for passing to services that accept std::atomic<bool>*)
    const std::atomic<bool> &cancelledFlag() const { return m_cancelled; }

    // ── Progress helpers ───────────────────────────────────
    // Thread-safe progress state that workers can update
    // and renderers can read.

    struct Progress {
        int         done = 0;
        int         total = 0;
        std::string label;
        std::string currentItem;
    };

    void setProgress(int done, int total, const std::string &label = {},
                     const std::string &currentItem = {})
    {
        std::lock_guard<std::mutex> lock(m_progressMutex);
        m_progress.done = done;
        m_progress.total = total;
        if (!label.empty()) m_progress.label = label;
        if (!currentItem.empty()) m_progress.currentItem = currentItem;
    }

    Progress progress() const
    {
        std::lock_guard<std::mutex> lock(m_progressMutex);
        return m_progress;
    }

    void resetProgress()
    {
        std::lock_guard<std::mutex> lock(m_progressMutex);
        m_progress = {};
    }

private:
    std::thread        m_thread;
    std::atomic<bool>  m_running{false};
    std::atomic<bool>  m_cancelled{false};
    mutable std::mutex m_progressMutex;
    Progress           m_progress;
};
