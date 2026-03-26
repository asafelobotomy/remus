#pragma once

#include <notcurses/notcurses.h>
#include <string>
#include <vector>
#include <utility>
#include <cstring>

/**
 * @brief Help overlay widget showing keybindings for the current screen.
 *
 * Renders a centered semi-transparent box on top of the current screen
 * with a list of keybinding descriptions.
 *
 * Toggled with '?' globally in TuiApp. Press '?' or Esc to dismiss.
 */
class HelpOverlay {
public:
    using KeyBinding = std::pair<std::string, std::string>; // {key, description}

    HelpOverlay() = default;

    void show(const std::string &screenName,
              const std::vector<KeyBinding> &bindings)
    {
        m_screenName = screenName;
        m_bindings = bindings;
        m_visible = true;
    }

    void dismiss() { m_visible = false; }
    bool visible() const { return m_visible; }

    /// Handle input when overlay is visible. Returns true if consumed.
    bool handleInput(int ch)
    {
        if (!m_visible) return false;
        if (ch == '?' || ch == NCKEY_ESC || ch == 'q') {
            m_visible = false;
            return true;
        }
        // Consume all input while visible (modal)
        return true;
    }

    /// Render the overlay centered on the terminal.
    void render(ncplane *plane, unsigned rows, unsigned cols) const
    {
        if (!m_visible) return;

        // Calculate box dimensions
        int maxKeyW = 0;
        int maxDescW = 0;
        for (const auto &[key, desc] : m_bindings) {
            if (static_cast<int>(key.size()) > maxKeyW)
                maxKeyW = static_cast<int>(key.size());
            if (static_cast<int>(desc.size()) > maxDescW)
                maxDescW = static_cast<int>(desc.size());
        }

        int contentW = maxKeyW + 3 + maxDescW; // "key  description"
        int boxW = contentW + 6; // padding
        int boxH = static_cast<int>(m_bindings.size()) + 5; // title + divider + padding

        // Clamp to terminal
        if (boxW > static_cast<int>(cols) - 4) boxW = static_cast<int>(cols) - 4;
        if (boxH > static_cast<int>(rows) - 2) boxH = static_cast<int>(rows) - 2;

        int startX = (static_cast<int>(cols) - boxW) / 2;
        int startY = (static_cast<int>(rows) - boxH) / 2;
        if (startX < 1) startX = 1;
        if (startY < 1) startY = 1;

        // Draw box background
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
        ncchannels_set_bg_rgb8(&ch, 0x11, 0x11, 0x22);
        ncplane_set_channels(plane, ch);

        std::string emptyRow(static_cast<size_t>(boxW), ' ');
        for (int y = startY; y < startY + boxH; ++y)
            ncplane_putstr_yx(plane, y, startX, emptyRow.c_str());

        // Title
        {
            uint64_t tc = 0;
            ncchannels_set_fg_rgb8(&tc, 0xFF, 0xFF, 0xFF);
            ncchannels_set_bg_rgb8(&tc, 0x11, 0x11, 0x22);
            ncplane_set_channels(plane, tc);
            ncplane_set_styles(plane, NCSTYLE_BOLD);
            std::string title = "Help: " + m_screenName;
            int tx = startX + (boxW - static_cast<int>(title.size())) / 2;
            ncplane_putstr_yx(plane, startY + 1, tx, title.c_str());
            ncplane_set_styles(plane, NCSTYLE_NONE);
        }

        // Divider
        {
            uint64_t dc = 0;
            ncchannels_set_fg_rgb8(&dc, 0x44, 0x44, 0x66);
            ncchannels_set_bg_rgb8(&dc, 0x11, 0x11, 0x22);
            ncplane_set_channels(plane, dc);
            std::string div(static_cast<size_t>(boxW - 4), '-');
            ncplane_putstr_yx(plane, startY + 2, startX + 2, div.c_str());
        }

        // Keybindings
        int y = startY + 3;
        int innW = boxW - 6;
        for (const auto &[key, desc] : m_bindings) {
            if (y >= startY + boxH - 1) break;

            // Key (highlighted)
            {
                uint64_t kc = 0;
                ncchannels_set_fg_rgb8(&kc, 0xAA, 0xCC, 0xFF);
                ncchannels_set_bg_rgb8(&kc, 0x11, 0x11, 0x22);
                ncplane_set_channels(plane, kc);
                std::string keyPad = key;
                keyPad.resize(static_cast<size_t>(maxKeyW), ' ');
                ncplane_putstr_yx(plane, y, startX + 3, keyPad.c_str());
            }

            // Description
            {
                uint64_t dc = 0;
                ncchannels_set_fg_rgb8(&dc, 0xCC, 0xCC, 0xCC);
                ncchannels_set_bg_rgb8(&dc, 0x11, 0x11, 0x22);
                ncplane_set_channels(plane, dc);
                std::string d = desc;
                int dmax = innW - maxKeyW - 3;
                if (dmax > 0 && static_cast<int>(d.size()) > dmax)
                    d = d.substr(0, static_cast<size_t>(dmax - 3)) + "...";
                ncplane_putstr_yx(plane, y, startX + 3 + maxKeyW + 3, d.c_str());
            }
            y++;
        }

        // Footer hint
        {
            uint64_t fc = 0;
            ncchannels_set_fg_rgb8(&fc, 0x66, 0x66, 0x88);
            ncchannels_set_bg_rgb8(&fc, 0x11, 0x11, 0x22);
            ncplane_set_channels(plane, fc);
            const char *hint = "Press ? or Esc to close";
            int fx = startX + (boxW - static_cast<int>(strlen(hint))) / 2;
            ncplane_putstr_yx(plane, startY + boxH - 1, fx, hint);
        }

        // Reset
        ch = 0;
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_NONE);
    }

private:
    std::string m_screenName;
    std::vector<KeyBinding> m_bindings;
    bool m_visible = false;
};
