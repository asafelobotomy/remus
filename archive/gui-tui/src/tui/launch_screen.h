#pragma once

#include "screen.h"
#include <chrono>

/**
 * @brief Splash screen shown at startup.
 *
 * Displays the REMUS title, version, and a brief loading bar
 * while the database initialises (already done by now, so 
 * it auto-transitions after ~1.5 s).
 */
class LaunchScreen : public Screen {
public:
    explicit LaunchScreen(TuiApp &app) : Screen(app) {}

    void    onEnter() override;
    bool    handleInput(struct notcurses *nc, const ncinput &ni, int ch) override;
    void    render(struct notcurses *nc) override;
    bool    tick() override;
    std::string name() const override { return "Launch"; }

private:
    std::chrono::steady_clock::time_point m_start;
    int  m_progress = 0;   // 0..100
    bool m_done = false;
};
