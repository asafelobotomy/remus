#pragma once

#include <notcurses/notcurses.h>
#include <memory>
#include <string>
#include <functional>
#include <mutex>
#include <vector>

#include "screen.h"
#include "widgets/toast.h"
#include "widgets/help_overlay.h"

namespace Remus { class Database; }

/**
 * @brief Main TUI application.
 *
 * Owns the notcurses context, the screen stack, and the shared
 * Database instance.  Screens push/pop themselves via the public API.
 */
class TuiApp {
public:
    TuiApp();
    ~TuiApp();

    /// Run the main event loop.  Returns exit code.
    int run();

    // ── Screen navigation ──────────────────────────────────────
    /// Push a new screen (becomes active, old screen preserved).
    void pushScreen(std::unique_ptr<Screen> screen);

    /// Pop current screen (previous screen becomes active).
    void popScreen();

    /// Replace entire stack with a single screen.
    void setScreen(std::unique_ptr<Screen> screen);

    // ── Thread-safe state queue ────────────────────────────────
    /// Enqueue a callback to run on the main-thread before next render.
    void post(std::function<void()> fn);

    // ── Toast notifications ────────────────────────────────────
    /// Show a toast notification.
    void toast(const std::string &message,
               Toast::Level level = Toast::Level::Info,
               int durationMs = 3000)
    {
        m_toast.show(message, level, durationMs);
    }

    // ── Accessors ──────────────────────────────────────────────
    struct notcurses *nc() const { return m_nc; }
    Remus::Database  &db() const { return *m_db; }
    std::string       version() const { return m_version; }

    /// Terminal dimensions (updated on resize)
    unsigned rows() const { return m_rows; }
    unsigned cols() const { return m_cols; }

private:
    void drainPosted();
    void updateDimensions();

    struct notcurses *m_nc = nullptr;
    std::unique_ptr<Remus::Database> m_db;
    std::string m_version;

    // Screen stack — top of stack is the active screen
    std::vector<std::unique_ptr<Screen>> m_screens;

    // Thread-safe callback queue
    std::mutex m_postMutex;
    std::vector<std::function<void()>> m_posted;

    unsigned m_rows = 0;
    unsigned m_cols = 0;

    // Global overlay widgets
    Toast m_toast;
    HelpOverlay m_helpOverlay;
};
