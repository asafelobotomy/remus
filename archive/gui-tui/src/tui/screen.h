#pragma once

#include <notcurses/notcurses.h>
#include <string>
#include <vector>
#include <utility>

// Forward declare App so screens can access it
class TuiApp;

/**
 * @brief Base class for all TUI screens.
 *
 * Each screen owns its rendering and input handling. The App
 * drives the event loop and delegates to the active screen.
 */
class Screen {
public:
    explicit Screen(TuiApp &app) : m_app(app) {}
    virtual ~Screen() = default;

    /// Called once when the screen becomes active
    virtual void onEnter() {}

    /// Called once when the screen is replaced by another
    virtual void onLeave() {}

    /// Process a single input event. Return true if handled.
    virtual bool handleInput(struct notcurses *nc, const ncinput &ni, int ch) = 0;

    /// Render the screen onto the standard plane hierarchy.
    virtual void render(struct notcurses *nc) = 0;

    /// Called on terminal resize — recreate any child planes.
    virtual void onResize(struct notcurses *nc) { (void)nc; }

    /// Periodic tick (200ms default). Return true if redraw needed.
    virtual bool tick() { return false; }

    /// Human-readable screen name (for debug / title bar)
    virtual std::string name() const = 0;

    /// Return keybinding pairs {key, description} for help overlay.
    virtual std::vector<std::pair<std::string, std::string>> keybindings() const { return {}; }

    /// Force-reload data from disk/database. Called when data is known to be stale.
    virtual void forceRefresh() {}

protected:
    TuiApp &m_app;
};
