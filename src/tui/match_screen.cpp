#include "match_screen.h"
#include "app.h"

#include "../core/database.h"
#include "../core/scanner.h"
#include "../core/hasher.h"
#include "../core/matching_engine.h"
#include "../core/system_detector.h"
#include "../metadata/local_database_provider.h"
#include "../metadata/provider_orchestrator.h"
#include "../metadata/hasheous_provider.h"
#include "../metadata/thegamesdb_provider.h"
#include "../metadata/igdb_provider.h"
#include "../core/constants/constants.h"

#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QSettings>
#include <algorithm>
#include <cctype>
#include <cstring>

using Remus::Database;
using Remus::FileRecord;
using Remus::LocalDatabaseProvider;
using Remus::SearchResult;

namespace {

QString baseNameForGrouping(const QString &filename)
{
    QFileInfo fi(filename);
    QString baseName = fi.completeBaseName();

    static QRegularExpression trackPattern(
        R"(\s*\(Track\s*\d+\)$)", QRegularExpression::CaseInsensitiveOption);
    baseName.remove(trackPattern);

    return baseName.trimmed();
}

QString groupKey(const FileRecord &file)
{
    QFileInfo info(file.originalPath);
    return info.path() + "/" + baseNameForGrouping(file.filename);
}

void sortExtensions(QStringList &exts)
{
    QMap<QString, int> priority = {
        {QStringLiteral(".cue"), 0}, {QStringLiteral(".gdi"), 1}, {QStringLiteral(".m3u"), 2},
        {QStringLiteral(".iso"), 3}, {QStringLiteral(".chd"), 4},
        {QStringLiteral(".bin"), 10}, {QStringLiteral(".img"), 11}, {QStringLiteral(".raw"), 12}
    };

    std::sort(exts.begin(), exts.end(), [&priority](const QString &a, const QString &b) {
        const int pa = priority.value(a.toLower(), 5);
        const int pb = priority.value(b.toLower(), 5);
        if (pa != pb) return pa < pb;
        return a < b;
    });
}

bool preferPrimaryCandidate(const FileRecord &candidate, const FileRecord &current,
                            const QMap<int, Database::MatchResult> &matches)
{
    const bool candidateHasMatch = matches.contains(candidate.id);
    const bool currentHasMatch = matches.contains(current.id);
    if (candidateHasMatch != currentHasMatch) {
        return candidateHasMatch;
    }

    auto isPrimaryExtension = [](const QString &ext) {
        const QString lower = ext.toLower();
        return lower == QStringLiteral(".cue") || lower == QStringLiteral(".gdi") || lower == QStringLiteral(".m3u");
    };

    if (candidate.isPrimary != current.isPrimary) {
        return candidate.isPrimary;
    }

    if (isPrimaryExtension(candidate.extension) && !isPrimaryExtension(current.extension)) {
        return true;
    }

    return false;
}

} // namespace

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

                // ── Section header row ──────────────────────────────
                if (f.isHeader) {
                    uint64_t ch = 0;
                    if (f.section == Section::Confident) {
                        ncchannels_set_fg_rgb8(&ch, 0x00, 0xCC, 0x00);
                    } else if (f.section == Section::Possible) {
                        ncchannels_set_fg_rgb8(&ch, 0xFF, 0xAA, 0x00);
                    } else {
                        ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
                    }
                    ncplane_set_channels(plane, ch);
                    ncplane_set_styles(plane, NCSTYLE_BOLD);
                    std::string hdr = f.filename;
                    if (static_cast<int>(hdr.size()) > listW - 1)
                        hdr = hdr.substr(0, static_cast<size_t>(listW - 1));
                    ncplane_putstr_yx(plane, y, 0, hdr.c_str());
                    ncplane_set_styles(plane, NCSTYLE_NONE);
                    uint64_t rc = 0;
                    ncplane_set_channels(plane, rc);
                    return;
                }

                // Row 1: confirmation prefix + checkbox + filename
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

                    // Confirmation status badge (✓ / ✗ / ·)
                    {
                        uint64_t badge = 0;
                        if (selected && listFocused)
                            ncchannels_set_bg_rgb8(&badge, 0x22, 0x44, 0x66);
                        else if (selected)
                            ncchannels_set_bg_rgb8(&badge, 0x22, 0x22, 0x33);
                        if (f.confirmStatus == ConfirmStatus::Confirmed)
                            ncchannels_set_fg_rgb8(&badge, 0x00, 0xCC, 0x00);
                        else if (f.confirmStatus == ConfirmStatus::Rejected)
                            ncchannels_set_fg_rgb8(&badge, 0xCC, 0x00, 0x00);
                        else
                            ncchannels_set_fg_rgb8(&badge, 0x55, 0x55, 0x55);
                        ncplane_set_channels(plane, badge);
                        const char *badge_str =
                            f.confirmStatus == ConfirmStatus::Confirmed ? "✓ " :
                            f.confirmStatus == ConfirmStatus::Rejected  ? "✗ " : "· ";
                        ncplane_putstr_yx(plane, y, 1, badge_str);
                    }

                    // Reset row colour
                    ncplane_set_channels(plane, ch);

                    const char *check = f.checked ? "[x] " : "[ ] ";
                    ncplane_putstr(plane, check);

                    int maxNameW = listW - 10; // badge(2) + check(4) + margin(4)
                    std::string fname = f.filename;
                    if (f.isPossiblyPatched) fname = "[P] " + fname;
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

    // Manual match overlay (drawn last, on top of everything)
    if (m_manualOverlay.isActive())
        drawManualMatchOverlay(std, rows, cols);

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
        bool focused = (m_focus == Focus::ScanButton);
        uint64_t ch = 0;
        if (scanning) {
            ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
        } else {
            ncchannels_set_fg_rgb8(&ch, 0x00, 0xCC, 0x00);
        }
        if (focused)
            ncchannels_set_bg_rgb8(&ch, 0x22, 0x44, 0x66);
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
    if (f.isHeader) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, startY + 2, startX + 2, "Navigate to a file entry");
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
    putField("Files:     ", f.extensions.empty() ? "-" : f.extensions, 0xCC, 0xCC, 0xCC);
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

void MatchScreen::loadFromDatabase()
{
    auto &db = m_app.db();
    auto allFiles = db.getExistingFiles();
    auto allMatches = db.getAllMatches();

    struct Group {
        FileRecord primary;
        bool hasPrimary = false;
        QStringList extensions;
    };

    QMap<QString, Group> groups;

    for (const auto &fr : allFiles) {
        const QString key = groupKey(fr);
        Group &g = groups[key];

        const QString ext = fr.extension.toLower();
        if (!g.extensions.contains(ext)) {
            g.extensions.append(ext);
        }

        if (!g.hasPrimary || preferPrimaryCandidate(fr, g.primary, allMatches)) {
            g.primary = fr;
            g.hasPrimary = true;
        }
    }

    // Build flat entry list (without headers yet)
    std::vector<FileEntry> confident, possible, noMatch;

    for (auto it = groups.begin(); it != groups.end(); ++it) {
        Group &g = it.value();
        sortExtensions(g.extensions);

        const FileRecord &fr = g.primary;
        FileEntry e;
        e.fileId = fr.id;
        const std::string baseName = baseNameForGrouping(fr.filename).toStdString();
        const std::string extDisplay = g.extensions.join(" ").toStdString();
        if (g.extensions.size() > 1) {
            e.filename = baseName + " [" + extDisplay + "]";
        } else {
            e.filename = fr.filename.toStdString();
        }
        e.extensions = extDisplay;
        e.hash = fr.crc32.toStdString();
        e.system = db.getSystemDisplayName(fr.systemId).toStdString();
        if (e.system.empty()) e.system = "Unknown";

        auto matchIt = allMatches.find(fr.id);
        if (matchIt != allMatches.end()) {
            const auto &mr = matchIt.value();
            e.confidence = static_cast<int>(mr.confidence);
            e.matchMethod = mr.matchMethod.toStdString();
            e.title = mr.gameTitle.toStdString();
            e.developer = mr.developer.toStdString();
            e.publisher = mr.publisher.toStdString();
            e.description = mr.description.toStdString();
            e.region = mr.region.toStdString();
            e.confirmStatus = mr.isConfirmed ? ConfirmStatus::Confirmed
                            : mr.isRejected  ? ConfirmStatus::Rejected
                            :                  ConfirmStatus::Pending;

            if (e.confidence >= 90) {
                e.section = Section::Confident;
                e.matchStatus = "match ✓";
                confident.push_back(std::move(e));
            } else if (e.confidence > 0) {
                e.section = Section::Possible;
                e.matchStatus = "match ?";
                possible.push_back(std::move(e));
            } else {
                e.section = Section::NoMatch;
                e.matchStatus = "no match";
                e.isPossiblyPatched = looksPatched(e.filename);
                noMatch.push_back(std::move(e));
            }
        } else {
            e.section = Section::NoMatch;
            e.matchStatus = fr.hashCalculated ? "unmatched" : "not hashed";
            e.isPossiblyPatched = looksPatched(e.filename);
            noMatch.push_back(std::move(e));
        }
    }

    // Sort each bucket alphabetically by filename
    auto byName = [](const FileEntry &a, const FileEntry &b) { return a.filename < b.filename; };
    std::sort(confident.begin(), confident.end(), byName);
    std::sort(possible.begin(), possible.end(), byName);
    std::sort(noMatch.begin(), noMatch.end(), byName);

    // Assemble with section-header rows
    std::vector<FileEntry> entries;
    auto addSection = [&](const std::vector<FileEntry> &bucket, const char *label, Section sec) {
        if (bucket.empty()) return;
        FileEntry hdr;
        hdr.isHeader = true;
        hdr.section = sec;
        hdr.filename = label;
        entries.push_back(hdr);
        for (const auto &e : bucket)
            entries.push_back(e);
    };

    addSection(confident, "── Confident Match (≥90%) ────────────────────────────", Section::Confident);
    addSection(possible,  "── Possible Match ────────────────────────────────────", Section::Possible);
    addSection(noMatch,   "── No Match ──────────────────────────────────────────", Section::NoMatch);

    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        m_files = std::move(entries);
    }

    m_fileList.setCount(static_cast<int>(m_files.size()));
    // Position on first non-header entry
    if (!m_files.empty()) {
        for (int i = 0; i < static_cast<int>(m_files.size()); ++i) {
            if (!m_files[static_cast<size_t>(i)].isHeader) {
                m_fileList.setSelected(i);
                break;
            }
        }
    }

    // Default focus based on content
    m_focus = m_files.empty() ? Focus::PathInput : Focus::FileList;
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
// Metadata enrichment
// ════════════════════════════════════════════════════════════

Remus::ProviderOrchestrator &MatchScreen::getOrchestrator()
{
    if (!m_orchestrator) {
        using namespace Remus;
        m_orchestrator = std::make_unique<ProviderOrchestrator>();

        // Hasheous — free, no auth
        auto *hasheous = new HasheousProvider();
        const auto hInfo = Constants::Providers::getProviderInfo(Constants::Providers::HASHEOUS);
        m_orchestrator->addProvider(Constants::Providers::HASHEOUS, hasheous, hInfo->priority);

        // TheGamesDB — free, optional API key
        QSettings settings;
        auto *tgdb = new TheGamesDBProvider();
        QString tgdbKey = settings.value(Constants::Settings::Providers::THEGAMESDB_API_KEY).toString();
        if (!tgdbKey.isEmpty()) tgdb->setApiKey(tgdbKey);
        const auto tInfo = Constants::Providers::getProviderInfo(Constants::Providers::THEGAMESDB);
        m_orchestrator->addProvider(Constants::Providers::THEGAMESDB, tgdb, tInfo->priority);

        // IGDB — requires Twitch credentials
        auto *igdb = new IGDBProvider();
        QString igdbId  = settings.value(Constants::Settings::Providers::IGDB_CLIENT_ID).toString();
        QString igdbSec = settings.value(Constants::Settings::Providers::IGDB_CLIENT_SECRET).toString();
        if (!igdbId.isEmpty() && !igdbSec.isEmpty()) {
            igdb->setCredentials(igdbId, igdbSec);
            const auto iInfo = Constants::Providers::getProviderInfo(Constants::Providers::IGDB);
            m_orchestrator->addProvider(Constants::Providers::IGDB, igdb, iInfo->priority);
        } else {
            delete igdb;
        }
    }
    return *m_orchestrator;
}

void MatchScreen::enrichSelectedMetadata()
{
    if (m_enrichTask.running()) {
        m_app.toast("Enrichment already in progress", Toast::Level::Warning);
        return;
    }

    int sel = m_fileList.selected();
    int targetFileId = 0;
    std::string targetTitle;
    std::string targetSystem;
    std::string targetHash;
    int gameId = 0;
    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        if (sel < 0 || sel >= static_cast<int>(m_files.size())) return;
        const FileEntry &e = m_files[static_cast<size_t>(sel)];
        if (e.isHeader || e.fileId == 0) return;
        if (e.section == Section::NoMatch) {
            m_app.toast("No match to enrich — confirm or manual-match first", Toast::Level::Warning);
            return;
        }
        targetFileId = e.fileId;
        targetTitle  = e.title;
        targetSystem = e.system;
        targetHash   = e.hash;
    }

    // Look up gameId from DB
    auto mr = m_app.db().getMatchForFile(targetFileId);
    if (mr.gameId == 0) {
        m_app.toast("No game match found in database", Toast::Level::Warning);
        return;
    }
    gameId = mr.gameId;

    m_app.toast("Enriching metadata…", Toast::Level::Info, 1500);

    m_enrichTask.start([this, targetFileId, targetTitle, targetSystem, targetHash, gameId]() {
        auto &orch = getOrchestrator();
        auto meta = orch.searchWithFallback(
            QString::fromStdString(targetHash),
            QString::fromStdString(targetTitle),
            QString::fromStdString(targetSystem));

        if (meta.title.isEmpty()) {
            m_app.post([this]() {
                m_app.toast("No additional metadata found", Toast::Level::Warning);
            });
            return;
        }

        // Update DB on main thread
        m_app.post([this, meta, gameId, targetFileId]() {
            m_app.db().updateGame(
                gameId,
                meta.publisher,
                meta.developer,
                meta.releaseDate,
                meta.description,
                meta.genres.join(", "),
                meta.players > 0 ? QString::number(meta.players) : QString(),
                meta.rating);

            // Refresh display entry with new metadata
            {
                std::lock_guard<std::mutex> lock(m_filesMutex);
                for (auto &f : m_files) {
                    if (f.fileId == targetFileId) {
                        if (!meta.title.isEmpty())
                            f.title = meta.title.toStdString();
                        if (!meta.developer.isEmpty())
                            f.developer = meta.developer.toStdString();
                        if (!meta.publisher.isEmpty())
                            f.publisher = meta.publisher.toStdString();
                        if (!meta.description.isEmpty())
                            f.description = meta.description.toStdString();
                        if (!meta.region.isEmpty())
                            f.region = meta.region.toStdString();
                        break;
                    }
                }
            }

            m_app.toast("Metadata enriched from " + meta.providerId.toStdString(),
                        Toast::Level::Success);
        });
    });
}

/*static*/ bool MatchScreen::looksPatched(const std::string &filename)
{
    // Keyword heuristics for patched/translated/hacked ROMs
    static const std::vector<std::string> keywords = {
        "(patch", "(patched", "(translated", "(translation",
        "(hack", "(hacked", "(mod", "(modified",
        "[t]", "[h]", "[t-", "(t)"
    };
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    for (const auto &kw : keywords) {
        if (lower.find(kw) != std::string::npos)
            return true;
    }
    return false;
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
