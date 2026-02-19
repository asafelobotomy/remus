#include "app.h"
#include "launch_screen.h"

#include "../core/database.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <chrono>

using Clock = std::chrono::steady_clock;

// ────────────────────────────────────────────────────────────
// Construction / Destruction
// ────────────────────────────────────────────────────────────
TuiApp::TuiApp()
    : m_db(std::make_unique<Remus::Database>())
{
    // Read version from VERSION file (project root)
    QFile vf(QStringLiteral("VERSION"));
    if (!vf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Try relative to executable
        vf.setFileName(QCoreApplication::applicationDirPath() + QStringLiteral("/../../VERSION"));
        (void)vf.open(QIODevice::ReadOnly | QIODevice::Text);
    }
    if (vf.isOpen()) {
        QTextStream in(&vf);
        m_version = in.readLine().trimmed().toStdString();
        vf.close();
    }
    if (m_version.empty())
        m_version = "0.10.1";
}

TuiApp::~TuiApp()
{
    // Screens must be destroyed before notcurses is stopped
    m_screens.clear();
}

// ────────────────────────────────────────────────────────────
// Main loop
// ────────────────────────────────────────────────────────────
int TuiApp::run()
{
    // ── Initialise database ────────────────────────────────
    QString dbDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dbDir);
    QString dbPath = dbDir + QStringLiteral("/remus.db");
    if (!m_db->initialize(dbPath)) {
        fprintf(stderr, "Failed to initialise database at %s\n", qPrintable(dbPath));
        return 1;
    }

    // ── Initialise notcurses ───────────────────────────────
    notcurses_options opts{};
    opts.flags = NCOPTION_SUPPRESS_BANNERS;

    m_nc = notcurses_init(&opts, nullptr);
    if (!m_nc)
        return 1;

    notcurses_mice_enable(m_nc, NCMICE_ALL_EVENTS);
    updateDimensions();

    // ── Push the launch screen ─────────────────────────────
    pushScreen(std::make_unique<LaunchScreen>(*this));

    // ── Event loop ─────────────────────────────────────────
    ncinput ni{};
    Clock::time_point nextTick = Clock::now();
    const auto tickInterval = std::chrono::milliseconds(200);
    bool quit = false;

    while (!quit && !m_screens.empty()) {
        // Non-blocking input (50 ms timeout)
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 50 * 1000 * 1000;
        int ch = notcurses_get(m_nc, &ts, &ni);

        if (ch == NCKEY_RESIZE) {
            updateDimensions();
            if (!m_screens.empty())
                m_screens.back()->onResize(m_nc);
            m_screenChanged = true; // force full repaint after resize
        }

        // Drain any posted callbacks (from background threads).
        // If any callbacks ran they may have mutated screen state — flag a
        // redraw so the new state is visible within one 50 ms poll cycle
        // rather than waiting up to 200 ms for the next periodic tick.
        bool postedRan = drainPosted();

        // Dispatch input to active screen
        if (ch > 0 && ch != NCKEY_RESIZE && !m_screens.empty()) {
            // Help overlay consumes all input when visible
            if (m_helpOverlay.visible()) {
                m_helpOverlay.handleInput(ch);
            }
            // '?' toggles help overlay (unless screen consumed it)
            else if (ch == '?') {
                auto bindings = m_screens.back()->keybindings();
                // Add global keybindings
                bindings.push_back({"?", "Toggle help overlay"});
                bindings.push_back({"Esc", "Back / cancel"});
                bindings.push_back({"q", "Quit (from top screen)"});
                m_helpOverlay.show(m_screens.back()->name(), bindings);
            }
            else {
                bool handled = m_screens.back()->handleInput(m_nc, ni, ch);
                if (!handled && (ch == 'q' || ch == NCKEY_ESC)) {
                    // Global quit if screen didn't consume it
                    if (m_screens.size() <= 1)
                        quit = true;
                    else
                        popScreen();
                }
            }
        }

        // Periodic tick
        auto now = Clock::now();
        bool needRedraw = false;
        if (now >= nextTick) {
            if (!m_screens.empty())
                needRedraw = m_screens.back()->tick();
            // Tick toast — auto-dismiss expired toasts
            if (m_toast.tick())
                needRedraw = true;
            nextTick = now + tickInterval;
        }

        // ── Render ────────────────────────────────────────────
        // Consolidate ALL rendering here — navigation methods (pushScreen,
        // popScreen, setScreen) no longer render inline; they set m_screenChanged
        // instead.  This eliminates the double-render-per-transition that was
        // confusing notcurses's damage tracker and producing garbled frames.
        if (ch > 0 || needRedraw || m_screenChanged || postedRan) {
            ncplane *stdp = notcurses_stdplane(m_nc);

            // Reset pen state completely before each render so that no style
            // or colour bleeds in from the previous screen's last draw call.
            ncplane_set_styles(stdp, NCSTYLE_NONE);
            ncplane_set_channels(stdp, 0);

            ncplane_erase(stdp);

            if (!m_screens.empty())
                m_screens.back()->render(m_nc);

            // Render overlays on top
            m_toast.render(stdp, m_rows - 2, m_cols);
            m_helpOverlay.render(stdp, m_rows, m_cols);

            if (m_screenChanged) {
                // After a screen transition the internal notcurses damage table
                // may hold stale state from the old screen.  notcurses_render
                // only sends a cell diff, so call it first to update the damage
                // table, then notcurses_refresh to force a full terminal repaint.
                notcurses_render(m_nc);
                notcurses_refresh(m_nc, nullptr, nullptr);
                m_screenChanged = false;
            } else {
                notcurses_render(m_nc);
            }
        }
    }

    // ── Cleanup ────────────────────────────────────────────
    m_screens.clear();
    notcurses_stop(m_nc);
    m_nc = nullptr;
    return 0;
}

// ────────────────────────────────────────────────────────────
// Screen navigation
// ────────────────────────────────────────────────────────────
void TuiApp::pushScreen(std::unique_ptr<Screen> screen)
{
    if (!m_screens.empty())
        m_screens.back()->onLeave();
    m_screens.push_back(std::move(screen));
    m_screens.back()->onEnter();

    // Signal the main loop to do a clean render + full refresh on its next
    // iteration.  We do NOT render inline here: doing so would produce a
    // double render (this render + the loop's render) which confuses
    // notcurses's damage tracker and causes garbled transitions.
    m_screenChanged = true;
}

void TuiApp::popScreen()
{
    if (m_screens.empty())
        return;
    m_screens.back()->onLeave();
    m_screens.pop_back();
    if (!m_screens.empty()) {
        m_screens.back()->onEnter();
        // Signal the main loop to render + full-refresh rather than rendering
        // inline here (prevents the double-render / garbling issue).
        m_screenChanged = true;
    }
}

void TuiApp::setScreen(std::unique_ptr<Screen> screen)
{
    for (auto &s : m_screens)
        s->onLeave();
    m_screens.clear();
    pushScreen(std::move(screen));
}

// ────────────────────────────────────────────────────────────
// Thread-safe callback queue
// ────────────────────────────────────────────────────────────
void TuiApp::post(std::function<void()> fn)
{
    std::lock_guard<std::mutex> lock(m_postMutex);
    m_posted.push_back(std::move(fn));
}

bool TuiApp::drainPosted()
{
    std::vector<std::function<void()>> local;
    {
        std::lock_guard<std::mutex> lock(m_postMutex);
        local.swap(m_posted);
    }
    for (auto &fn : local)
        fn();
    return !local.empty();
}

// ────────────────────────────────────────────────────────────
// Helpers
// ────────────────────────────────────────────────────────────
void TuiApp::updateDimensions()
{
    ncplane *std = notcurses_stddim_yx(m_nc, &m_rows, &m_cols);
    (void)std;
}
