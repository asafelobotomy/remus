#include "library_screen.h"
#include "app.h"

#include "../core/database.h"
#include "../services/match_service.h"

#include <QFileInfo>
#include <cstring>
#include <algorithm>
#include <map>
#include <set>

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

void LibraryScreen::onResize(struct notcurses *)
{
}

// ════════════════════════════════════════════════════════════
// Rendering
// ════════════════════════════════════════════════════════════

void LibraryScreen::render(struct notcurses *nc)
{
    unsigned rows = m_app.rows();
    unsigned cols = m_app.cols();
    ncplane *std = notcurses_stdplane(nc);

    int headerH = 3;
    int footerH = 1;
    int progressH = 0;
    auto layout = m_splitPane.compute(cols, rows, headerH, footerH, progressH);
    m_lastLayout = layout;

    drawHeader(std, cols);

    // ── File list (left pane) ──────────────────────────────
    {
        bool focused = (m_focus == Focus::FileList);
        int count;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            count = static_cast<int>(m_entries.size());
        }
        m_fileList.setCount(count);
        m_fileList.ensureVisible(layout.bodyH);

        if (count == 0) {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
            ncplane_set_channels(std, ch);
            ncplane_putstr_yx(std, layout.bodyY + 2, 2, "Library is empty. Run Match first.");
        } else {
            std::lock_guard<std::mutex> lock(m_mutex);
            int w = layout.leftW;
            m_fileList.render(std, layout.bodyY, layout.bodyH, focused,
                              [&](ncplane *plane, int y, int idx, bool sel, bool foc) {
                const auto &e = m_entries[idx];
                if (e.isHeader) {
                    uint64_t ch = 0;
                    ncchannels_set_fg_rgb8(&ch, 0xAA, 0xAA, 0xFF);
                    ncplane_set_channels(plane, ch);
                    ncplane_set_styles(plane, NCSTYLE_BOLD);
                    std::string hdr = "▸ " + e.system;
                    if (static_cast<int>(hdr.size()) > w - 1)
                        hdr = hdr.substr(0, static_cast<size_t>(w - 1));
                    ncplane_putstr_yx(plane, y, 1, hdr.c_str());
                    ncplane_set_styles(plane, NCSTYLE_NONE);
                } else {
                    uint64_t ch = 0;
                    if (sel && foc) {
                        ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
                        ncchannels_set_bg_rgb8(&ch, 0x22, 0x44, 0x66);
                    } else if (sel) {
                        ncchannels_set_fg_rgb8(&ch, 0xDD, 0xDD, 0xDD);
                        ncchannels_set_bg_rgb8(&ch, 0x22, 0x22, 0x33);
                    } else {
                        ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
                    }
                    ncplane_set_channels(plane, ch);
                    if (sel) ncplane_set_styles(plane, NCSTYLE_BOLD);

                    // Confirmation status prefix: ✓ (green) / ✗ (red) / ? (orange)
                    {
                        uint64_t pfxCh = 0;
                        if (sel && foc)
                            ncchannels_set_bg_rgb8(&pfxCh, 0x22, 0x44, 0x66);
                        else if (sel)
                            ncchannels_set_bg_rgb8(&pfxCh, 0x22, 0x22, 0x33);
                        if (e.confirmStatus == ConfirmationStatus::Confirmed)
                            ncchannels_set_fg_rgb8(&pfxCh, 0x44, 0xCC, 0x44);
                        else if (e.confirmStatus == ConfirmationStatus::Rejected)
                            ncchannels_set_fg_rgb8(&pfxCh, 0xCC, 0x44, 0x44);
                        else
                            ncchannels_set_fg_rgb8(&pfxCh, 0xCC, 0x99, 0x33);
                        ncplane_set_channels(plane, pfxCh);
                        const char *pfx = (e.confirmStatus == ConfirmationStatus::Confirmed) ? "✓ "
                                        : (e.confirmStatus == ConfirmationStatus::Rejected) ? "✗ "
                                        : "? ";
                        ncplane_putstr_yx(plane, y, 3, pfx);
                    }
                    // Restore row colors for filename 
                    ncplane_set_channels(plane, ch);
                    if (sel) ncplane_set_styles(plane, NCSTYLE_BOLD);

                    int maxNameW = w - 7 - 12;  // 7 = indent(3) + prefix(2) + pad(2)
                    std::string fname = e.filename;
                    if (static_cast<int>(fname.size()) > maxNameW)
                        fname = fname.substr(0, static_cast<size_t>(maxNameW - 3)) + "...";
                    ncplane_putstr(plane, fname.c_str());

                    setConfidenceColor(plane, e.confidence);
                    std::string status = " " + e.matchStatus + " " + confidenceIcon(e.confidence);
                    int statusX = w - static_cast<int>(status.size()) - 1;
                    if (statusX > static_cast<int>(fname.size()) + 5)
                        ncplane_putstr_yx(plane, y, statusX, status.c_str());

                    ncplane_set_styles(plane, NCSTYLE_NONE);
                    uint64_t reset = 0;
                    ncplane_set_channels(plane, reset);
                }
            });
        }
    }

    // ── Separator ──────────────────────────────────────────
    m_splitPane.renderSeparator(std, layout);

    // ── Detail pane (right pane) ──────────────────────────
    drawDetailPane(std, layout.bodyY, layout.bodyH, layout.rightX, layout.rightW);

    drawFooter(std, rows, cols);
}

void LibraryScreen::drawHeader(ncplane *plane, unsigned cols)
{
    // Row 0: Title + filter
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xAA, 0xAA, 0xFF);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        ncplane_putstr_yx(plane, 0, 2, "LIBRARY");
        ncplane_set_styles(plane, NCSTYLE_NONE);
    }

    // Filter input (TextInput widget)
    m_filterInput.render(plane, 0, 12, static_cast<int>(cols) / 3,
                         m_focus == Focus::FilterInput);

    // "REMUS" right-aligned
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, 0, static_cast<int>(cols) - 7, "REMUS");
    }

    // Row 1: Stats
    {
        char stats[128];
        snprintf(stats, sizeof(stats), " %d files, %d systems, %d matched",
                 m_totalFiles, m_totalSystems, m_totalMatched);
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, 1, 2, stats);
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

void LibraryScreen::drawDetailPane(ncplane *plane, int startY, int height, int startX, unsigned width)
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

    std::lock_guard<std::mutex> lock(m_mutex);
    int sel = m_fileList.selected();
    if (sel < 0 || sel >= static_cast<int>(m_entries.size())) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, startY + 2, startX + 2, "Select a file to see details");
        return;
    }

    const auto &e = m_entries[sel];

    // If it's a header row, show system info
    if (e.isHeader) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xAA, 0xAA, 0xFF);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        ncplane_putstr_yx(plane, startY + 2, startX + 2, e.system.c_str());
        ncplane_set_styles(plane, NCSTYLE_NONE);
        return;
    }

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

    // Title
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        std::string title = e.title.empty() ? e.filename : e.title;
        if (static_cast<int>(title.size()) > maxW)
            title = title.substr(0, static_cast<size_t>(maxW - 3)) + "...";
        ncplane_putstr_yx(plane, y, startX + 2, title.c_str());
        ncplane_set_styles(plane, NCSTYLE_NONE);
        y++;
    }

    putField("System:    ", e.system, 0xAA, 0xAA, 0xFF);
    putField("Developer: ", e.developer.empty() ? "-" : e.developer, 0xCC, 0xCC, 0xCC);
    putField("Publisher: ", e.publisher.empty() ? "-" : e.publisher, 0xCC, 0xCC, 0xCC);
    putField("Region:    ", e.region.empty() ? "-" : e.region, 0xCC, 0xCC, 0xCC);
    putField("Match:     ", e.matchMethod.empty() ? "-" : e.matchMethod, 0xCC, 0xCC, 0xCC);

    // Confidence
    if (y < startY + height) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, y, startX + 2, "Confidence:");

        setConfidenceColor(plane, e.confidence);
        char conf[32];
        snprintf(conf, sizeof(conf), " %d%% %s", e.confidence, confidenceIcon(e.confidence).c_str());
        ncplane_putstr(plane, conf);
        y++;
    }

    putField("Hash:      ", e.hash.empty() ? "-" : e.hash, 0x88, 0xCC, 0x88);
    putField("Path:      ", e.path, 0x88, 0x88, 0x88);

    y++;

    // Description
    if (!e.description.empty() && y < startY + height) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, y++, startX + 2, "Description:");

        ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xAA, 0xAA, 0xAA);
        ncplane_set_channels(plane, ch);

        std::string desc = e.description;
        int lineW = maxW - 1;
        if (lineW < 10) lineW = 10;
        size_t pos = 0;
        while (pos < desc.size() && y < startY + height) {
            std::string line = desc.substr(pos, static_cast<size_t>(lineW));
            if (pos + static_cast<size_t>(lineW) < desc.size()) {
                auto lastSpace = line.rfind(' ');
                if (lastSpace != std::string::npos && lastSpace > 0)
                    line = line.substr(0, lastSpace);
            }
            ncplane_putstr_yx(plane, y++, startX + 3, line.c_str());
            pos += line.size();
            if (pos < desc.size() && desc[pos] == ' ') pos++;
        }
    }
}

void LibraryScreen::drawFooter(ncplane *plane, unsigned rows, unsigned cols)
{
    const char *hint = nullptr;
    switch (m_focus) {
    case Focus::FilterInput:
        hint = "Type to filter  Enter:apply  Tab:next  Esc:back";
        break;
    case Focus::FileList:
        hint = "j/k:nav  f:filter  r:refresh  c:confirm  x:reject  Esc:back";
        break;
    case Focus::DetailPane:
        hint = "j/k:nav  c:confirm  x:reject  Tab:next  Esc:back";
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

void LibraryScreen::loadFromDatabase()
{
    auto &db = m_app.db();
    auto allFiles = db.getExistingFiles();
    auto allMatches = db.getAllMatches();

    // Build flat list of all entries (no filtering)
    std::map<std::string, std::vector<FileEntry>> bySystem;

    for (const auto &fr : allFiles) {
        std::string system = db.getSystemDisplayName(fr.systemId).toStdString();
        if (system.empty()) system = "Unknown";

        FileEntry e;
        e.fileId = fr.id;
        e.filename = fr.filename.toStdString();
        e.hash = fr.crc32.toStdString();
        e.system = system;
        e.path = fr.currentPath.toStdString();

        auto it = allMatches.find(fr.id);
        if (it != allMatches.end()) {
            e.confidence = static_cast<int>(it.value().confidence);
            e.matchMethod = it.value().matchMethod.toStdString();
            e.title = it.value().gameTitle.toStdString();
            e.developer = it.value().developer.toStdString();
            e.publisher = it.value().publisher.toStdString();
            e.description = it.value().description.toStdString();
            e.region = it.value().region.toStdString();

            // Confirmation status from database
            if (it.value().isConfirmed)
                e.confirmStatus = ConfirmationStatus::Confirmed;
            else if (it.value().isRejected)
                e.confirmStatus = ConfirmationStatus::Rejected;
            else
                e.confirmStatus = ConfirmationStatus::Pending;

            if (e.confidence >= 90) e.matchStatus = "match ✓";
            else if (e.confidence > 0) e.matchStatus = "match ?";
            else e.matchStatus = "no match";
        } else {
            e.matchStatus = fr.hashCalculated ? "unmatched" : "pending";
        }

        bySystem[system].push_back(std::move(e));
    }

    // Build flat list with headers
    std::vector<FileEntry> flat;
    for (const auto &[system, files] : bySystem) {
        FileEntry header;
        header.isHeader = true;
        header.system = system + " (" + std::to_string(files.size()) + ")";
        flat.push_back(std::move(header));

        for (const auto &f : files)
            flat.push_back(f);
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_allEntries = std::move(flat);
    }

    applyFilter();
}

void LibraryScreen::applyFilter()
{
    std::string filterLower = m_filterInput.value();
    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

    std::vector<FileEntry> filtered;
    std::set<std::string> systemSet;
    int totalFiles = 0;
    int totalMatched = 0;
    std::string currentSystem;
    std::vector<FileEntry> currentGroup;

    auto flushGroup = [&]() {
        if (!currentGroup.empty()) {
            FileEntry hdr;
            hdr.isHeader = true;
            hdr.system = currentSystem + " (" + std::to_string(currentGroup.size()) + ")";
            filtered.push_back(std::move(hdr));
            for (auto &f : currentGroup)
                filtered.push_back(std::move(f));
            currentGroup.clear();
        }
    };

    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto &e : m_allEntries) {
        if (e.isHeader) {
            flushGroup();
            // Extract raw system name (strip count suffix)
            currentSystem = e.system;
            auto paren = currentSystem.rfind(" (");
            if (paren != std::string::npos)
                currentSystem = currentSystem.substr(0, paren);
            continue;
        }

        totalFiles++;
        if (e.confidence > 0) totalMatched++;

        if (!filterLower.empty()) {
            std::string sysLower = currentSystem;
            std::transform(sysLower.begin(), sysLower.end(), sysLower.begin(), ::tolower);
            std::string fnameLower = e.filename;
            std::transform(fnameLower.begin(), fnameLower.end(), fnameLower.begin(), ::tolower);
            std::string titleLower = e.title;
            std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::tolower);
            if (sysLower.find(filterLower) == std::string::npos &&
                fnameLower.find(filterLower) == std::string::npos &&
                titleLower.find(filterLower) == std::string::npos)
                continue;
        }

        systemSet.insert(currentSystem);
        currentGroup.push_back(e);
    }
    flushGroup();

    m_entries = std::move(filtered);
    m_totalFiles = totalFiles;
    m_totalSystems = static_cast<int>(systemSet.size());
    m_totalMatched = totalMatched;
    m_fileList.setCount(static_cast<int>(m_entries.size()));
    if (!m_entries.empty() && m_fileList.selected() < 0)
        m_fileList.setSelected(0);
}

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
    m_app.toast("Match confirmed", Toast::Level::Info, 1500);
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

void LibraryScreen::forceRefresh()
{
    loadFromDatabase();
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

// ════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════

std::string LibraryScreen::confidenceIcon(int confidence)
{
    if (confidence >= 90) return "✓";
    if (confidence >= 60) return "~";
    if (confidence > 0)   return "?";
    return "-";
}

void LibraryScreen::setConfidenceColor(ncplane *plane, int confidence)
{
    uint64_t ch = 0;
    if (confidence >= 90)
        ncchannels_set_fg_rgb8(&ch, 0x00, 0xCC, 0x00);
    else if (confidence >= 60)
        ncchannels_set_fg_rgb8(&ch, 0xFF, 0xAA, 0x00);
    else if (confidence > 0)
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0x00, 0x00);
    else
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
    ncplane_set_channels(plane, ch);
}
