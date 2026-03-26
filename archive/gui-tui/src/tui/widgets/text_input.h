#pragma once

#include <notcurses/notcurses.h>
#include <string>

/**
 * @brief Reusable text input widget for TUI screens.
 *
 * Manages a single-line text field with cursor, focus highlight,
 * truncation, and keyboard input handling. All rendering and input
 * logic that was duplicated across 5+ screens is consolidated here.
 *
 * Usage:
 *   TextInput input;
 *   input.setLabel("Path: ");
 *   // In handleInput():
 *   if (input.handleInput(ch)) return true;
 *   // In render():
 *   input.render(plane, y, x, width, focused);
 */
class TextInput {
public:
    TextInput() = default;
    explicit TextInput(const std::string &label, const std::string &placeholder = "")
        : m_label(label), m_placeholder(placeholder) {}

    // ── State access ───────────────────────────────────────
    const std::string &value() const { return m_value; }
    void setValue(const std::string &v) { m_value = v; }
    void clear() { m_value.clear(); }
    bool empty() const { return m_value.empty(); }

    const std::string &label() const { return m_label; }
    void setLabel(const std::string &l) { m_label = l; }

    void setPlaceholder(const std::string &p) { m_placeholder = p; }
    void setMasked(bool masked) { m_masked = masked; }

    // ── Input handling ─────────────────────────────────────
    /// Process a keyboard event. Returns true if consumed.
    bool handleInput(int ch)
    {
        if (ch == NCKEY_BACKSPACE || ch == 127) {
            if (!m_value.empty()) {
                m_value.pop_back();
                return true;
            }
            return false; // nothing to delete
        }
        if (ch >= 32 && ch < 127) {
            m_value.push_back(static_cast<char>(ch));
            return true;
        }
        return false;
    }

    /// Returns true if Enter was pressed (caller decides what to do)
    static bool isSubmit(int ch)
    {
        return ch == NCKEY_ENTER || ch == '\n' || ch == '\r';
    }

    // ── Rendering ──────────────────────────────────────────
    /// Render the input field at (y, x) with the given width.
    void render(ncplane *plane, int y, int x, int width, bool focused) const
    {
        // Label
        if (!m_label.empty()) {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0x99, 0x99, 0x99);
            ncplane_set_channels(plane, ch);
            ncplane_putstr_yx(plane, y, x, m_label.c_str());
        }

        int fieldStart = x + static_cast<int>(m_label.size());
        int fieldWidth = width - static_cast<int>(m_label.size());
        if (fieldWidth < 4) fieldWidth = 4;

        // Field colors
        uint64_t ch = 0;
        if (focused) {
            ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
            ncchannels_set_bg_rgb8(&ch, 0x33, 0x33, 0x33);
        } else {
            ncchannels_set_fg_rgb8(&ch, 0xAA, 0xAA, 0xAA);
        }
        ncplane_set_channels(plane, ch);

        // Build display string
        std::string display;
        if (m_value.empty() && !focused) {
            display = m_placeholder.empty() ? "" : m_placeholder;
        } else if (m_masked && !m_value.empty()) {
            display = std::string(m_value.size(), '*');
            if (focused) display += "_";
        } else {
            display = m_value;
            if (focused) display += "_";
        }

        // Truncate with ellipsis if too long
        if (static_cast<int>(display.size()) > fieldWidth)
            display = "..." + display.substr(display.size() - static_cast<size_t>(fieldWidth) + 3);
        display.resize(static_cast<size_t>(fieldWidth), ' ');
        ncplane_putstr_yx(plane, y, fieldStart, display.c_str());

        // Reset bg
        ch = 0;
        ncplane_set_channels(plane, ch);
    }

    // ── Hit testing (for mouse support) ────────────────────
    /// Returns true if the given (row, col) is within the field area.
    bool hitTest(int row, int col, int fieldRow, int fieldX, int fieldWidth) const
    {
        return row == fieldRow &&
               col >= fieldX + static_cast<int>(m_label.size()) &&
               col < fieldX + fieldWidth;
    }

private:
    std::string m_value;
    std::string m_label;
    std::string m_placeholder;
    bool        m_masked = false;
};
