#pragma once

#include <notcurses/notcurses.h>
#include <string>
#include <cstdio>

/**
 * @brief Reusable progress bar widget for TUI screens.
 *
 * Renders: separator line + [####    ] stage done/total + current item
 * Consolidates the identical progress bar pattern from MatchScreen,
 * CompressorScreen, and PatchScreen.
 */
class ProgressBarWidget {
public:
    ProgressBarWidget() = default;

    // ── State ──────────────────────────────────────────────
    void set(int done, int total, const std::string &label,
             const std::string &currentItem = {})
    {
        m_done = done;
        m_total = total;
        m_label = label;
        m_currentItem = currentItem;
    }

    void reset()
    {
        m_done = 0;
        m_total = 0;
        m_label.clear();
        m_currentItem.clear();
    }

    int done() const { return m_done; }
    int total() const { return m_total; }
    const std::string &label() const { return m_label; }

    // ── Rendering ──────────────────────────────────────────
    /// Render a 2-row progress bar: separator on row y, bar on row y+1.
    void render(ncplane *plane, int y, unsigned cols) const
    {
        // Row 1: separator line
        {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0x44, 0x44, 0x44);
            ncplane_set_channels(plane, ch);
            std::string sep(cols, '-');
            ncplane_putstr_yx(plane, y, 0, sep.c_str());
        }
        y++;

        // Bar dimensions
        const int barW = 30;
        int filled = 0;
        if (m_total > 0) {
            filled = m_done * barW / m_total;
            if (filled > barW) filled = barW;
        }

        // Build bar string: [#####    ]
        std::string bar;
        bar.push_back('[');
        bar.append(static_cast<size_t>(filled), '#');
        bar.append(static_cast<size_t>(barW - filled), ' ');
        bar.push_back(']');

        // "Progress: "
        {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
            ncplane_set_channels(plane, ch);
            ncplane_putstr_yx(plane, y, 2, "Progress: ");
        }

        // Bar (green)
        {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0x00, 0xAA, 0x00);
            ncplane_set_channels(plane, ch);
            ncplane_putstr(plane, bar.c_str());
        }

        // Stage label + done/total
        char info[128];
        snprintf(info, sizeof(info), "  %s %d/%d",
                 m_label.c_str(), m_done, m_total);
        {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
            ncplane_set_channels(plane, ch);
            ncplane_putstr(plane, info);
        }

        // Current item (truncated, dim)
        if (!m_currentItem.empty()) {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0x55, 0x55, 0x55);
            ncplane_set_channels(plane, ch);
            int remaining = static_cast<int>(cols) - 2 - 10 - barW - 2
                            - static_cast<int>(strlen(info));
            if (remaining > 5) {
                std::string path = m_currentItem;
                if (static_cast<int>(path.size()) > remaining)
                    path = "..." + path.substr(path.size()
                           - static_cast<size_t>(remaining) + 3);
                ncplane_putstr(plane, "  ");
                ncplane_putstr(plane, path.c_str());
            }
        }
    }

private:
    int         m_done = 0;
    int         m_total = 0;
    std::string m_label;
    std::string m_currentItem;
};
