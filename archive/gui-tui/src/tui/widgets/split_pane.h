#pragma once

#include <notcurses/notcurses.h>
#include <string>
#include <functional>

/**
 * @brief Reusable split pane widget for TUI screens.
 *
 * Manages a horizontal two-panel layout with a vertical separator.
 * Used by MatchScreen, CompressorScreen, and LibraryScreen.
 *
 * Usage:
 *   SplitPane pane;
 *   auto layout = pane.compute(cols, rows, headerH, footerH);
 *   pane.renderSeparator(plane, layout);
 *   // Use layout.leftW, layout.rightW, layout.bodyH, etc. for rendering
 */
class SplitPane {
public:
    SplitPane() = default;

    /// Set the left panel width as a percentage (default 55%).
    void setLeftPercent(int pct) { m_leftPct = pct; }

    struct Layout {
        int leftW;      // width of left pane
        int rightW;     // width of right pane
        int rightX;     // starting X of right pane (leftW + 1)
        int bodyY;      // starting Y of body area (after header)
        int bodyH;      // height of body area
        int progressY;  // starting Y of progress area (bodyY + bodyH)
    };

    /// Compute the layout given terminal dimensions and header/footer sizes.
    Layout compute(unsigned cols, unsigned rows, int headerH, int footerH,
                   int progressH = 2) const
    {
        Layout l;
        l.bodyH = static_cast<int>(rows) - headerH - footerH - progressH;
        if (l.bodyH < 3) l.bodyH = 3;

        l.leftW = static_cast<int>(cols) * m_leftPct / 100;
        if (l.leftW < 20) l.leftW = 20;
        l.rightW = static_cast<int>(cols) - l.leftW - 1; // 1 col for separator
        if (l.rightW < 10) l.rightW = 10;
        l.rightX = l.leftW + 1;
        l.bodyY = headerH;
        l.progressY = headerH + l.bodyH;

        return l;
    }

    /// Render the vertical separator between panes.
    void renderSeparator(ncplane *plane, const Layout &layout) const
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x44, 0x44, 0x44);
        ncplane_set_channels(plane, ch);
        for (int y = layout.bodyY; y < layout.bodyY + layout.bodyH; ++y)
            ncplane_putstr_yx(plane, y, layout.leftW, "│");
    }

private:
    int m_leftPct = 55;
};
