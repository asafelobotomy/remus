#include "library_screen.h"
#include "app.h"

#include "../services/match_service.h"

#include <cstring>

// ════════════════════════════════════════════════════════════
// Construction / Lifecycle
// ════════════════════════════════════════════════════════════

LibraryScreen::LibraryScreen(TuiApp &app)
    : Screen(app)
{
}

void LibraryScreen::onEnter()
{
    // Only load on first visit; preserve state on back-navigation
    if (m_allEntries.empty())
        loadFromDatabase();
}

// ════════════════════════════════════════════════════════════
// Input handling
// ════════════════════════════════════════════════════════════

bool LibraryScreen::handleInput(struct notcurses *, const ncinput &ni, int ch)
{
    // Esc: clear filter first (regardless of focus), then go back
    if (ch == 27) { // Esc
        if (!m_filterInput.value().empty()) {
            m_filterInput.clear();
            m_focus = Focus::FileList;
            applyFilter();
            return true;
        }
        return false;  // let app pop screen
    }

    // Mouse click-to-select
    if (ch == NCKEY_BUTTON1 && ni.evtype == NCTYPE_PRESS) {
        int row = ni.y, col = ni.x;
        unsigned cols = m_app.cols();
        int filterW = static_cast<int>(cols) / 3;
        if (m_filterInput.hitTest(row, col, 0, 12, filterW)) {
            m_focus = Focus::FilterInput;
        } else if (row >= m_lastLayout.bodyY && row < m_lastLayout.bodyY + m_lastLayout.bodyH) {
            if (col < m_lastLayout.leftW) {
                m_focus = Focus::FileList;
                m_fileList.handleClick(row, m_lastLayout.bodyY, m_lastLayout.bodyH);
            } else {
                m_focus = Focus::DetailPane;
            }
        }
        return true;
    }

    // Tab cycles focus
    if (ch == '\t') {
        if (m_focus == Focus::FilterInput)
            m_focus = Focus::FileList;
        else if (m_focus == Focus::FileList)
            m_focus = Focus::DetailPane;
        else
            m_focus = Focus::FilterInput;
        return true;
    }

    // 'f' to jump to filter
    if (ch == 'f' && m_focus != Focus::FilterInput) {
        m_focus = Focus::FilterInput;
        return true;
    }

    // 'r' to refresh
    if (ch == 'r') {
        loadFromDatabase();
        return true;
    }

    // ── Filter input ───────────────────────────────────────
    if (m_focus == Focus::FilterInput) {
        if (TextInput::isSubmit(ch)) {
            m_focus = Focus::FileList;
            return true;
        }
        bool handled = m_filterInput.handleInput(ch);
        if (handled) applyFilter();
        return handled;
    }

    // ── File list ──────────────────────────────────────────
    if (m_focus == Focus::FileList) {
        auto action = m_fileList.handleInput(ch);
        if (action != SelectableList::Action::None) return true;

        if (ch == 'c') { confirmMatch(); return true; }
        if (ch == 'x') { rejectMatch(); return true; }
        return false;
    }

    // ── Detail pane ────────────────────────────────────────
    if (m_focus == Focus::DetailPane) {
        auto action = m_fileList.handleInput(ch);
        if (action != SelectableList::Action::None) return true;

        if (ch == 'c') { confirmMatch(); return true; }
        if (ch == 'x') { rejectMatch(); return true; }
        return false;
    }

    return false;
}

// ════════════════════════════════════════════════════════════
// Tick
// ════════════════════════════════════════════════════════════

bool LibraryScreen::tick()
{
    return false; // static screen — redraws only on input
}

// ════════════════════════════════════════════════════════════
// Actions
// ════════════════════════════════════════════════════════════

void LibraryScreen::confirmMatch()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int sel = m_fileList.selected();
    if (sel < 0 || sel >= static_cast<int>(m_entries.size()))
        return;
    const auto &e = m_entries[sel];
    if (e.isHeader || e.fileId == 0) return;

    Remus::MatchService ms;
    ms.confirmMatch(&m_app.db(), e.fileId);

    // Update in-memory state for immediate visual feedback
    for (auto &ae : m_allEntries) {
        if (ae.fileId == e.fileId) {
            ae.confirmStatus = ConfirmationStatus::Confirmed;
            break;
        }
    }
    // Also update the displayed entry directly
    m_entries[sel].confirmStatus = ConfirmationStatus::Confirmed;
    m_app.toast("Match confirmed", Toast::Level::Success, 1500);
}

void LibraryScreen::rejectMatch()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    int sel = m_fileList.selected();
    if (sel < 0 || sel >= static_cast<int>(m_entries.size()))
        return;
    const auto &e = m_entries[sel];
    if (e.isHeader || e.fileId == 0) return;

    Remus::MatchService ms;
    ms.rejectMatch(&m_app.db(), e.fileId);

    // Update in-memory state for immediate visual feedback
    for (auto &ae : m_allEntries) {
        if (ae.fileId == e.fileId) {
            ae.confirmStatus = ConfirmationStatus::Rejected;
            break;
        }
    }
    m_entries[sel].confirmStatus = ConfirmationStatus::Rejected;
    m_app.toast("Match rejected", Toast::Level::Warning, 1500);
}

std::vector<std::pair<std::string, std::string>> LibraryScreen::keybindings() const
{
    return {
        {"Tab",  "Cycle focus"},
        {"f",    "Focus filter"},
        {"r",    "Refresh from DB"},
        {"c",    "Confirm match"},
        {"x",    "Reject match"},
        {"j/k",  "Navigate list"},
        {"g/G",  "First / last"},
        {"Esc",  "Clear filter / back"},
    };
}
