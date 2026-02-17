#include "main_menu_screen.h"
#include "app.h"
#include "match_screen.h"
#include "library_screen.h"
#include "compressor_screen.h"
#include "patch_screen.h"
#include "options_screen.h"

#include <cstring>

// ────────────────────────────────────────────────────────────
MainMenuScreen::MainMenuScreen(TuiApp &app)
    : Screen(app)
{
    m_items = {
        { "MATCH",      true  },  // 0 — red/bold
        { "LIBRARY",    false },  // 1
        { "COMPRESSOR", false },  // 2
        { "PATCH",      true  },  // 3 — red/bold
        { "OPTIONS",    false },  // 4
    };
}

// ────────────────────────────────────────────────────────────
void MainMenuScreen::onEnter()
{
    // Could restore last selection from QSettings in the future
}

// ────────────────────────────────────────────────────────────
bool MainMenuScreen::handleInput(struct notcurses *, const ncinput &, int ch)
{
    int count = static_cast<int>(m_items.size());

    if (ch == 'j' || ch == NCKEY_DOWN) {
        m_selected = (m_selected + 1) % count;
        return true;
    }
    if (ch == 'k' || ch == NCKEY_UP) {
        m_selected = (m_selected - 1 + count) % count;
        return true;
    }
    if (ch == NCKEY_ENTER || ch == '\n' || ch == '\r') {
        activateSelected();
        return true;
    }

    // Number keys 1-5
    if (ch >= '1' && ch <= '5') {
        m_selected = ch - '1';
        activateSelected();
        return true;
    }

    // Let 'q'/Esc fall through to app for quit
    return false;
}

// ────────────────────────────────────────────────────────────
void MainMenuScreen::activateSelected()
{
    switch (m_selected) {
    case 0: // MATCH
        m_app.pushScreen(std::make_unique<MatchScreen>(m_app));
        break;
    case 1: // LIBRARY
        m_app.pushScreen(std::make_unique<LibraryScreen>(m_app));
        break;
    case 2: // COMPRESSOR
        m_app.pushScreen(std::make_unique<CompressorScreen>(m_app));
        break;
    case 3: // PATCH
        m_app.pushScreen(std::make_unique<PatchScreen>(m_app));
        break;
    case 4: // OPTIONS
        m_app.pushScreen(std::make_unique<OptionsScreen>(m_app));
        break;
    }
}

// ────────────────────────────────────────────────────────────
void MainMenuScreen::render(struct notcurses *nc)
{
    unsigned rows = m_app.rows();
    unsigned cols = m_app.cols();
    ncplane *std = notcurses_stdplane(nc);

    // ── Title block (top-center) ───────────────────────────
    int y = 2;
    {
        const char *title = "REMUS";
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
        ncplane_set_channels(std, ch);
        ncplane_set_styles(std, NCSTYLE_BOLD);
        int x = (static_cast<int>(cols) - static_cast<int>(strlen(title))) / 2;
        ncplane_putstr_yx(std, y++, x, title);
        ncplane_set_styles(std, NCSTYLE_NONE);
    }

    {
        const char *sub = "Retro ROM Manager";
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x99, 0x99, 0x99);
        ncplane_set_channels(std, ch);
        int x = (static_cast<int>(cols) - static_cast<int>(strlen(sub))) / 2;
        ncplane_putstr_yx(std, y++, x, sub);
    }

    {
        std::string ver = "v" + m_app.version();
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
        ncplane_set_channels(std, ch);
        int x = (static_cast<int>(cols) - static_cast<int>(ver.size())) / 2;
        ncplane_putstr_yx(std, y++, x, ver.c_str());
    }

    y += 2; // spacing

    // ── Menu items (centered) ──────────────────────────────
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        const auto &item = m_items[i];
        bool selected = (i == m_selected);

        // Build display string
        std::string display;
        if (selected)
            display = "> " + item.label + " <";
        else
            display = "  " + item.label + "  ";

        int x = (static_cast<int>(cols) - static_cast<int>(display.size())) / 2;
        if (x < 0) x = 0;

        uint64_t ch = 0;
        if (item.highlighted) {
            // Red text for MATCH and PATCH
            ncchannels_set_fg_rgb8(&ch, 0xCC, 0x00, 0x00);
        } else {
            ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
        }
        ncplane_set_channels(std, ch);

        if (selected)
            ncplane_set_styles(std, NCSTYLE_BOLD | NCSTYLE_UNDERLINE);
        else
            ncplane_set_styles(std, NCSTYLE_NONE);

        ncplane_putstr_yx(std, y, x, display.c_str());
        y++;
    }

    ncplane_set_styles(std, NCSTYLE_NONE);

    // ── Footer hint ────────────────────────────────────────
    {
        const char *hint = "j/k:navigate  Enter:select  q:quit";
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x55, 0x55, 0x55);
        ncplane_set_channels(std, ch);
        int x = (static_cast<int>(cols) - static_cast<int>(strlen(hint))) / 2;
        ncplane_putstr_yx(std, static_cast<int>(rows) - 1, x, hint);
    }

    ncplane_set_channels(std, 0);
}

std::vector<std::pair<std::string, std::string>> MainMenuScreen::keybindings() const
{
    return {
        {"j/k",   "Navigate menu"},
        {"Enter", "Select item"},
        {"1-5",   "Jump to item"},
        {"q",     "Quit"},
    };
}
