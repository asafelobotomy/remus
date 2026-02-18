#include "compressor_screen.h"
#include "app.h"
#include "tool_hints.h"

#include "../services/conversion_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <cstring>
#include <algorithm>

// ════════════════════════════════════════════════════════════
// Construction / Lifecycle
// ════════════════════════════════════════════════════════════

CompressorScreen::CompressorScreen(TuiApp &app)
    : Screen(app)
{
    m_conversionService = new Remus::ConversionService();
    m_fileList.setCheckboxes(true);
    m_fileList.setRowsPerItem(2);
}

CompressorScreen::~CompressorScreen()
{
    m_conversionService->cancel();
    m_task.stop();
    delete m_conversionService;
}

void CompressorScreen::onEnter()
{
}

void CompressorScreen::onLeave()
{
    m_conversionService->cancel();
    m_task.stop();
}

// ════════════════════════════════════════════════════════════
// File type classification
// ════════════════════════════════════════════════════════════

CompressorScreen::FileType CompressorScreen::detectFileType(const std::string &filename)
{
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.size() >= 4) {
        std::string ext = lower.substr(lower.rfind('.') != std::string::npos ? lower.rfind('.') : 0);
        if (ext == ".cue") return FileType::CUE;
        if (ext == ".iso") return FileType::ISO;
        if (ext == ".gdi") return FileType::GDI;
        if (ext == ".chd") return FileType::CHD;
        if (ext == ".zip") return FileType::ZIP;
        if (ext == ".7z")  return FileType::SevenZ;
        if (ext == ".rar") return FileType::RAR;
    }
    return FileType::Unknown;
}

std::string CompressorScreen::fileTypeString(FileType ft)
{
    switch (ft) {
    case FileType::CUE:    return "BIN/CUE";
    case FileType::ISO:    return "ISO";
    case FileType::GDI:    return "GDI";
    case FileType::CHD:    return "CHD";
    case FileType::ZIP:    return "ZIP";
    case FileType::SevenZ: return "7z";
    case FileType::RAR:    return "RAR";
    default:               return "Unknown";
    }
}

std::string CompressorScreen::formatSize(int64_t bytes)
{
    if (bytes < 0) return "?";
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
        return buf;
    }
    if (bytes < 1024LL * 1024 * 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024));
        return buf;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024 * 1024));
    return buf;
}

// ════════════════════════════════════════════════════════════
// Input handling
// ════════════════════════════════════════════════════════════

bool CompressorScreen::handleInput(struct notcurses *, const ncinput &ni, int ch)
{
    // Esc first-refusal: cancel running task
    if (ch == NCKEY_ESC) {
        if (m_task.running()) {
            m_conversionService->cancel();
            m_task.stop();
            m_progressBar.set(0, 0, "cancelled");
            m_app.toast("Processing cancelled", Toast::Level::Warning);
            return true;
        }
        return false;
    }

    // Mouse click-to-select
    if (ch == NCKEY_BUTTON1 && ni.evtype == NCTYPE_PRESS) {
        int row = ni.y, col = ni.x;
        unsigned cols = m_app.cols();
        int fieldW = static_cast<int>(cols) - 2;
        if (m_sourceInput.hitTest(row, col, 1, 2, fieldW)) {
            m_focus = Focus::SourceInput;
        } else if (m_outputInput.hitTest(row, col, 2, 2, fieldW)) {
            m_focus = Focus::OutputInput;
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
        if (m_focus == Focus::SourceInput)
            m_focus = Focus::OutputInput;
        else if (m_focus == Focus::OutputInput)
            m_focus = Focus::FileList;
        else if (m_focus == Focus::FileList)
            m_focus = Focus::DetailPane;
        else
            m_focus = Focus::SourceInput;
        return true;
    }

    // 'm' cycles mode (Compress → Extract → Archive → Compress) when not processing
    if (ch == 'm' && !m_task.running()) {
        switch (m_mode) {
        case OpMode::Compress: m_mode = OpMode::Extract; break;
        case OpMode::Extract:  m_mode = OpMode::Archive; break;
        case OpMode::Archive:  m_mode = OpMode::Compress; break;
        }
        return true;
    }

    // 'd' toggles delete-originals flag
    if (ch == 'd' && !m_task.running()) {
        m_deleteOriginals = !m_deleteOriginals;
        return true;
    }

    // ── Source input ───────────────────────────────────────
    if (m_focus == Focus::SourceInput) {
        if (TextInput::isSubmit(ch)) {
            scanSource();
            return true;
        }
        return m_sourceInput.handleInput(ch);
    }

    // ── Output input ───────────────────────────────────────
    if (m_focus == Focus::OutputInput) {
        if (TextInput::isSubmit(ch)) {
            m_focus = Focus::FileList;
            return true;
        }
        return m_outputInput.handleInput(ch);
    }

    // ── File list ──────────────────────────────────────────
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
                                          [](const FileEntry &f){ return f.checked; });
            for (auto &f : m_files) f.checked = !allChecked;
            return true;
        }
        if (action != SelectableList::Action::None)
            return true;

        // 's' / Enter to start processing
        if ((ch == 's' || ch == 'S') && !m_task.running()) {
            startProcessing();
            return true;
        }
        return false;
    }

    // ── Detail pane ────────────────────────────────────────
    if (m_focus == Focus::DetailPane) {
        auto action = m_fileList.handleInput(ch);
        if (action != SelectableList::Action::None)
            return true;
        return false;
    }

    return false;
}

// ════════════════════════════════════════════════════════════
// Tick
// ════════════════════════════════════════════════════════════

bool CompressorScreen::tick()
{
    return m_task.running();
}

// ════════════════════════════════════════════════════════════
// Rendering
// ════════════════════════════════════════════════════════════

void CompressorScreen::render(struct notcurses *nc)
{
    unsigned rows = m_app.rows();
    unsigned cols = m_app.cols();
    ncplane *std = notcurses_stdplane(nc);

    const int headerH = 4;
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
    m_fileList.ensureVisible(layout.bodyH / 2);

    bool listFocused = (m_focus == Focus::FileList);
    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        int checked = 0;
        for (const auto &f : m_files) if (f.checked) checked++;
        char hdr[128];
        snprintf(hdr, sizeof(hdr), " Files (%d) [%d selected]",
                 static_cast<int>(m_files.size()), checked);
        m_fileList.renderHeader(std, layout.bodyY, 0, hdr, listFocused);
    }

    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        m_fileList.render(std, layout.bodyY + 1, layout.bodyH - 1, listFocused,
            [this, listFocused, listW = layout.leftW](ncplane *plane, int y, int idx,
                                                        bool selected, bool /*focused*/) {
                if (idx < 0 || idx >= static_cast<int>(m_files.size())) return;
                const auto &f = m_files[idx];

                // Row 1: checkbox + filename + size
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
                    if (selected) ncplane_set_styles(plane, NCSTYLE_BOLD);

                    const char *check = f.checked ? "[x] " : "[ ] ";
                    ncplane_putstr_yx(plane, y, 1, check);

                    int maxNameW = listW - 6 - 12;
                    std::string fname = f.filename;
                    if (static_cast<int>(fname.size()) > maxNameW)
                        fname = fname.substr(0, static_cast<size_t>(maxNameW - 3)) + "...";
                    ncplane_putstr(plane, fname.c_str());

                    // Size right-aligned
                    std::string sizeStr = formatSize(f.sizeBytes);
                    int sizeX = listW - static_cast<int>(sizeStr.size()) - 1;
                    if (sizeX > 0) {
                        ch = 0;
                        ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
                        if (selected && listFocused) ncchannels_set_bg_rgb8(&ch, 0x22, 0x44, 0x66);
                        else if (selected) ncchannels_set_bg_rgb8(&ch, 0x22, 0x22, 0x33);
                        ncplane_set_channels(plane, ch);
                        ncplane_putstr_yx(plane, y, sizeX, sizeStr.c_str());
                    }
                    ncplane_set_styles(plane, NCSTYLE_NONE);
                    ch = 0;
                    ncplane_set_channels(plane, ch);
                }

                // Row 2: type + status
                {
                    uint64_t ch = 0;
                    if (selected && listFocused) ncchannels_set_bg_rgb8(&ch, 0x22, 0x44, 0x66);
                    else if (selected) ncchannels_set_bg_rgb8(&ch, 0x22, 0x22, 0x33);
                    ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
                    ncplane_set_channels(plane, ch);

                    char detail[256];
                    snprintf(detail, sizeof(detail), "    %s — %s",
                             fileTypeString(f.type).c_str(), f.status.c_str());
                    if (static_cast<int>(strlen(detail)) > listW - 1) detail[listW - 1] = '\0';
                    ncplane_putstr_yx(plane, y + 1, 0, detail);
                    ch = 0;
                    ncplane_set_channels(plane, ch);
                }
            });

        if (m_files.empty()) {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
            ncplane_set_channels(std, ch);
            ncplane_putstr_yx(std, layout.bodyY + 2, 2, "No files. Enter source path and press Enter.");
        }
    }

    drawDetailPane(std, layout.bodyY, layout.bodyH, layout.rightX, static_cast<unsigned>(layout.rightW));
    m_splitPane.renderSeparator(std, layout);
    m_progressBar.render(std, layout.progressY, cols);
    drawFooter(std, rows, cols);

    ncplane_set_channels(std, 0);
    ncplane_set_styles(std, NCSTYLE_NONE);
}

void CompressorScreen::onResize(struct notcurses *)
{
}

// ════════════════════════════════════════════════════════════
// Render helpers
// ════════════════════════════════════════════════════════════

void CompressorScreen::drawHeader(ncplane *plane, unsigned cols)
{
    // Row 0: Title + mode
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x00, 0xAA, 0xCC);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        ncplane_putstr_yx(plane, 0, 2, "COMPRESSOR");
        ncplane_set_styles(plane, NCSTYLE_NONE);
    }

    // Mode indicator
    {
        uint64_t ch = 0;
        const char *modeStr;
        switch (m_mode) {
        case OpMode::Compress: modeStr = "[COMPRESS\u2192CHD]"; break;
        case OpMode::Extract:  modeStr = "[EXTRACT CHD\u2192BIN]"; break;
        case OpMode::Archive:  modeStr = "[COMPRESS\u2192ARCHIVE]"; break;
        }
        switch (m_mode) {
        case OpMode::Compress: ncchannels_set_fg_rgb8(&ch, 0x00, 0xCC, 0x00); break;
        case OpMode::Extract:  ncchannels_set_fg_rgb8(&ch, 0xCC, 0xAA, 0x00); break;
        case OpMode::Archive:  ncchannels_set_fg_rgb8(&ch, 0x00, 0xAA, 0xCC); break;
        }
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, 0, 14, modeStr);
    }

    // "REMUS" right-aligned
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, 0, static_cast<int>(cols) - 7, "REMUS");
    }

    // Delete originals flag
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, m_deleteOriginals ? 0xCC : 0x66,
                                     m_deleteOriginals ? 0x44 : 0x66,
                                     m_deleteOriginals ? 0x44 : 0x66);
        ncplane_set_channels(plane, ch);
        const char *delStr = m_deleteOriginals ? "[d] Delete originals: ON" : "[d] Delete originals: OFF";
        ncplane_putstr_yx(plane, 0, static_cast<int>(cols) - 7 - 28, delStr);
    }

    // Row 1: Source path via TextInput widget
    {
        int fieldWidth = static_cast<int>(cols) - 2;
        m_sourceInput.render(plane, 1, 2, fieldWidth, m_focus == Focus::SourceInput);
    }

    // Row 2: Output path via TextInput widget
    {
        int fieldWidth = static_cast<int>(cols) - 2;
        m_outputInput.render(plane, 2, 2, fieldWidth, m_focus == Focus::OutputInput);
    }

    // Row 3: separator
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x44, 0x44, 0x44);
        ncplane_set_channels(plane, ch);
        std::string sep(cols, '-');
        ncplane_putstr_yx(plane, 3, 0, sep.c_str());
    }
}

void CompressorScreen::drawDetailPane(ncplane *plane, int startY, int height, int startX, unsigned width)
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

    int sel = m_fileList.selected();
    std::lock_guard<std::mutex> lock(m_filesMutex);
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

    // File name — bold
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        std::string title = f.filename;
        if (static_cast<int>(title.size()) > maxW)
            title = title.substr(0, static_cast<size_t>(maxW - 3)) + "...";
        ncplane_putstr_yx(plane, y, startX + 2, title.c_str());
        ncplane_set_styles(plane, NCSTYLE_NONE);
        y++;
    }

    putField("Format:  ", fileTypeString(f.type), 0xAA, 0xAA, 0xFF);
    putField("Size:    ", formatSize(f.sizeBytes), 0xCC, 0xCC, 0xCC);
    putField("Path:    ", f.path, 0x88, 0x88, 0x88);
    putField("Status:  ", f.status, 0xCC, 0xCC, 0xCC);

    if (f.ratio > 0.0) {
        char ratioStr[32];
        snprintf(ratioStr, sizeof(ratioStr), "%.1f%%", f.ratio * 100.0);
        putField("Ratio:   ", ratioStr, 0x00, 0xCC, 0x00);
    }

    y++;

    // Tool availability
    if (y < startY + height) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, y++, startX + 2, "Tools:");

        bool chdOk = m_conversionService->isChdmanAvailable();
        ch = 0;
        ncchannels_set_fg_rgb8(&ch, chdOk ? 0x00 : 0xCC, chdOk ? 0xCC : 0x00, 0x00);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, y, startX + 4, chdOk ? "chdman: available" : "chdman: NOT FOUND");
        if (!chdOk && y < startY + height) {
            std::string hint = ToolHints::getInstallHint("chdman");
            if (!hint.empty()) {
                // Show just the first line of the hint
                auto nl = hint.find('\n');
                if (nl != std::string::npos) hint = hint.substr(0, nl);
                ch = 0;
                ncchannels_set_fg_rgb8(&ch, 0x55, 0x55, 0x55);
                ncplane_set_channels(plane, ch);
                ncplane_putstr_yx(plane, y + 1, startX + 6, hint.c_str());
                y++;
            }
        }
        y++;
    }

    // Mode description
    y++;
    if (y < startY + height) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
        ncplane_set_channels(plane, ch);
        switch (m_mode) {
        case OpMode::Compress:
            ncplane_putstr_yx(plane, y, startX + 2, "Mode: Convert disc images to CHD");
            break;
        case OpMode::Extract:
            ncplane_putstr_yx(plane, y, startX + 2, "Mode: Extract CHD back to BIN/CUE");
            break;
        case OpMode::Archive:
            ncplane_putstr_yx(plane, y, startX + 2, "Mode: Compress files into ZIP/7z");
            break;
        }
    }
}

void CompressorScreen::drawFooter(ncplane *plane, unsigned rows, unsigned cols)
{
    const char *hint = nullptr;
    switch (m_focus) {
    case Focus::SourceInput:
        hint = "Enter:scan dir  Tab:next  m:mode  d:del orig  Esc:back";
        break;
    case Focus::OutputInput:
        hint = "Enter:confirm  Tab:next  Esc:back";
        break;
    case Focus::FileList:
        hint = "j/k:nav  Space:toggle  a:all  s:start  m:mode  Esc:back";
        break;
    case Focus::DetailPane:
        hint = "j/k:nav files  Tab:next  Esc:back";
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

void CompressorScreen::scanSource()
{
    std::string path = m_sourceInput.value();
    if (path.empty()) return;

    // Expand ~
    if (path[0] == '~') {
        QString home = QDir::homePath();
        path = home.toStdString() + path.substr(1);
    }

    QFileInfo info(QString::fromStdString(path));
    std::vector<FileEntry> found;

    // Supported extensions for compress mode
    QStringList compressExts = {"*.cue", "*.iso", "*.gdi"};
    QStringList extractExts = {"*.chd"};
    QStringList archiveExts = {"*.zip", "*.7z", "*.rar"};

    // All file types to scan for in Archive mode
    QStringList allExts = {"*.cue", "*.iso", "*.gdi", "*.chd", "*.zip", "*.7z", "*.rar",
                           "*.bin", "*.img", "*.rom", "*.nes", "*.sfc", "*.smc",
                           "*.gb", "*.gbc", "*.gba", "*.nds", "*.n64", "*.z64",
                           "*.md", "*.gen", "*.sms", "*.gg"};

    QStringList filters;
    switch (m_mode) {
    case OpMode::Compress: filters = compressExts + archiveExts; break;
    case OpMode::Extract:  filters = extractExts; break;
    case OpMode::Archive:  filters = allExts; break;
    }

    auto addFile = [&](const QString &filePath) {
        QFileInfo fi(filePath);
        FileEntry e;
        e.path = filePath.toStdString();
        e.filename = fi.fileName().toStdString();
        e.type = detectFileType(e.filename);
        e.sizeBytes = fi.size();
        e.status = "Ready";
        found.push_back(std::move(e));
    };

    if (info.isDir()) {
        QDirIterator it(QString::fromStdString(path), filters,
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            addFile(it.filePath());
        }
    } else if (info.isFile()) {
        addFile(QString::fromStdString(path));
    }

    // Sort by filename
    std::sort(found.begin(), found.end(), [](const FileEntry &a, const FileEntry &b) {
        return a.filename < b.filename;
    });

    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        m_files = std::move(found);
    }
    m_fileList.setCount(static_cast<int>(m_files.size()));
    m_fileList.setSelected(m_files.empty() ? -1 : 0);
}

void CompressorScreen::startProcessing()
{
    if (m_task.running()) return;

    // Count checked files
    int checked = 0;
    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        for (const auto &f : m_files)
            if (f.checked) checked++;
    }
    if (checked == 0) return;

    std::string label;
    switch (m_mode) {
    case OpMode::Compress: label = "converting"; break;
    case OpMode::Extract:  label = "extracting"; break;
    case OpMode::Archive:  label = "compressing"; break;
    }
    m_progressBar.set(0, checked, label);

    m_task.start([this]() { processFiles(); });
}

void CompressorScreen::processFiles()
{
    std::string outDir = m_outputInput.value();
    if (!outDir.empty() && outDir[0] == '~') {
        QString home = QDir::homePath();
        outDir = home.toStdString() + outDir.substr(1);
    }

    // Build work list
    std::vector<size_t> workIndices;
    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        for (size_t i = 0; i < m_files.size(); ++i) {
            if (m_files[i].checked)
                workIndices.push_back(i);
        }
    }

    int done = 0;
    for (size_t idx : workIndices) {
        if (m_task.cancelled()) break;

        {
            std::lock_guard<std::mutex> lock(m_filesMutex);
            m_files[idx].status = "Processing...";
        }

        processSingleFile(idx, outDir);

        done++;
        m_progressBar.set(done, static_cast<int>(workIndices.size()),
                          m_progressBar.label());
    }

    m_app.post([this]() {
        m_progressBar.set(m_progressBar.done(), m_progressBar.total(), "done");
        m_app.toast("Conversion complete", Toast::Level::Info, 3000);
    });
}

void CompressorScreen::processSingleFile(size_t idx, const std::string &outDir)
{
    std::string filePath;
    FileType ftype;
    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        filePath = m_files[idx].path;
        ftype = m_files[idx].type;
    }

    QString qPath = QString::fromStdString(filePath);
    QString qOutDir = outDir.empty() ? QString() : QString::fromStdString(outDir);

    if (m_mode == OpMode::Compress) {
        // Archive types — extract via ConversionService
        if (ftype == FileType::ZIP || ftype == FileType::SevenZ || ftype == FileType::RAR) {
            auto exResult = m_conversionService->extractArchive(qPath, qOutDir);
            std::lock_guard<std::mutex> lock(m_filesMutex);
            if (exResult.success)
                m_files[idx].status = "Extracted (" + std::to_string(exResult.filesExtracted) + " files)";
            else
                m_files[idx].status = "Error: " + exResult.error.toStdString();
            return;
        }

        // Convert disc images to CHD via ConversionService
        auto result = m_conversionService->convertToCHD(qPath, Remus::CHDCodec::Auto, qOutDir);

        {
            std::lock_guard<std::mutex> lock(m_filesMutex);
            if (result.success) {
                m_files[idx].status = "Done (" + formatSize(result.outputSize) + ")";
                m_files[idx].ratio = result.compressionRatio;
                if (m_deleteOriginals && !result.inputPath.isEmpty())
                    QFile::remove(result.inputPath);
            } else {
                m_files[idx].status = "Error: " + result.error.toStdString();
            }
        }
    } else if (m_mode == OpMode::Extract) {
        // Extract CHD → BIN/CUE via ConversionService
        auto result = m_conversionService->extractCHD(qPath, qOutDir);
        {
            std::lock_guard<std::mutex> lock(m_filesMutex);
            if (result.success) {
                m_files[idx].status = "Done (extracted)";
                if (m_deleteOriginals)
                    QFile::remove(qPath);
            } else {
                m_files[idx].status = "Error: " + result.error.toStdString();
            }
        }
    } else {
        // Archive mode — compress files to ZIP/7z via ConversionService
        // Determine output archive path
        QFileInfo fi(qPath);
        QString archiveName = fi.completeBaseName() + QStringLiteral(".zip");
        QString archivePath = qOutDir.isEmpty()
            ? fi.absolutePath() + QStringLiteral("/") + archiveName
            : qOutDir + QStringLiteral("/") + archiveName;

        auto result = m_conversionService->compressToArchive(
            QStringList{qPath}, archivePath, Remus::ArchiveFormat::ZIP);
        {
            std::lock_guard<std::mutex> lock(m_filesMutex);
            if (result.success) {
                m_files[idx].status = "Done (" + formatSize(result.compressedSize) + ")";
                if (result.originalSize > 0)
                    m_files[idx].ratio = static_cast<double>(result.compressedSize) / static_cast<double>(result.originalSize);
                if (m_deleteOriginals)
                    QFile::remove(qPath);
            } else {
                m_files[idx].status = "Error: " + result.error.toStdString();
            }
        }
    }
}

std::vector<std::pair<std::string, std::string>> CompressorScreen::keybindings() const
{
    return {
        {"Tab",   "Cycle focus"},
        {"Enter", "Scan / start"},
        {"j/k",   "Navigate list"},
        {"g/G",   "First / last"},
        {"Space", "Toggle file"},
        {"a",     "Toggle all"},
        {"s",     "Start processing"},
        {"m",     "Cycle mode (CHD/Extract/Archive)"},
        {"d",     "Toggle delete originals"},
        {"Esc",   "Cancel / back"},
    };
}
