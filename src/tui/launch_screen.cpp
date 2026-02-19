#include "launch_screen.h"
#include "app.h"
#include "main_menu_screen.h"

#include <cstring>

using Clock = std::chrono::steady_clock;

// ────────────────────────────────────────────────────────────
void LaunchScreen::onEnter()
{
    m_start = Clock::now();
    m_progress = 0;
    m_done = false;
}

// ────────────────────────────────────────────────────────────
bool LaunchScreen::handleInput(struct notcurses *, const ncinput &, int ch)
{
    // Any key skips splash
    if (ch > 0) {
        m_done = true;
        m_app.setScreen(std::make_unique<MainMenuScreen>(m_app));
        return true;
    }
    return false;
}

// ────────────────────────────────────────────────────────────
bool LaunchScreen::tick()
{
    if (m_done)
        return false;

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       Clock::now() - m_start).count();

    // Animate the progress bar from 0→100 over 1.5 seconds
    m_progress = static_cast<int>(elapsed * 100 / 1500);
    if (m_progress > 100) m_progress = 100;

    if (elapsed >= 1500 && !m_done) {
        m_done = true;
        // NOTE: setScreen() destroys this LaunchScreen (via m_screens.clear()).
        // Return immediately after — do NOT touch any member after this call.
        m_app.setScreen(std::make_unique<MainMenuScreen>(m_app));
        return false;
    }
    return true; // always redraw during splash
}

// ────────────────────────────────────────────────────────────
void LaunchScreen::render(struct notcurses *nc)
{
    unsigned rows = m_app.rows();
    unsigned cols = m_app.cols();
    ncplane *std = notcurses_stdplane(nc);

    // ── Centered title block ───────────────────────────────
    const char *titleLines[] = {
        "REMUS",
        "Retro ROM Manager",
    };
    std::string versionLine = "v" + m_app.version();

    int blockH = 7; // title + subtitle + version + blank + bar + blank + hint
    int startY = static_cast<int>(rows) / 2 - blockH / 2;
    if (startY < 1) startY = 1;
    int y = startY;

    // Title — bold, red
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0x00, 0x00);
        ncplane_set_channels(std, ch);
        ncplane_set_styles(std, NCSTYLE_BOLD);
        int x = (static_cast<int>(cols) - static_cast<int>(strlen(titleLines[0]))) / 2;
        ncplane_putstr_yx(std, y++, x, titleLines[0]);
        ncplane_set_styles(std, NCSTYLE_NONE);
    }

    // Subtitle — white
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
        ncplane_set_channels(std, ch);
        int x = (static_cast<int>(cols) - static_cast<int>(strlen(titleLines[1]))) / 2;
        ncplane_putstr_yx(std, y++, x, titleLines[1]);
    }

    // Version — dim
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
        ncplane_set_channels(std, ch);
        int x = (static_cast<int>(cols) - static_cast<int>(versionLine.size())) / 2;
        ncplane_putstr_yx(std, y++, x, versionLine.c_str());
    }

    y++; // blank line

    // ── Progress bar ───────────────────────────────────────
    {
        const int barW = 30;
        int filled = m_progress * barW / 100;
        if (filled < 0) filled = 0;
        if (filled > barW) filled = barW;

        std::string bar;
        bar.reserve(static_cast<size_t>(barW + 2));
        bar.push_back('[');

        // Filled portion in red
        std::string filledPart(static_cast<size_t>(filled), '#');
        std::string emptyPart(static_cast<size_t>(barW - filled), ' ');

        int x = (static_cast<int>(cols) - (barW + 2)) / 2;

        ncplane_putstr_yx(std, y, x, "[");
        {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0xCC, 0x00, 0x00);
            ncplane_set_channels(std, ch);
            ncplane_putstr(std, filledPart.c_str());
        }
        {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0x44, 0x44, 0x44);
            ncplane_set_channels(std, ch);
            ncplane_putstr(std, emptyPart.c_str());
        }
        {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
            ncplane_set_channels(std, ch);
            ncplane_putstr(std, "]");
        }
        y++;
    }

    // "LOADING..." label
    {
        const char *label = m_progress >= 100 ? "READY" : "LOADING...";
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0x00, 0x00);
        ncplane_set_channels(std, ch);
        ncplane_set_styles(std, NCSTYLE_BOLD);
        int x = (static_cast<int>(cols) - static_cast<int>(strlen(label))) / 2;
        ncplane_putstr_yx(std, y++, x, label);
        ncplane_set_styles(std, NCSTYLE_NONE);
    }

    // Reset channels
    ncplane_set_channels(std, 0);
}
