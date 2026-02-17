#include "match_screen.h"
#include "app.h"

#include "../core/database.h"
#include "../core/scanner.h"
#include "../core/hasher.h"
#include "../core/matching_engine.h"
#include "../core/system_detector.h"

#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include <cstring>

// ════════════════════════════════════════════════════════════
// Construction / Lifecycle
// ════════════════════════════════════════════════════════════

MatchScreen::MatchScreen(TuiApp &app)
    : Screen(app)
{
    m_fileList.setCheckboxes(true);
    m_fileList.setRowsPerItem(2); // filename + system/hash/match
}

MatchScreen::~MatchScreen()
{
    m_pipeline.stop();
}

void MatchScreen::onEnter()
{
    // Only load on first visit; preserve state on back-navigation
    if (m_files.empty())
        loadFromDatabase();
}

void MatchScreen::onLeave()
{
    m_pipeline.stop();
}

// ════════════════════════════════════════════════════════════
// Input handling
// ════════════════════════════════════════════════════════════

bool MatchScreen::handleInput(struct notcurses *, const ncinput &ni, int ch)
{
    // Esc first-refusal: cancel running scan, or clear focus
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
        if (m_focus == Focus::PathInput)
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

    // ── File list mode ─────────────────────────────────────
    if (m_focus == Focus::FileList) {
        auto action = m_fileList.handleInput(ch);
        if (action == SelectableList::Action::ToggleCheck) {
            int sel = m_fileList.selected();
            std::lock_guard<std::mutex> lock(m_filesMutex);
            if (sel >= 0 && sel < static_cast<int>(m_files.size()))
                m_files[sel].checked = !m_files[sel].checked;
            return true;
        }
        if (action == SelectableList::Action::ToggleAll) {
            std::lock_guard<std::mutex> lock(m_filesMutex);
            bool allChecked = std::all_of(m_files.begin(), m_files.end(),
                                          [](const FileEntry &e) { return e.checked; });
            for (auto &f : m_files)
                f.checked = !allChecked;
            return true;
        }
        if (action != SelectableList::Action::None)
            return true;

        // 's' to trigger scan from file list
        if (ch == 's' || ch == 'S') {
            startScan();
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
    return false;
}

// ════════════════════════════════════════════════════════════
// Rendering
// ════════════════════════════════════════════════════════════

void MatchScreen::render(struct notcurses *nc)
{
    unsigned rows = m_app.rows();
    unsigned cols = m_app.cols();
    ncplane *std = notcurses_stdplane(nc);

    // Layout via SplitPane widget
    const int headerH = 3;
    const int footerH = 1;
    const int progressH = 2;
    auto layout = m_splitPane.compute(cols, rows, headerH, footerH, progressH);
    m_lastLayout = layout;

    drawHeader(std, cols);

    // File list via SelectableList widget
    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        m_fileList.setCount(static_cast<int>(m_files.size()));
    }
    m_fileList.ensureVisible(layout.bodyH / 2); // 2 rows per item

    bool listFocused = (m_focus == Focus::FileList);
    {
        // List header
        std::lock_guard<std::mutex> lock(m_filesMutex);
        char hdr[128];
        snprintf(hdr, sizeof(hdr), " Files (%d)", static_cast<int>(m_files.size()));
        m_fileList.renderHeader(std, layout.bodyY, 0, hdr, listFocused);
    }

    // Render file list using callback
    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        m_fileList.render(std, layout.bodyY + 1, layout.bodyH - 1, listFocused,
            [this, listFocused, listW = layout.leftW](ncplane *plane, int y, int idx,
                                                        bool selected, bool /*focused*/) {
                if (idx < 0 || idx >= static_cast<int>(m_files.size())) return;
                const auto &f = m_files[idx];

                // Row 1: checkbox + filename
                {
                    uint64_t ch = 0;
                    if (selected && listFocused) {
                        ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
                        ncchannels_set_bg_rgb8(&ch, 0x22, 0x44, 0x66);
                    } else if (selected) {
                        ncchannels_set_fg_rgb8(&ch, 0xDD, 0xDD, 0xDD);
                        ncchannels_set_bg_rgb8(&ch, 0x22, 0x22, 0x33);
                    } else {
                        ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
                    }
                    ncplane_set_channels(plane, ch);
                    if (selected)
                        ncplane_set_styles(plane, NCSTYLE_BOLD);

                    const char *check = f.checked ? "[x] " : "[ ] ";
                    ncplane_putstr_yx(plane, y, 1, check);

                    int maxNameW = listW - 6;
                    std::string fname = f.filename;
                    if (static_cast<int>(fname.size()) > maxNameW)
                        fname = fname.substr(0, static_cast<size_t>(maxNameW - 3)) + "...";
                    ncplane_putstr(plane, fname.c_str());
                    ncplane_set_styles(plane, NCSTYLE_NONE);
                    uint64_t rc = 0;
                    ncplane_set_channels(plane, rc);
                }

                // Row 2: system — hash — match status
                {
                    uint64_t ch = 0;
                    if (selected && listFocused) {
                        ncchannels_set_bg_rgb8(&ch, 0x22, 0x44, 0x66);
                    } else if (selected) {
                        ncchannels_set_bg_rgb8(&ch, 0x22, 0x22, 0x33);
                    }
                    ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
                    ncplane_set_channels(plane, ch);

                    char detail[256];
                    snprintf(detail, sizeof(detail), "    %s - %s - %s",
                             f.system.c_str(),
                             f.hash.empty() ? "no hash" : f.hash.substr(0, 8).c_str(),
                             f.matchStatus.c_str());
                    if (static_cast<int>(strlen(detail)) > listW - 1)
                        detail[listW - 1] = '\0';
                    ncplane_putstr_yx(plane, y + 1, 0, detail);

                    setConfidenceColor(plane, f.confidence);
                    std::string confStr = " " + confidenceIcon(f.confidence);
                    ncplane_putstr(plane, confStr.c_str());

                    uint64_t rc = 0;
                    ncplane_set_channels(plane, rc);
                }
            });

        if (m_files.empty()) {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
            ncplane_set_channels(std, ch);
            ncplane_putstr_yx(std, layout.bodyY + 2, 2, "No files. Enter a path and press Enter to scan.");
        }
    }

    // Detail pane
    drawDetailPane(std, layout.bodyY, layout.bodyH, layout.rightX, static_cast<unsigned>(layout.rightW));

    // Separator
    m_splitPane.renderSeparator(std, layout);

    // Progress bar via widget
    m_progressBar.render(std, layout.progressY, cols);

    drawFooter(std, rows, cols);

    ncplane_set_channels(std, 0);
    ncplane_set_styles(std, NCSTYLE_NONE);
}

void MatchScreen::onResize(struct notcurses *)
{
    // Dimensions auto-updated by App
}

// ════════════════════════════════════════════════════════════
// Render helpers
// ════════════════════════════════════════════════════════════

void MatchScreen::drawHeader(ncplane *plane, unsigned cols)
{
    // Row 0: Title
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0x00, 0x00);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        ncplane_putstr_yx(plane, 0, 2, "MATCH");
        ncplane_set_styles(plane, NCSTYLE_NONE);
    }

    // "REMUS" right-aligned
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
        ncplane_set_channels(plane, ch);
        const char *brand = "REMUS";
        ncplane_putstr_yx(plane, 0, static_cast<int>(cols) - 7, brand);
    }

    // Row 1: Path input via TextInput widget
    {
        int fieldWidth = static_cast<int>(cols) - 2 - 10; // leave room for SCAN button
        if (fieldWidth < 20) fieldWidth = 20;
        m_pathInput.render(plane, 1, 2, fieldWidth, m_focus == Focus::PathInput);
    }

    // SCAN button
    {
        bool scanning = m_pipelineRunning.load();
        uint64_t ch = 0;
        if (scanning) {
            ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
        } else {
            ncchannels_set_fg_rgb8(&ch, 0x00, 0xCC, 0x00);
        }
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        ncplane_putstr_yx(plane, 1, static_cast<int>(cols) - 8, scanning ? "RUNNING" : "[SCAN]");
        ncplane_set_styles(plane, NCSTYLE_NONE);
    }

    // Row 2: separator
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x44, 0x44, 0x44);
        ncplane_set_channels(plane, ch);
        std::string sep(cols, '-');
        ncplane_putstr_yx(plane, 2, 0, sep.c_str());
    }
}

void MatchScreen::drawDetailPane(ncplane *plane, int startY, int height, int startX, unsigned width)
{
    bool focused = (m_focus == Focus::DetailPane);

    // Header
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, focused ? 0xFF : 0x88, focused ? 0xFF : 0x88, focused ? 0xFF : 0x88);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        ncplane_putstr_yx(plane, startY, startX + 1, "Details");
        ncplane_set_styles(plane, NCSTYLE_NONE);
    }

    std::lock_guard<std::mutex> lock(m_filesMutex);
    int sel = m_fileList.selected();
    if (sel < 0 || sel >= static_cast<int>(m_files.size())) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, startY + 2, startX + 2, "Select a file to see details");
        return;
    }

    const auto &f = m_files[sel];
    int y = startY + 2;
    int maxW = static_cast<int>(width) - 3;

    auto putField = [&](const char *label, const std::string &value, uint8_t r, uint8_t g, uint8_t b) {
        if (y >= startY + height) return;
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, y, startX + 2, label);

        ch = 0;
        ncchannels_set_fg_rgb8(&ch, r, g, b);
        ncplane_set_channels(plane, ch);

        std::string val = value;
        int labelLen = static_cast<int>(strlen(label));
        int valMax = maxW - labelLen;
        if (valMax > 0 && static_cast<int>(val.size()) > valMax)
            val = val.substr(0, static_cast<size_t>(valMax - 3)) + "...";
        ncplane_putstr(plane, val.c_str());
        y++;
    };

    // Title — bold white
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        std::string title = f.title.empty() ? f.filename : f.title;
        if (static_cast<int>(title.size()) > maxW)
            title = title.substr(0, static_cast<size_t>(maxW - 3)) + "...";
        ncplane_putstr_yx(plane, y, startX + 2, title.c_str());
        ncplane_set_styles(plane, NCSTYLE_NONE);
        y++;
    }

    putField("System:    ", f.system, 0xAA, 0xAA, 0xFF);
    putField("Developer: ", f.developer.empty() ? "-" : f.developer, 0xCC, 0xCC, 0xCC);
    putField("Publisher: ", f.publisher.empty() ? "-" : f.publisher, 0xCC, 0xCC, 0xCC);
    putField("Region:    ", f.region.empty() ? "-" : f.region, 0xCC, 0xCC, 0xCC);
    putField("Match:     ", f.matchMethod.empty() ? "-" : f.matchMethod, 0xCC, 0xCC, 0xCC);

    // Confidence — color-coded
    if (y < startY + height) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, y, startX + 2, "Confidence:");

        setConfidenceColor(plane, f.confidence);
        char conf[32];
        snprintf(conf, sizeof(conf), " %d%% %s", f.confidence, confidenceIcon(f.confidence).c_str());
        ncplane_putstr(plane, conf);
        y++;
    }

    // Hash
    putField("CRC32:     ", f.hash.empty() ? "not calculated" : f.hash, 0x88, 0xCC, 0x88);

    y++;

    // Description (word-wrapped)
    if (!f.description.empty() && y < startY + height) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, y++, startX + 2, "Description:");

        ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xAA, 0xAA, 0xAA);
        ncplane_set_channels(plane, ch);

        // Simple word wrap
        std::string desc = f.description;
        int lineW = maxW - 1;
        if (lineW < 10) lineW = 10;
        size_t pos = 0;
        while (pos < desc.size() && y < startY + height) {
            std::string line = desc.substr(pos, static_cast<size_t>(lineW));
            // Try to break at word boundary
            if (pos + static_cast<size_t>(lineW) < desc.size()) {
                auto lastSpace = line.rfind(' ');
                if (lastSpace != std::string::npos && lastSpace > 0) {
                    line = line.substr(0, lastSpace);
                }
            }
            ncplane_putstr_yx(plane, y++, startX + 3, line.c_str());
            pos += line.size();
            if (pos < desc.size() && desc[pos] == ' ')
                pos++; // skip space at break
        }
    }
}

void MatchScreen::drawFooter(ncplane *plane, unsigned rows, unsigned cols)
{
    const char *hint = nullptr;
    switch (m_focus) {
    case Focus::PathInput:
        hint = "Enter:scan  Tab:next pane  Esc:back";
        break;
    case Focus::FileList:
        hint = "j/k:navigate  Space:toggle  s:scan  Tab:next pane  Esc:back";
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
    if (path.empty())
        return;

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
            if (msg.find("Pipeline done") != std::string::npos) {
                m_app.post([this]() {
                    m_progressBar.set(m_progressBar.total(), m_progressBar.total(), "done");
                    m_pipelineRunning = false;
                    loadFromDatabase();
                    m_app.toast("Scan complete", Toast::Level::Info);
                });
            }
        },
        &m_app.db()
    );
}

void MatchScreen::loadFromDatabase()
{
    auto &db = m_app.db();
    auto allFiles = db.getAllFiles();
    auto allMatches = db.getAllMatches();

    std::vector<FileEntry> entries;
    entries.reserve(static_cast<size_t>(allFiles.size()));

    for (const auto &fr : allFiles) {
        FileEntry e;
        e.fileId = fr.id;
        e.filename = fr.filename.toStdString();
        e.hash = fr.crc32.toStdString();
        e.system = db.getSystemDisplayName(fr.systemId).toStdString();
        if (e.system.empty()) e.system = "Unknown";

        // Look up match
        auto it = allMatches.find(fr.id);
        if (it != allMatches.end()) {
            e.confidence = static_cast<int>(it.value().confidence);
            e.matchMethod = it.value().matchMethod.toStdString();
            e.title = it.value().gameTitle.toStdString();
            e.developer = it.value().developer.toStdString();
            e.publisher = it.value().publisher.toStdString();
            e.description = it.value().description.toStdString();
            e.region = it.value().region.toStdString();

            if (e.confidence >= 90)
                e.matchStatus = "match ✓";
            else if (e.confidence > 0)
                e.matchStatus = "match ?";
            else
                e.matchStatus = "no match";
        } else {
            e.matchStatus = fr.hashCalculated ? "unmatched" : "not hashed";
        }

        entries.push_back(std::move(e));
    }

    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        m_files = std::move(entries);
    }

    m_fileList.setCount(static_cast<int>(m_files.size()));
    if (m_fileList.selected() < 0 && !m_files.empty())
        m_fileList.setSelected(0);
}

void MatchScreen::forceRefresh()
{
    loadFromDatabase();
}

// ════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════

std::string MatchScreen::confidenceIcon(int confidence)
{
    if (confidence >= 90) return "✓";
    if (confidence >= 60) return "~";
    if (confidence > 0)   return "?";
    return "-";
}

void MatchScreen::setConfidenceColor(ncplane *plane, int confidence)
{
    uint64_t ch = 0;
    if (confidence >= 90) {
        ncchannels_set_fg_rgb8(&ch, 0x00, 0xCC, 0x00); // green
    } else if (confidence >= 60) {
        ncchannels_set_fg_rgb8(&ch, 0xFF, 0xAA, 0x00); // orange
    } else if (confidence > 0) {
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0x00, 0x00); // red
    } else {
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66); // dim
    }
    ncplane_set_channels(plane, ch);
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
        {"s",       "Start scan (from file list)"},
        {"Esc",     "Cancel running scan / go back"},
    };
}
