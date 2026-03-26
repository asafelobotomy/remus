#include "match_screen.h"
#include "app.h"
#include "../core/constants/constants.h"

#include <cstring>

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

// ════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════

std::string MatchScreen::confidenceIcon(int confidence)
{
    if (confidence >= static_cast<int>(Remus::Constants::Confidence::Thresholds::HIGH)) return "✓";
    if (confidence >= static_cast<int>(Remus::Constants::Confidence::Thresholds::MEDIUM)) return "~";
    if (confidence > 0)   return "?";
    return "-";
}

void MatchScreen::setConfidenceColor(ncplane *plane, int confidence)
{
    uint64_t ch = 0;
    if (confidence >= static_cast<int>(Remus::Constants::Confidence::Thresholds::HIGH)) {
        ncchannels_set_fg_rgb8(&ch, 0x00, 0xCC, 0x00); // green
    } else if (confidence >= static_cast<int>(Remus::Constants::Confidence::Thresholds::MEDIUM)) {
        ncchannels_set_fg_rgb8(&ch, 0xFF, 0xAA, 0x00); // orange
    } else if (confidence > 0) {
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0x00, 0x00); // red
    } else {
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66); // dim
    }
    ncplane_set_channels(plane, ch);
}
