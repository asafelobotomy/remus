#pragma once

#include <notcurses/notcurses.h>
#include <string>
#include <chrono>

/**
 * @brief Transient notification bar widget.
 *
 * Displays a brief message at a fixed position with auto-dismiss.
 * Three severity levels with distinct colors.
 *
 * Owned by TuiApp; screens call app.toast("message", level).
 */
class Toast {
public:
    enum class Level { Info, Warning, Error };

    Toast() = default;

    /// Show a toast message with the given severity and duration (ms).
    void show(const std::string &message, Level level = Level::Info,
              int durationMs = 3000)
    {
        m_message = message;
        m_level = level;
        m_expiry = std::chrono::steady_clock::now()
                   + std::chrono::milliseconds(durationMs);
        m_visible = true;
    }

    /// Dismiss the toast immediately.
    void dismiss() { m_visible = false; }

    /// Check if the toast has expired.
    bool tick()
    {
        if (m_visible && std::chrono::steady_clock::now() >= m_expiry) {
            m_visible = false;
            return true; // state changed, needs redraw
        }
        return false;
    }

    bool visible() const { return m_visible; }

    /// Render the toast at the given row (typically rows - 2).
    void render(ncplane *plane, int y, unsigned cols) const
    {
        if (!m_visible) return;

        // Background bar
        uint64_t ch = 0;
        switch (m_level) {
        case Level::Info:
            ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
            ncchannels_set_bg_rgb8(&ch, 0x22, 0x44, 0x66);
            break;
        case Level::Warning:
            ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
            ncchannels_set_bg_rgb8(&ch, 0x66, 0x55, 0x00);
            break;
        case Level::Error:
            ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
            ncchannels_set_bg_rgb8(&ch, 0x66, 0x00, 0x00);
            break;
        }
        ncplane_set_channels(plane, ch);

        // Center the message
        std::string display = " " + m_message + " ";
        int x = (static_cast<int>(cols) - static_cast<int>(display.size())) / 2;
        if (x < 0) x = 0;

        // Clear the row with bg color
        std::string bg(cols, ' ');
        ncplane_putstr_yx(plane, y, 0, bg.c_str());
        // Write message
        ncplane_putstr_yx(plane, y, x, display.c_str());

        // Reset
        ch = 0;
        ncplane_set_channels(plane, ch);
    }

private:
    std::string m_message;
    Level       m_level = Level::Info;
    std::chrono::steady_clock::time_point m_expiry;
    bool        m_visible = false;
};
