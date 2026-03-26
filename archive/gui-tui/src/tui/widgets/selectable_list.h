#pragma once

#include <notcurses/notcurses.h>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

/**
 * @brief Reusable scrollable list widget with j/k navigation.
 *
 * Manages selection, scroll offset, optional checkboxes, and renders
 * items via a caller-provided callback. Consolidates the list navigation
 * pattern duplicated across 6+ screens.
 *
 * Usage:
 *   SelectableList list;
 *   list.setCount(items.size());
 *   if (list.handleInput(ch)) return true;
 *   list.render(plane, startY, height, width, focused,
 *       [&](ncplane *p, int y, int idx, bool selected, bool focused) { ... });
 */
class SelectableList {
public:
    SelectableList() = default;

    // ── State ──────────────────────────────────────────────
    int  count() const { return m_count; }
    void setCount(int count)
    {
        m_count = count;
        clampSelection();
    }

    int  selected() const { return m_selected; }
    void setSelected(int idx)
    {
        m_selected = idx;
        clampSelection();
    }

    int  scroll() const { return m_scroll; }
    void setScroll(int s) { m_scroll = s; }

    bool hasSelection() const { return m_selected >= 0 && m_selected < m_count; }

    /// Number of rows each item occupies (default 1). Affects render/scroll.
    void setRowsPerItem(int n) { m_rowsPerItem = (n < 1) ? 1 : n; }
    int  rowsPerItem() const { return m_rowsPerItem; }

    // ── Checkbox support ───────────────────────────────────
    void setCheckboxes(bool enabled) { m_checkboxes = enabled; }
    bool checkboxes() const { return m_checkboxes; }

    /// Checkbox state is managed externally — this just returns
    /// whether the toggle key was pressed on the selected item.

    // ── Input handling ─────────────────────────────────────
    enum class Action {
        None,
        SelectionChanged,
        ToggleCheck,    // Space pressed on current item
        ToggleAll,      // 'a' pressed
        Submit,         // Enter pressed
    };

    /// Process a keyboard event. Returns the action taken.
    Action handleInput(int ch)
    {
        if (ch == 'j' || ch == NCKEY_DOWN || ch == NCKEY_SCROLL_DOWN) {
            if (m_selected < m_count - 1) {
                m_selected++;
                return Action::SelectionChanged;
            }
            return Action::None;
        }
        if (ch == 'k' || ch == NCKEY_UP || ch == NCKEY_SCROLL_UP) {
            if (m_selected > 0) {
                m_selected--;
                return Action::SelectionChanged;
            }
            return Action::None;
        }
        if (ch == 'g') {
            m_selected = 0;
            m_scroll = 0;
            return Action::SelectionChanged;
        }
        if (ch == 'G') {
            if (m_count > 0) m_selected = m_count - 1;
            return Action::SelectionChanged;
        }
        if (ch == ' ' && m_checkboxes) {
            return Action::ToggleCheck;
        }
        if (ch == 'a' && m_checkboxes) {
            return Action::ToggleAll;
        }
        if (ch == NCKEY_ENTER || ch == '\n' || ch == '\r') {
            return Action::Submit;
        }
        return Action::None;
    }

    /// Check if a mouse click at (row, col) falls within the list area
    /// and update selection accordingly.
    /// Returns the new selected index, or -1 if the click was outside.
    int handleClick(int clickRow, int listStartY, int listHeight)
    {
        if (clickRow < listStartY || clickRow >= listStartY + listHeight)
            return -1;

        int relRow = (clickRow - listStartY) / m_rowsPerItem + m_scroll;
        if (relRow >= 0 && relRow < m_count) {
            m_selected = relRow;
            return m_selected;
        }
        return -1;
    }

    // ── Scroll management ──────────────────────────────────
    /// Ensure the selected item is visible. Call before rendering.
    void ensureVisible(int visibleRows)
    {
        if (m_selected < 0) return;
        int itemsVisible = visibleRows / m_rowsPerItem;
        if (itemsVisible < 1) itemsVisible = 1;
        if (m_selected < m_scroll)
            m_scroll = m_selected;
        if (m_selected >= m_scroll + itemsVisible)
            m_scroll = m_selected - itemsVisible + 1;
        if (m_scroll < 0) m_scroll = 0;
    }

    // ── Rendering ──────────────────────────────────────────
    using RenderItemCallback = std::function<void(ncplane *plane, int y, int index,
                                                   bool isSelected, bool isFocused)>;

    /// Render the list using the callback for each visible item.
    /// The callback is responsible for drawing the actual content.
    /// The callback receives the y coordinate of the first row for that item.
    void render(ncplane *plane, int startY, int height, bool focused,
                const RenderItemCallback &renderItem) const
    {
        if (m_count == 0) return;

        int y = startY;
        for (int i = m_scroll; i < m_count && y + m_rowsPerItem - 1 < startY + height; ++i, y += m_rowsPerItem) {
            bool selected = (i == m_selected);
            renderItem(plane, y, i, selected, focused);
        }
    }

    /// Render a header label for the list (e.g., "Files (42)")
    void renderHeader(ncplane *plane, int y, int x, const std::string &title,
                      bool focused) const
    {
        uint64_t ch = 0;
        uint8_t bright = focused ? 0xFF : 0x88;
        ncchannels_set_fg_rgb8(&ch, bright, bright, bright);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        ncplane_putstr_yx(plane, y, x, title.c_str());
        ncplane_set_styles(plane, NCSTYLE_NONE);
    }

private:
    void clampSelection()
    {
        if (m_count <= 0) {
            m_selected = -1;
            return;
        }
        if (m_selected >= m_count) m_selected = m_count - 1;
        if (m_selected < 0) m_selected = 0;
    }

    int  m_count = 0;
    int  m_selected = -1;
    int  m_scroll = 0;
    int  m_rowsPerItem = 1;
    bool m_checkboxes = false;
};
