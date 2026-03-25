#include "match_screen.h"
#include "app.h"

#include <QDir>
#include <cstring>

// ════════════════════════════════════════════════════════════
// Construction / Lifecycle
// ════════════════════════════════════════════════════════════

MatchScreen::MatchScreen(TuiApp &app)
    : Screen(app), m_manualOverlay(app)
{
    m_fileList.setCheckboxes(true);
    m_fileList.setRowsPerItem(2); // filename + system/hash/match

    // When the overlay applies a match, reload our display data
    m_manualOverlay.onApplied = [this](int /*fileId*/, int /*gameId*/, const std::string & /*title*/) {
        loadFromDatabase();
    };
}

MatchScreen::~MatchScreen()
{
    m_pipeline.stop();
    m_enrichTask.stop();
}

void MatchScreen::onEnter()
{
    // Only load on first visit; preserve state on back-navigation
    if (m_files.empty())
        loadFromDatabase();

    // Default focus: file list when data exists, path when empty
    if (!m_files.empty())
        m_focus = Focus::FileList;
    else
        m_focus = Focus::PathInput;
}

void MatchScreen::onLeave()
{
    m_pipeline.stop();
    m_enrichTask.cancel();
    m_manualOverlay.close();
}

// ════════════════════════════════════════════════════════════
// Input handling
// ════════════════════════════════════════════════════════════

bool MatchScreen::handleInput(struct notcurses *nc, const ncinput &ni, int ch)
{
    // Overlay captures all input when active
    if (m_manualOverlay.isActive())
        return handleOverlayInput(ch, ni);
    if (ch == NCKEY_ESC) {
        if (m_pipelineRunning.load()) {
            m_pipeline.stop();
            m_pipelineRunning = false;
            m_progressBar.set(0, 0, "cancelled");
            m_app.toast("Scan cancelled", Toast::Level::Warning);
            return true;
        }
        // Don't consume Esc — let app handle (pop screen / quit)
        return false;
    }

    // Mouse click-to-select
    if (ch == NCKEY_BUTTON1 && ni.evtype == NCTYPE_PRESS) {
        int row = ni.y, col = ni.x;
        unsigned cols = m_app.cols();
        int fieldW = static_cast<int>(cols) - 12;
        if (m_pathInput.hitTest(row, col, 1, 2, fieldW)) {
            m_focus = Focus::PathInput;
        } else {
            // Scan button hit-test: row 1, near right edge ("[SCAN]" or "RUNNING")
            int scanX = static_cast<int>(cols) - 8;
            if (row == 1 && col >= scanX && col < scanX + 8) {
                m_focus = Focus::ScanButton;
                if (!m_pipelineRunning.load()) startScan();
            } else if (row >= m_lastLayout.bodyY && row < m_lastLayout.bodyY + m_lastLayout.bodyH) {
                if (col < m_lastLayout.leftW) {
                    m_focus = Focus::FileList;
                    m_fileList.handleClick(row, m_lastLayout.bodyY, m_lastLayout.bodyH);
                } else {
                    m_focus = Focus::DetailPane;
                }
            }
        }
        return true;
    }

    // Tab cycles focus (Path -> Scan -> List -> Detail)
    if (ch == '\t') {
        if (m_focus == Focus::PathInput)
            m_focus = Focus::ScanButton;
        else if (m_focus == Focus::ScanButton)
            m_focus = Focus::FileList;
        else if (m_focus == Focus::FileList)
            m_focus = Focus::DetailPane;
        else
            m_focus = Focus::PathInput;
        return true;
    }

    // ── Path input mode ────────────────────────────────────
    if (m_focus == Focus::PathInput) {
        if (TextInput::isSubmit(ch)) {
            startScan();
            return true;
        }
        return m_pathInput.handleInput(ch);
    }

    // ── Scan button mode ───────────────────────────────────
    if (m_focus == Focus::ScanButton) {
        if (TextInput::isSubmit(ch) || ch == ' ')
        {
            startScan();
            return true;
        }
        // Allow quick hop back to path with left-arrow
        if (ch == NCKEY_LEFT) {
            m_focus = Focus::PathInput;
            return true;
        }
        return false;
    }

    // ── File list mode ─────────────────────────────────────
    if (m_focus == Focus::FileList) {
        auto action = m_fileList.handleInput(ch);
        if (action == SelectableList::Action::SelectionChanged) {
            // Skip over header rows: nudge in the direction of travel
            std::lock_guard<std::mutex> lock(m_filesMutex);
            bool goDown = (ch == 'j' || ch == NCKEY_DOWN || ch == NCKEY_SCROLL_DOWN || ch == 'G');
            int sel = m_fileList.selected();
            while (sel >= 0 && sel < static_cast<int>(m_files.size()) &&
                   m_files[static_cast<size_t>(sel)].isHeader) {
                if (goDown && sel < static_cast<int>(m_files.size()) - 1)
                    m_fileList.setSelected(++sel);
                else if (!goDown && sel > 0)
                    m_fileList.setSelected(--sel);
                else
                    break;
            }
            return true;
        }
        if (action == SelectableList::Action::ToggleCheck) {
            int sel = m_fileList.selected();
            std::lock_guard<std::mutex> lock(m_filesMutex);
            if (sel >= 0 && sel < static_cast<int>(m_files.size())) {
                auto &entry = m_files[static_cast<size_t>(sel)];
                if (!entry.isHeader)
                    entry.checked = !entry.checked;
            }
            return true;
        }
        if (action == SelectableList::Action::ToggleAll) {
            std::lock_guard<std::mutex> lock(m_filesMutex);
            bool allChecked = std::all_of(m_files.begin(), m_files.end(),
                                          [](const FileEntry &e) { return e.isHeader || e.checked; });
            for (auto &f : m_files)
                if (!f.isHeader) f.checked = !allChecked;
            return true;
        }
        if (action != SelectableList::Action::None)
            return true;

        // 's' to trigger scan from file list
        if (ch == 's' || ch == 'S') {
            startScan();
            return true;
        }
        // 'c' to confirm, 'x' to reject selected match
        if (ch == 'c' || ch == 'C') {
            confirmSelectedMatch();
            return true;
        }
        if (ch == 'x' || ch == 'X') {
            rejectSelectedMatch();
            return true;
        }
        // 'm' to open manual match overlay
        if (ch == 'm' || ch == 'M') {
            openManualMatch();
            return true;
        }
        // 'e' to enrich metadata from online providers
        if (ch == 'e' || ch == 'E') {
            enrichSelectedMetadata();
            return true;
        }
        return false;
    }

    // ── Detail pane mode ───────────────────────────────────
    if (m_focus == Focus::DetailPane) {
        // j/k scrolls file list selection from detail pane too
        auto action = m_fileList.handleInput(ch);
        if (action != SelectableList::Action::None)
            return true;
        return false;
    }

    return false;
}

// ════════════════════════════════════════════════════════════
// Tick (periodic update)
// ════════════════════════════════════════════════════════════

bool MatchScreen::tick()
{
    if (m_pipelineRunning.load())
        return true; // keep redrawing while pipeline is active
    if (m_manualOverlay.isActive())
        return true; // redraw while overlay is up
    return false;
}


void MatchScreen::drawFooter(ncplane *plane, unsigned rows, unsigned cols)
{
    const char *hint = nullptr;
    switch (m_focus) {
    case Focus::PathInput:
        hint = "Enter:scan  Tab:next pane  Esc:back";
        break;
    case Focus::ScanButton:
        hint = "Enter/Space:scan  Left:path input  Tab:file list";
        break;
    case Focus::FileList:
        hint = "j/k:navigate  c:confirm  x:reject  m:manual  e:enrich  s:scan  Tab:next  Esc:back";
        break;
    case Focus::DetailPane:
        hint = "j/k:navigate files  Tab:next pane  Esc:back";
        break;
    }

    uint64_t ch = 0;
    ncchannels_set_fg_rgb8(&ch, 0x55, 0x55, 0x55);
    ncplane_set_channels(plane, ch);
    if (hint) {
        int x = (static_cast<int>(cols) - static_cast<int>(strlen(hint))) / 2;
        ncplane_putstr_yx(plane, static_cast<int>(rows) - 1, x, hint);
    }
}

// ════════════════════════════════════════════════════════════
// Actions
// ════════════════════════════════════════════════════════════

void MatchScreen::startScan()
{
    if (m_pipelineRunning.load())
        return;

    std::string path = m_pathInput.value();
    if (path.empty()) {
        m_app.toast("Enter a source path to scan", Toast::Level::Warning);
        m_focus = Focus::PathInput;
        return;
    }

    // Expand ~ to home directory
    if (path[0] == '~') {
        QString home = QDir::homePath();
        path = home.toStdString() + path.substr(1);
    }

    m_pipelineRunning = true;
    m_progressBar.set(0, 0, "scanning");

    m_pipeline.start(path,
        [this](const PipelineProgress &p) {
            const char *label = "idle";
            switch (p.stage) {
            case PipelineProgress::Scanning: label = "scanning"; break;
            case PipelineProgress::Hashing:  label = "hashing";  break;
            case PipelineProgress::Matching: label = "matching"; break;
            default: break;
            }
            m_progressBar.set(p.done, p.total, label, p.path);
        },
        [this](const std::string &msg) {
            if (msg.find("No matches were found") != std::string::npos) {
                m_app.post([this]() {
                    m_progressBar.set(m_progressBar.total(), m_progressBar.total(), "no matches");
                    m_app.toast("No matches were found", Toast::Level::Warning);
                });
            } else if (msg.find("Scan found 0 file") != std::string::npos) {
                m_app.post([this]() {
                    m_progressBar.set(0, 0, "no files found");
                    m_app.toast("No files found in source", Toast::Level::Warning);
                });
            } else if (msg.find("Pipeline done") != std::string::npos) {
                m_app.post([this]() {
                    m_progressBar.set(m_progressBar.total(), m_progressBar.total(), "done");
                    m_pipelineRunning = false;
                    loadFromDatabase();
                    m_app.toast("Scan complete", Toast::Level::Success);
                });
            }
        },
        &m_app.db()
    );
}

std::vector<std::pair<std::string, std::string>> MatchScreen::keybindings() const
{
    return {
        {"Tab",     "Cycle focus (Path / List / Detail)"},
        {"Enter",   "Start scan (when path focused)"},
        {"j/k",     "Navigate file list"},
        {"g/G",     "Jump to first/last file"},
        {"Space",   "Toggle checkbox"},
        {"a",       "Toggle all checkboxes"},
        {"c",       "Confirm selected match"},
        {"x",       "Reject selected match"},
        {"m",       "Manual match search"},
        {"e",       "Enrich metadata (online providers)"},
        {"s",       "Start scan (from file list)"},
        {"Esc",     "Cancel running scan / go back"},
    };
}

// ════════════════════════════════════════════════════════════
// Confirm / reject helpers
// ════════════════════════════════════════════════════════════

void MatchScreen::confirmSelectedMatch()
{
    int sel = m_fileList.selected();
    std::lock_guard<std::mutex> lock(m_filesMutex);
    if (sel < 0 || sel >= static_cast<int>(m_files.size())) return;
    FileEntry &e = m_files[static_cast<size_t>(sel)];
    if (e.isHeader || e.fileId == 0) return;

    if (m_app.db().confirmMatch(e.fileId)) {
        e.confirmStatus = ConfirmStatus::Confirmed;
        m_app.toast("Match confirmed", Toast::Level::Success);
    }
}

void MatchScreen::rejectSelectedMatch()
{
    int sel = m_fileList.selected();
    std::lock_guard<std::mutex> lock(m_filesMutex);
    if (sel < 0 || sel >= static_cast<int>(m_files.size())) return;
    FileEntry &e = m_files[static_cast<size_t>(sel)];
    if (e.isHeader || e.fileId == 0) return;

    if (m_app.db().rejectMatch(e.fileId)) {
        e.confirmStatus = ConfirmStatus::Rejected;
        m_app.toast("Match rejected", Toast::Level::Warning);
    }
}

// ════════════════════════════════════════════════════════════
// Manual match overlay (delegates to ManualMatchOverlay)
// ════════════════════════════════════════════════════════════

void MatchScreen::openManualMatch()
{
    int sel = m_fileList.selected();
    std::string title, system;
    int fileId = 0;
    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        if (sel < 0 || sel >= static_cast<int>(m_files.size())) return;
        const FileEntry &e = m_files[static_cast<size_t>(sel)];
        if (e.isHeader || e.fileId == 0) return;
        fileId = e.fileId;
        system = e.system;
        title  = e.title.empty() ? e.filename : e.title;
        if (title == e.filename) {
            auto dot = title.rfind('.');
            if (dot != std::string::npos) title = title.substr(0, dot);
        }
    }

    m_manualOverlay.open(fileId, system, title);
}

bool MatchScreen::handleOverlayInput(int ch, const ncinput &ni)
{
    return m_manualOverlay.handleInput(ch, ni);
}

void MatchScreen::drawManualMatchOverlay(ncplane *plane, unsigned rows, unsigned cols)
{
    m_manualOverlay.render(plane, rows, cols);
}
