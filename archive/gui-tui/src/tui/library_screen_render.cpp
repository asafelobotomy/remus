#include "library_screen.h"
#include "app.h"

#include "../core/constants/confidence.h"

#include <cstring>

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
    putField("Files:     ", e.extensions.empty() ? "-" : e.extensions, 0xCC, 0xCC, 0xCC);
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

void LibraryScreen::onResize(struct notcurses *)
{
}

// ════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════

std::string LibraryScreen::confidenceIcon(int confidence)
{
    if (confidence >= static_cast<int>(Remus::Constants::Confidence::Thresholds::HIGH)) return "✓";
    if (confidence >= static_cast<int>(Remus::Constants::Confidence::Thresholds::MEDIUM)) return "~";
    if (confidence > 0)   return "?";
    return "-";
}

void LibraryScreen::setConfidenceColor(ncplane *plane, int confidence)
{
    uint64_t ch = 0;
    if (confidence >= static_cast<int>(Remus::Constants::Confidence::Thresholds::HIGH))
        ncchannels_set_fg_rgb8(&ch, 0x00, 0xCC, 0x00);
    else if (confidence >= static_cast<int>(Remus::Constants::Confidence::Thresholds::MEDIUM))
        ncchannels_set_fg_rgb8(&ch, 0xFF, 0xAA, 0x00);
    else if (confidence > 0)
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0x00, 0x00);
    else
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
    ncplane_set_channels(plane, ch);
}
