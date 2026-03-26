#include "patch_screen.h"
#include "app.h"
#include "tool_hints.h"

#include "../services/patch_service.h"

#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QFile>
#include <cstring>
#include <algorithm>

// ════════════════════════════════════════════════════════════
// Construction / Lifecycle
// ════════════════════════════════════════════════════════════

PatchScreen::PatchScreen(TuiApp &app)
    : Screen(app)
{
    m_patchService = new Remus::PatchService();
    m_patchList.setCheckboxes(true);
    m_patchList.setRowsPerItem(2);
}

PatchScreen::~PatchScreen()
{
    m_task.stop();
    delete m_patchService;
}

void PatchScreen::onEnter()
{
}

void PatchScreen::onLeave()
{
    m_task.stop();
}

// ════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════

std::string PatchScreen::formatSize(int64_t bytes)
{
    if (bytes < 0) return "?";
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
        return buf;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024));
    return buf;
}

// ════════════════════════════════════════════════════════════
// Input handling
// ════════════════════════════════════════════════════════════

bool PatchScreen::handleInput(struct notcurses *, const ncinput &ni, int ch)
{
    // Esc first-refusal: cancel running task, else go back
    if (ch == 27) { // Esc
        if (m_task.running()) {
            m_task.cancel();
            m_app.toast("Patching cancelled", Toast::Level::Warning, 2000);
            return true;
        }
        m_app.popScreen();
        return true;
    }

    // Mouse click-to-select
    if (ch == NCKEY_BUTTON1 && ni.evtype == NCTYPE_PRESS) {
        int row = ni.y, col = ni.x;
        unsigned cols = m_app.cols();
        int fieldW = static_cast<int>(cols) - 4;
        if (m_romInput.hitTest(row, col, 1, 2, fieldW)) {
            m_focus = Focus::RomInput;
        } else if (m_patchInput.hitTest(row, col, 2, 2, fieldW)) {
            m_focus = Focus::PatchInput;
        } else if (row >= m_lastLayout.bodyY && row < m_lastLayout.bodyY + m_lastLayout.bodyH) {
            if (col < m_lastLayout.leftW) {
                m_focus = Focus::PatchList;
                m_patchList.handleClick(row, m_lastLayout.bodyY, m_lastLayout.bodyH);
            } else {
                m_focus = Focus::DetailPane;
            }
        }
        return true;
    }

    // Tab cycles focus
    if (ch == '\t') {
        if (m_focus == Focus::RomInput)
            m_focus = Focus::PatchInput;
        else if (m_focus == Focus::PatchInput)
            m_focus = Focus::PatchList;
        else if (m_focus == Focus::PatchList)
            m_focus = Focus::DetailPane;
        else
            m_focus = Focus::RomInput;
        return true;
    }

    // 'b' toggle backup
    if (ch == 'b' && !m_task.running()) {
        m_createBackup = !m_createBackup;
        return true;
    }

    // ── ROM input ──────────────────────────────────────────
    if (m_focus == Focus::RomInput) {
        if (TextInput::isSubmit(ch)) {
            m_focus = Focus::PatchInput;
            return true;
        }
        return m_romInput.handleInput(ch);
    }

    // ── Patch input ────────────────────────────────────────
    if (m_focus == Focus::PatchInput) {
        if (TextInput::isSubmit(ch)) {
            scanPatches();
            return true;
        }
        return m_patchInput.handleInput(ch);
    }

    // ── Patch list ─────────────────────────────────────────
    if (m_focus == Focus::PatchList) {
        auto action = m_patchList.handleInput(ch);
        if (action == SelectableList::Action::ToggleCheck) {
            int sel = m_patchList.selected();
            std::lock_guard<std::mutex> lock(m_patchesMutex);
            if (sel >= 0 && sel < static_cast<int>(m_patches.size()))
                m_patches[sel].checked = !m_patches[sel].checked;
            return true;
        }
        if (action == SelectableList::Action::ToggleAll) {
            std::lock_guard<std::mutex> lock(m_patchesMutex);
            bool allChecked = std::all_of(m_patches.begin(), m_patches.end(),
                                          [](const PatchEntry &p){ return p.checked; });
            for (auto &p : m_patches) p.checked = !allChecked;
            return true;
        }
        if (action != SelectableList::Action::None) return true;

        if ((ch == 's' || ch == 'S') && !m_task.running()) {
            startPatching();
            return true;
        }
        return false;
    }

    // ── Detail pane ────────────────────────────────────────
    if (m_focus == Focus::DetailPane) {
        auto action = m_patchList.handleInput(ch);
        return action != SelectableList::Action::None;
    }

    return false;
}

// ════════════════════════════════════════════════════════════
// Tick
// ════════════════════════════════════════════════════════════

bool PatchScreen::tick()
{
    return m_task.running();
}

// ════════════════════════════════════════════════════════════
// Rendering
// ════════════════════════════════════════════════════════════

void PatchScreen::render(struct notcurses *nc)
{
    unsigned rows = m_app.rows();
    unsigned cols = m_app.cols();
    ncplane *std = notcurses_stdplane(nc);

    int headerH = 4;
    int footerH = 1;
    int progressH = 2;
    auto layout = m_splitPane.compute(cols, rows, headerH, footerH, progressH);
    m_lastLayout = layout;

    drawHeader(std, cols);

    // ── Patch list (left pane) ─────────────────────────────
    {
        bool focused = (m_focus == Focus::PatchList);
        int count;
        {
            std::lock_guard<std::mutex> lock(m_patchesMutex);
            count = static_cast<int>(m_patches.size());
        }
        m_patchList.setCount(count);
        m_patchList.ensureVisible(layout.bodyH);

        // Header line
        {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, focused ? 0xFF : 0x88, focused ? 0xFF : 0x88, focused ? 0xFF : 0x88);
            ncplane_set_channels(std, ch);
            ncplane_set_styles(std, NCSTYLE_BOLD);
            char hdr[128];
            snprintf(hdr, sizeof(hdr), " Patches (%d)", count);
            ncplane_putstr_yx(std, layout.bodyY, 0, hdr);
            ncplane_set_styles(std, NCSTYLE_NONE);
        }

        if (count == 0) {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
            ncplane_set_channels(std, ch);
            ncplane_putstr_yx(std, layout.bodyY + 2, 2, "No patches found.");
            ncplane_putstr_yx(std, layout.bodyY + 3, 2, "Enter a patch file/dir and press Enter.");
        } else {
            std::lock_guard<std::mutex> lock(m_patchesMutex);
            int w = layout.leftW;
            m_patchList.render(std, layout.bodyY + 1, layout.bodyH - 1, focused,
                               [&](ncplane *plane, int y, int idx, bool sel, bool foc) {
                const auto &p = m_patches[idx];
                // Row 1: checkbox + filename
                {
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

                    const char *check = p.checked ? "[x] " : "[ ] ";
                    ncplane_putstr_yx(plane, y, 1, check);

                    int maxNameW = w - 6 - 8;
                    std::string fname = p.filename;
                    if (static_cast<int>(fname.size()) > maxNameW)
                        fname = fname.substr(0, static_cast<size_t>(maxNameW - 3)) + "...";
                    ncplane_putstr(plane, fname.c_str());
                    ncplane_set_styles(plane, NCSTYLE_NONE);
                    ch = 0; ncplane_set_channels(plane, ch);
                }
                // Row 2: format + size + status
                {
                    uint64_t ch = 0;
                    if (sel && foc) ncchannels_set_bg_rgb8(&ch, 0x22, 0x44, 0x66);
                    else if (sel) ncchannels_set_bg_rgb8(&ch, 0x22, 0x22, 0x33);
                    ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
                    ncplane_set_channels(plane, ch);

                    char detail[256];
                    snprintf(detail, sizeof(detail), "    %s — %s — %s",
                             p.formatName.c_str(),
                             formatSize(p.sizeBytes).c_str(),
                             p.status.c_str());
                    int maxW = w - 1;
                    if (static_cast<int>(strlen(detail)) > maxW) detail[maxW] = '\0';
                    ncplane_putstr_yx(plane, y + 1, 0, detail);
                    ch = 0; ncplane_set_channels(plane, ch);
                }
            });
        }
    }

    // ── Detail pane (right) ────────────────────────────────
    drawDetailPane(std, layout.bodyY, layout.bodyH, layout.rightX, layout.rightW);

    // ── Separator + progress ───────────────────────────────
    m_splitPane.renderSeparator(std, layout);
    m_progressBar.render(std, layout.progressY, cols);

    drawFooter(std, rows, cols);

    ncplane_set_channels(std, 0);
    ncplane_set_styles(std, NCSTYLE_NONE);
}

void PatchScreen::onResize(struct notcurses *)
{
}

// ════════════════════════════════════════════════════════════
// Render helpers
// ════════════════════════════════════════════════════════════

void PatchScreen::drawHeader(ncplane *plane, unsigned cols)
{
    // Row 0: Title
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0x00, 0x00);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        ncplane_putstr_yx(plane, 0, 2, "PATCH");
        ncplane_set_styles(plane, NCSTYLE_NONE);
    }

    // Backup flag
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, m_createBackup ? 0x00 : 0xCC,
                                     m_createBackup ? 0xCC : 0x66,
                                     m_createBackup ? 0x00 : 0x66);
        ncplane_set_channels(plane, ch);
        const char *bkStr = m_createBackup ? "[b] Backup ROM: ON" : "[b] Backup ROM: OFF";
        int bkX = static_cast<int>(cols) - 7 - 22;
        if (bkX > 10) ncplane_putstr_yx(plane, 0, bkX, bkStr);
    }

    // "REMUS" right-aligned
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, 0, static_cast<int>(cols) - 7, "REMUS");
    }

    // Row 1: ROM path (TextInput)
    m_romInput.render(plane, 1, 2, static_cast<int>(cols) - 4, m_focus == Focus::RomInput);

    // Row 2: Patch path (TextInput)
    m_patchInput.render(plane, 2, 2, static_cast<int>(cols) - 4, m_focus == Focus::PatchInput);

    // Row 3: separator
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x44, 0x44, 0x44);
        ncplane_set_channels(plane, ch);
        std::string sep(cols, '-');
        ncplane_putstr_yx(plane, 3, 0, sep.c_str());
    }
}

void PatchScreen::drawDetailPane(ncplane *plane, int startY, int height, int startX, unsigned width)
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

    int sel = m_patchList.selected();
    std::lock_guard<std::mutex> lock(m_patchesMutex);
    if (sel < 0 || sel >= static_cast<int>(m_patches.size())) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, startY + 2, startX + 2, "Select a patch to see details");
        return;
    }

    const auto &p = m_patches[sel];
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

    // File name — bold
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        std::string title = p.filename;
        if (static_cast<int>(title.size()) > maxW)
            title = title.substr(0, static_cast<size_t>(maxW - 3)) + "...";
        ncplane_putstr_yx(plane, y, startX + 2, title.c_str());
        ncplane_set_styles(plane, NCSTYLE_NONE);
        y++;
    }

    putField("Format:     ", p.formatName, 0xAA, 0xAA, 0xFF);
    putField("Size:       ", formatSize(p.sizeBytes), 0xCC, 0xCC, 0xCC);
    putField("Status:     ", p.status, 0xCC, 0xCC, 0xCC);

    if (!p.sourceCrc.empty())
        putField("Source CRC: ", p.sourceCrc, 0x88, 0xCC, 0x88);
    if (!p.targetCrc.empty())
        putField("Target CRC: ", p.targetCrc, 0x88, 0xCC, 0x88);

    putField("Valid:      ", p.valid ? "Yes" : "No",
             p.valid ? (uint8_t)0x00 : (uint8_t)0xCC,
             p.valid ? (uint8_t)0xCC : (uint8_t)0x00,
             0x00);

    y++;

    // ROM info
    if (!m_romInput.value().empty() && y < startY + height) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, y++, startX + 2, "Target ROM:");

        ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
        ncplane_set_channels(plane, ch);
        std::string romName = m_romInput.value();
        auto slash = romName.rfind('/');
        if (slash != std::string::npos) romName = romName.substr(slash + 1);
        if (static_cast<int>(romName.size()) > maxW - 2)
            romName = romName.substr(0, static_cast<size_t>(maxW - 5)) + "...";
        ncplane_putstr_yx(plane, y++, startX + 4, romName.c_str());
    }

    y++;
    drawToolStatus(plane, y, startX, startY + height);
}

void PatchScreen::drawToolStatus(ncplane *plane, int &y, int startX, int maxY)
{
    if (y >= maxY) return;

    uint64_t ch = 0;
    ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
    ncplane_set_channels(plane, ch);
    ncplane_putstr_yx(plane, y++, startX + 2, "Patching tools:");

    auto tools = m_patchService->getToolStatus();
    for (auto it = tools.begin(); it != tools.end() && y < maxY; ++it) {
        ch = 0;
        ncchannels_set_fg_rgb8(&ch, it.value() ? 0x00 : 0xCC,
                                     it.value() ? 0xCC : 0x00, 0x00);
        ncplane_set_channels(plane, ch);

        std::string label = "  " + it.key().toStdString() + ": " + (it.value() ? "available" : "NOT FOUND");
        ncplane_putstr_yx(plane, y++, startX + 2, label.c_str());

        // Show install hint for missing tools
        if (!it.value() && y < maxY) {
            std::string hint = ToolHints::getInstallHint(it.key().toStdString());
            if (!hint.empty()) {
                auto nl = hint.find('\n');
                if (nl != std::string::npos) hint = hint.substr(0, nl);
                ch = 0;
                ncchannels_set_fg_rgb8(&ch, 0x55, 0x55, 0x55);
                ncplane_set_channels(plane, ch);
                ncplane_putstr_yx(plane, y++, startX + 4, hint.c_str());
            }
        }
    }

    // Built-in IPS note
    if (y < maxY) {
        ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, y++, startX + 2, "(IPS has built-in fallback)");
    }
}

void PatchScreen::drawFooter(ncplane *plane, unsigned rows, unsigned cols)
{
    const char *hint = nullptr;
    switch (m_focus) {
    case Focus::RomInput:
        hint = "Type ROM path  Enter:confirm  Tab:next  Esc:back";
        break;
    case Focus::PatchInput:
        hint = "Type patch path  Enter:scan  Tab:next  Esc:back";
        break;
    case Focus::PatchList:
        hint = "j/k:nav  Space:toggle  a:all  s:start  b:backup  Esc:back";
        break;
    case Focus::DetailPane:
        hint = "j/k:scroll  Tab:next  Esc:back";
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

void PatchScreen::scanPatches()
{
    std::string path = m_patchInput.value();
    if (path.empty()) return;

    // Expand ~
    if (path[0] == '~') {
        QString home = QDir::homePath();
        path = home.toStdString() + path.substr(1);
    }

    QFileInfo info(QString::fromStdString(path));
    std::vector<PatchEntry> found;

    QStringList patchExts = {"*.ips", "*.bps", "*.ups", "*.xdelta", "*.xd", "*.ppf"};

    auto addPatch = [&](const QString &filePath) {
        QFileInfo fi(filePath);
        Remus::PatchInfo pi = m_patchService->detectFormat(filePath);

        PatchEntry e;
        e.path = filePath.toStdString();
        e.filename = fi.fileName().toStdString();
        e.formatName = pi.formatName.toStdString();
        e.sizeBytes = fi.size();
        e.sourceCrc = pi.sourceChecksum.toStdString();
        e.targetCrc = pi.targetChecksum.toStdString();
        e.valid = pi.valid;
        e.status = pi.valid ? "Ready" : ("Invalid: " + pi.error.toStdString());
        found.push_back(std::move(e));
    };

    if (info.isDir()) {
        QDirIterator it(QString::fromStdString(path), patchExts,
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            addPatch(it.filePath());
        }
    } else if (info.isFile()) {
        addPatch(QString::fromStdString(path));
    }

    // Sort by filename
    std::sort(found.begin(), found.end(), [](const PatchEntry &a, const PatchEntry &b) {
        return a.filename < b.filename;
    });

    {
        std::lock_guard<std::mutex> lock(m_patchesMutex);
        m_patches = std::move(found);
    }
    m_patchList.setCount(static_cast<int>(m_patches.size()));
    m_patchList.setSelected(m_patches.empty() ? -1 : 0);
}

void PatchScreen::startPatching()
{
    if (m_task.running()) return;
    if (m_romInput.value().empty()) return;

    int checked = 0;
    {
        std::lock_guard<std::mutex> lock(m_patchesMutex);
        for (const auto &p : m_patches)
            if (p.checked && p.valid) checked++;
    }
    if (checked == 0) return;

    m_progressBar.set(0, checked, "patching");
    m_task.start([this]() { applyPatches(); });
}

void PatchScreen::applyPatches()
{
    std::string romPath = m_romInput.value();
    if (romPath.empty()) return;
    if (romPath[0] == '~') {
        QString home = QDir::homePath();
        romPath = home.toStdString() + romPath.substr(1);
    }

    QString qRomPath = QString::fromStdString(romPath);

    // Create backup if requested
    if (m_createBackup) {
        QString backupPath = qRomPath + ".bak";
        if (!QFile::exists(backupPath))
            QFile::copy(qRomPath, backupPath);
    }

    // Build work list
    std::vector<size_t> workIndices;
    {
        std::lock_guard<std::mutex> lock(m_patchesMutex);
        for (size_t i = 0; i < m_patches.size(); ++i) {
            if (m_patches[i].checked && m_patches[i].valid)
                workIndices.push_back(i);
        }
    }

    int done = 0;
    for (size_t idx : workIndices) {
        if (m_task.cancelled()) break;

        {
            std::lock_guard<std::mutex> lock(m_patchesMutex);
            m_patches[idx].status = "Applying...";
        }

        applySinglePatch(idx, qRomPath);

        done++;
        m_progressBar.set(done, static_cast<int>(workIndices.size()),
                          m_progressBar.label());
    }

    m_app.post([this]() {
        m_progressBar.set(m_progressBar.done(), m_progressBar.total(), "done");
        m_app.toast("Patching complete", Toast::Level::Success, 3000);
    });
}

void PatchScreen::applySinglePatch(size_t idx, const QString &qRomPath)
{
    std::string patchPath;
    {
        std::lock_guard<std::mutex> lock(m_patchesMutex);
        patchPath = m_patches[idx].path;
    }

    QString qPatchPath = QString::fromStdString(patchPath);
    Remus::PatchInfo pi = m_patchService->detectFormat(qPatchPath);

    // Generate output path: romname_patched.ext
    QString baseName = QFileInfo(qRomPath).completeBaseName();
    QString ext = QFileInfo(qRomPath).suffix();
    QString outPath = QFileInfo(qRomPath).absolutePath() + "/" + baseName + "_patched." + ext;

    auto result = m_patchService->apply(qRomPath, pi, outPath);

    {
        std::lock_guard<std::mutex> lock(m_patchesMutex);
        if (result.success) {
            std::string statusStr = "Applied";
            if (result.checksumVerified)
                statusStr += " (verified)";
            m_patches[idx].status = statusStr;
        } else {
            m_patches[idx].status = "Error: " + result.error.toStdString();
        }
    }
}

std::vector<std::pair<std::string, std::string>> PatchScreen::keybindings() const
{
    return {
        {"Tab",   "Cycle focus"},
        {"Enter", "Confirm / scan"},
        {"j/k",   "Navigate list"},
        {"g/G",   "First / last"},
        {"Space", "Toggle patch"},
        {"a",     "Toggle all"},
        {"s",     "Start patching"},
        {"b",     "Toggle backup"},
        {"Esc",   "Cancel / back"},
    };
}