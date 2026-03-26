#include "organize_screen.h"
#include "app.h"
#include "tool_hints.h"

#include "../core/database.h"
#include "../core/organize_engine.h"
#include "../core/template_engine.h"
#include "../core/constants/confidence.h"
#include "../core/constants/settings.h"
#include "../metadata/metadata_provider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QMap>
#include <QSettings>
#include <cstring>
#include <algorithm>

using Remus::Database;
using Remus::FileRecord;
using Remus::GameMetadata;

// ════════════════════════════════════════════════════════════
// Construction / Lifecycle
// ════════════════════════════════════════════════════════════

OrganizeScreen::OrganizeScreen(TuiApp &app)
    : Screen(app)
{
    m_fileList.setRowsPerItem(2);
}

OrganizeScreen::~OrganizeScreen()
{
    m_task.stop();
}

void OrganizeScreen::onEnter()
{
    loadFromDatabase();
    if (!m_entries.empty())
        runDryRun();
}

void OrganizeScreen::onLeave()
{
    m_task.stop();
}

// ════════════════════════════════════════════════════════════
// Data loading
// ════════════════════════════════════════════════════════════

void OrganizeScreen::loadFromDatabase()
{
    Database &db = m_app.db();

    // Read confidence threshold from settings (fallback to constant)
    QSettings settings;
    float threshold = settings.value(
        QString::fromUtf8(Remus::Constants::Settings::Match::CONFIDENCE_THRESHOLD),
        Remus::Constants::Confidence::Thresholds::ORGANIZE_MINIMUM
    ).toFloat();

    // Collect file records for currentPath + systemId lookup
    auto allFiles = db.getAllFiles();
    QMap<int, FileRecord> fileMap;
    for (const auto &fr : allFiles)
        fileMap.insert(fr.id, fr);

    // Collect all matches — keep those with high confidence or user-confirmed
    auto allMatches = db.getAllMatches();

    std::vector<OrganizeEntry> entries;
    for (auto it = allMatches.constBegin(); it != allMatches.constEnd(); ++it) {
        const auto &mr = it.value();
        if (mr.isRejected)
            continue;
        if (!mr.isConfirmed && mr.confidence < threshold)
            continue;

        auto fileIt = fileMap.find(mr.fileId);
        if (fileIt == fileMap.end())
            continue;
        const auto &fr = fileIt.value();

        OrganizeEntry e;
        e.fileId      = mr.fileId;
        e.confidence  = static_cast<int>(mr.confidence);
        e.filename    = fr.filename.toStdString();
        e.system      = db.getSystemDisplayName(fr.systemId).toStdString();
        e.title       = mr.gameTitle.toStdString();
        e.region      = mr.region.toStdString();
        e.publisher   = mr.publisher.toStdString();
        e.developer   = mr.developer.toStdString();
        e.releaseYear = mr.releaseYear;
        e.oldPath     = fr.currentPath.toStdString();
        e.status      = EntryStatus::Pending;

        // Collect linked child files (e.g. .bin tracks for a .cue)
        auto children = db.getFilesByParent(mr.fileId);
        for (const auto &child : children)
            e.linkedFileIds.push_back(child.id);

        entries.push_back(std::move(e));
    }

    // Sort: confirmed first, then by confidence desc, then alphabetically
    std::sort(entries.begin(), entries.end(), [](const OrganizeEntry &a, const OrganizeEntry &b) {
        if (a.confidence != b.confidence) return a.confidence > b.confidence;
        return a.filename < b.filename;
    });

    {
        std::lock_guard<std::mutex> lock(m_entriesMutex);
        m_entries = std::move(entries);
    }

    m_fileList.setCount(static_cast<int>(m_entries.size()));
    m_progressBar.set(0, 0, "");
}

// ════════════════════════════════════════════════════════════
// Dry-run preview (main thread, synchronous)
// ════════════════════════════════════════════════════════════

static GameMetadata makeMetadata(const OrganizeScreen::OrganizeEntry &e)
{
    GameMetadata meta;
    meta.title     = QString::fromStdString(e.title);
    meta.system    = QString::fromStdString(e.system);
    meta.region    = QString::fromStdString(e.region);
    meta.publisher = QString::fromStdString(e.publisher);
    meta.developer = QString::fromStdString(e.developer);
    if (e.releaseYear > 0)
        meta.releaseDate = QString("%1-01-01").arg(e.releaseYear);
    return meta;
}

void OrganizeScreen::runDryRun()
{
    if (m_task.running()) {
        m_app.toast("Execute running — wait for it to finish first",
                    Toast::Level::Warning);
        return;
    }

    const std::string destStr = m_destInput.value();
    if (destStr.empty()) {
        // Just reset to Pending so user knows preview isn't ready
        std::lock_guard<std::mutex> lock(m_entriesMutex);
        for (auto &e : m_entries)
            e.status = EntryStatus::Pending;
        m_progressBar.set(0, 0, "Set destination directory first");
        return;
    }

    const QString dest = QString::fromStdString(destStr);

    // Build the template string
    QString tmpl = QString::fromStdString(m_templInput.value()).trimmed();
    if (tmpl.isEmpty())
        tmpl = Remus::TemplateEngine::getNoIntroTemplate();

    // Create engine in dry-run mode
    auto engine = std::make_unique<Remus::OrganizeEngine>(m_app.db());
    engine->setDryRun(true);
    engine->setTemplate(tmpl);
    engine->setCollisionStrategy(Remus::CollisionStrategy::Rename);

    int previewed = 0;
    int skipped   = 0;

    {
        std::lock_guard<std::mutex> lock(m_entriesMutex);
        for (auto &entry : m_entries) {
            if (entry.oldPath.empty()) {
                entry.status = EntryStatus::Skipped;
                ++skipped;
                continue;
            }
            GameMetadata meta = makeMetadata(entry);
            Remus::OrganizeResult result = engine->organizeFile(
                entry.fileId, meta, dest, Remus::FileOperation::Move);

            if (result.success) {
                entry.newPath = result.newPath.toStdString();
                entry.status  = EntryStatus::Preview;
                entry.errorMsg.clear();
                ++previewed;
            } else {
                entry.newPath.clear();
                entry.status   = EntryStatus::Error;
                entry.errorMsg = result.error.toStdString();
                ++skipped;
            }
        }
    }

    char label[128];
    snprintf(label, sizeof(label), "Preview: %d ready, %d skipped", previewed, skipped);
    m_progressBar.set(previewed, static_cast<int>(m_entries.size()), label);
}

// ════════════════════════════════════════════════════════════
// Execute (background file I/O only, DB update on main thread)
// ════════════════════════════════════════════════════════════

void OrganizeScreen::runExecute()
{
    if (m_task.running()) {
        m_app.toast("Already running", Toast::Level::Warning);
        return;
    }

    const std::string destStr = m_destInput.value();
    if (destStr.empty()) {
        m_app.toast("Set a destination directory first", Toast::Level::Error);
        return;
    }

    // Check at least one entry is in Preview state
    {
        std::lock_guard<std::mutex> lock(m_entriesMutex);
        bool anyReady = std::any_of(m_entries.begin(), m_entries.end(),
                                    [](const OrganizeEntry &e) {
                                        return e.status == EntryStatus::Preview;
                                    });
        if (!anyReady) {
            m_app.toast("Run Preview first (press p)", Toast::Level::Warning);
            return;
        }
    }

    const bool doCopy = (m_opMode == OpMode::Copy);

    // Count ready entries
    {
        std::lock_guard<std::mutex> lock(m_entriesMutex);
        int ready = static_cast<int>(
            std::count_if(m_entries.begin(), m_entries.end(),
                          [](const OrganizeEntry &e){ return e.status == EntryStatus::Preview; }));
        m_taskDone.store(0);
        m_taskTotal.store(ready);
        m_progressBar.set(0, ready, "Organizing…");
    }

    m_task.start([this, doCopy]() {
        // Build work list under lock, then release for I/O
        struct WorkItem {
            size_t index;
            std::string oldPath;
            std::string newPath;
            std::vector<int> linkedFileIds;
        };
        std::vector<WorkItem> work;
        {
            std::lock_guard<std::mutex> lock(m_entriesMutex);
            for (size_t i = 0; i < m_entries.size(); ++i) {
                if (m_entries[i].status == EntryStatus::Preview) {
                    work.push_back({i, m_entries[i].oldPath, m_entries[i].newPath,
                                    m_entries[i].linkedFileIds});
                }
            }
        }

        int done = 0;
        for (const auto &w : work) {
            if (m_task.cancelled()) break;

            // Ensure destination directory exists
            QString newPath = QString::fromStdString(w.newPath);
            QDir().mkpath(QFileInfo(newPath).absolutePath());

            // Move/copy primary file
            auto moveOrCopy = [&](const QString &src, const QString &dst) -> std::pair<bool, std::string> {
                if (doCopy) {
                    QFile f(src);
                    if (f.copy(dst)) return {true, {}};
                    return {false, f.errorString().toStdString()};
                } else {
                    QFile f(src);
                    if (f.rename(dst)) return {true, {}};
                    // Fall back to copy+delete
                    QFile f2(src);
                    if (f2.copy(dst)) {
                        QFile::remove(src);
                        return {true, {}};
                    }
                    return {false, f2.errorString().toStdString()};
                }
            };

            auto [ok, errMsg] = moveOrCopy(
                QString::fromStdString(w.oldPath), newPath);

            // Co-move linked files (tracks, bins) to same destination dir
            std::string linkedErr;
            if (ok && !w.linkedFileIds.empty()) {
                QString destDir = QFileInfo(newPath).absolutePath();
                for (int childId : w.linkedFileIds) {
                    if (m_task.cancelled()) break;
                    // Look up child path from DB on main thread isn't safe from bg,
                    // so we read it under our entries mutex (children aren't entries,
                    // use DB directly — it's thread-safe for reads)
                    auto childRec = m_app.db().getFileById(childId);
                    if (childRec.id == 0) continue;
                    QString childDest = destDir + "/" + childRec.filename;
                    auto [childOk, childErr] = moveOrCopy(childRec.currentPath, childDest);
                    if (!childOk) {
                        linkedErr = "Linked file " + childRec.filename.toStdString()
                                  + ": " + childErr;
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(m_entriesMutex);
                auto &entry = m_entries[w.index];
                if (ok) {
                    entry.status = EntryStatus::OK;
                    if (!linkedErr.empty())
                        entry.errorMsg = linkedErr; // partial success
                } else {
                    entry.status   = EntryStatus::Error;
                    entry.errorMsg = errMsg.empty() ? "File operation failed" : errMsg;
                }
            }
            ++done;
            m_taskDone.store(done);
        }

        // Post DB updates to the main thread
        m_app.post([this]() {
            Database &db = m_app.db();
            int okCount = 0;
            {
                std::lock_guard<std::mutex> lk(m_entriesMutex);
                for (const auto &e : m_entries) {
                    if (e.status == EntryStatus::OK) {
                        db.updateFilePath(e.fileId,
                                          QString::fromStdString(e.newPath));
                        // Update linked file paths too
                        if (!e.linkedFileIds.empty()) {
                            QString destDir = QFileInfo(
                                QString::fromStdString(e.newPath)).absolutePath();
                            for (int childId : e.linkedFileIds) {
                                auto childRec = db.getFileById(childId);
                                if (childRec.id > 0) {
                                    db.updateFilePath(childId,
                                        destDir + "/" + childRec.filename);
                                }
                            }
                        }
                        ++okCount;
                    }
                }
            }
            char msg[128];
            snprintf(msg, sizeof(msg), "Done — %d file(s) organized", okCount);
            m_progressBar.set(okCount, m_taskTotal.load(), msg);
            m_app.toast(std::string(msg), Toast::Level::Success);
        });
    });
}

// ════════════════════════════════════════════════════════════
// Input handling
// ════════════════════════════════════════════════════════════

bool OrganizeScreen::handleInput(struct notcurses *, const ncinput &ni, int ch)
{
    // Esc: cancel running task or pop screen
    if (ch == NCKEY_ESC) {
        if (m_task.running()) {
            m_task.stop();
            m_progressBar.set(m_taskDone.load(), m_taskTotal.load(), "cancelled");
            m_app.toast("Organize cancelled", Toast::Level::Warning);
            return true;
        }
        return false; // pop screen
    }

    // Tab: cycle focus
    if (ch == '\t') {
        switch (m_focus) {
        case Focus::DestInput:  m_focus = Focus::TemplInput; break;
        case Focus::TemplInput: m_focus = Focus::FileList;   break;
        case Focus::FileList:   m_focus = Focus::DestInput;  break;
        }
        return true;
    }

    // 'p': run preview
    if (ch == 'p' && m_focus != Focus::DestInput && m_focus != Focus::TemplInput) {
        runDryRun();
        return true;
    }

    // 'e': execute
    if (ch == 'e' && m_focus != Focus::DestInput && m_focus != Focus::TemplInput) {
        runExecute();
        return true;
    }

    // 'm': toggle Move / Copy
    if (ch == 'm' && m_focus != Focus::DestInput && m_focus != Focus::TemplInput) {
        m_opMode = (m_opMode == OpMode::Move) ? OpMode::Copy : OpMode::Move;
        return true;
    }

    // 'n': apply No-Intro template preset
    if (ch == 'n' && m_focus != Focus::DestInput) {
        m_templInput.setValue(
            Remus::TemplateEngine::getNoIntroTemplate().toStdString());
        return true;
    }

    // 'r': apply Redump template preset
    if (ch == 'r' && m_focus != Focus::DestInput) {
        m_templInput.setValue(
            Remus::TemplateEngine::getRedumpTemplate().toStdString());
        return true;
    }

    // Focus-specific routing
    switch (m_focus) {
    case Focus::DestInput:
        if (TextInput::isSubmit(ch)) {
            m_focus = Focus::TemplInput;
            return true;
        }
        m_destInput.handleInput(ch);
        return true;

    case Focus::TemplInput:
        if (TextInput::isSubmit(ch)) {
            runDryRun();
            m_focus = Focus::FileList;
            return true;
        }
        m_templInput.handleInput(ch);
        return true;

    case Focus::FileList: {
        // j/k / arrow navigation
        if (ch == 'j' || ch == NCKEY_DOWN) {
            m_fileList.handleInput('j');
            return true;
        }
        if (ch == 'k' || ch == NCKEY_UP) {
            m_fileList.handleInput('k');
            return true;
        }
        // Enter on file list: re-run preview for current item
        if (ch == NCKEY_ENTER || ch == '\n' || ch == '\r') {
            runDryRun();
            return true;
        }
        return false;
    }
    }

    return false;
}

// ════════════════════════════════════════════════════════════
// Tick
// ════════════════════════════════════════════════════════════

bool OrganizeScreen::tick()
{
    if (m_task.running()) {
        int done  = m_taskDone.load();
        int total = m_taskTotal.load();
        if (total > 0) {
            char label[128];
            snprintf(label, sizeof(label), "Organizing… %d/%d", done, total);
            m_progressBar.set(done, total, label);
        }
        return true; // keep ticking
    }
    return false;
}

// ════════════════════════════════════════════════════════════
// Render
// ════════════════════════════════════════════════════════════

void OrganizeScreen::render(struct notcurses *nc)
{
    unsigned rows = m_app.rows();
    unsigned cols = m_app.cols();
    ncplane *std = notcurses_stdplane(nc);

    const int headerH  = 4;  // title + dest + tmpl + separator
    const int footerH  = 1;
    const int progressH = 2;
    auto layout = m_splitPane.compute(cols, rows, headerH, footerH, progressH);
    m_lastLayout = layout;

    drawHeader(std, cols);

    // ── File list ─────────────────────────────────────────────
    {
        std::lock_guard<std::mutex> lock(m_entriesMutex);
        m_fileList.setCount(static_cast<int>(m_entries.size()));
    }
    m_fileList.ensureVisible(layout.bodyH / 2);

    bool listFocused = (m_focus == Focus::FileList);

    {
        std::lock_guard<std::mutex> lock(m_entriesMutex);
        char hdr[128];
        snprintf(hdr, sizeof(hdr), " Files (%d confirmed matches)",
                 static_cast<int>(m_entries.size()));
        m_fileList.renderHeader(std, layout.bodyY, 0, hdr, listFocused);
    }

    {
        std::lock_guard<std::mutex> lock(m_entriesMutex);
        m_fileList.render(std, layout.bodyY + 1, layout.bodyH - 1, listFocused,
            [this, listFocused, listW = layout.leftW]
            (ncplane *plane, int y, int idx, bool selected, bool /*focused*/) {
                if (idx < 0 || idx >= static_cast<int>(m_entries.size())) return;
                const auto &e = m_entries[idx];

                // ── Row 1: icon + confidence + filename ───────
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

                    const char *icon = statusIcon(e.status).c_str();
                    ncplane_putstr_yx(plane, y, 1, icon);

                    char conf[8];
                    snprintf(conf, sizeof(conf), "%3d%% ", e.confidence);
                    ncplane_putstr(plane, conf);

                    int maxNameW = listW - 8;
                    std::string name = e.filename;
                    if (static_cast<int>(name.size()) > maxNameW)
                        name = name.substr(0, static_cast<size_t>(maxNameW - 3)) + "...";
                    ncplane_putstr(plane, name.c_str());

                    ncplane_set_styles(plane, NCSTYLE_NONE);
                    ch = 0;
                    ncplane_set_channels(plane, ch);
                }

                // ── Row 2: title + status ─────────────────────
                {
                    // Build a combined channel: status color fg + selection bg
                    uint64_t scCh = 0;
                    if (selected && listFocused)
                        ncchannels_set_bg_rgb8(&scCh, 0x22, 0x44, 0x66);
                    else if (selected)
                        ncchannels_set_bg_rgb8(&scCh, 0x22, 0x22, 0x33);
                    switch (e.status) {
                    case EntryStatus::Pending:  ncchannels_set_fg_rgb8(&scCh, 0x77, 0x77, 0x77); break;
                    case EntryStatus::Preview:  ncchannels_set_fg_rgb8(&scCh, 0x88, 0xCC, 0xFF); break;
                    case EntryStatus::OK:       ncchannels_set_fg_rgb8(&scCh, 0x44, 0xCC, 0x44); break;
                    case EntryStatus::Skipped:  ncchannels_set_fg_rgb8(&scCh, 0x88, 0x88, 0x44); break;
                    case EntryStatus::Error:    ncchannels_set_fg_rgb8(&scCh, 0xFF, 0x44, 0x44); break;
                    }
                    ncplane_set_channels(plane, scCh);

                    char detail[256];
                    std::string statusLabel;
                    switch (e.status) {
                    case EntryStatus::Pending:  statusLabel = "pending";  break;
                    case EntryStatus::Preview:  statusLabel = "preview";  break;
                    case EntryStatus::OK:       statusLabel = "done";     break;
                    case EntryStatus::Skipped:  statusLabel = "skipped";  break;
                    case EntryStatus::Error:    statusLabel = "error";    break;
                    }
                    snprintf(detail, sizeof(detail), "    %s — [%s]",
                             e.title.c_str(), statusLabel.c_str());
                    if (static_cast<int>(strlen(detail)) > listW - 1)
                        detail[listW - 1] = '\0';
                    ncplane_putstr_yx(plane, y + 1, 0, detail);
                    ncplane_set_channels(plane, 0);
                }
            });

        if (m_entries.empty()) {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
            ncplane_set_channels(std, ch);
            ncplane_putstr_yx(std, layout.bodyY + 2, 2,
                              "No confirmed matches. Use Match screen (m) to confirm matches first.");
            ncplane_set_channels(std, 0);
        }
    }

    drawDetailPane(std, layout.bodyY, layout.bodyH,
                   layout.rightX, static_cast<unsigned>(layout.rightW));
    m_splitPane.renderSeparator(std, layout);
    m_progressBar.render(std, layout.progressY, cols);
    drawFooter(std, rows, cols);

    ncplane_set_channels(std, 0);
    ncplane_set_styles(std, NCSTYLE_NONE);
}

void OrganizeScreen::onResize(struct notcurses *)
{
}

void OrganizeScreen::forceRefresh()
{
    loadFromDatabase();
    if (!m_entries.empty())
        runDryRun();
}

// ════════════════════════════════════════════════════════════
// Render helpers
// ════════════════════════════════════════════════════════════

void OrganizeScreen::drawHeader(ncplane *plane, unsigned cols) const
{
    // Row 0: screen name + entry count
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        ncplane_putstr_yx(plane, 0, 1, "ORGANIZE");
        ncplane_set_styles(plane, NCSTYLE_NONE);

        int n;
        {
            std::lock_guard<std::mutex> lk(m_entriesMutex);
            n = static_cast<int>(m_entries.size());
        }
        char info[64];
        snprintf(info, sizeof(info), " %d confirmed matches", n);
        uint64_t ci = 0;
        ncchannels_set_fg_rgb8(&ci, 0x88, 0x88, 0x88);
        ncplane_set_channels(plane, ci);
        ncplane_putstr(plane, info);

        // Op mode badge (right side)
        std::string badge = " [" + formatOpMode(m_opMode) + "] ";
        int bx = static_cast<int>(cols) - static_cast<int>(badge.size()) - 1;
        uint64_t bc = 0;
        ncchannels_set_fg_rgb8(&bc, 0xFF, 0xCC, 0x00);
        ncplane_set_channels(plane, bc);
        ncplane_putstr_yx(plane, 0, bx, badge.c_str());

        ncplane_set_channels(plane, 0);
    }

    // Row 1: Dest input
    {
        int fieldW = static_cast<int>(cols) - 2;
        m_destInput.render(plane, 1, 1, fieldW, m_focus == Focus::DestInput);
    }

    // Row 2: Template input
    {
        int fieldW = static_cast<int>(cols) - 2;
        m_templInput.render(plane, 2, 1, fieldW, m_focus == Focus::TemplInput);
    }

    // Row 3: thin separator
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x33, 0x33, 0x33);
        ncplane_set_channels(plane, ch);
        for (unsigned x = 0; x < cols; ++x)
            ncplane_putchar_yx(plane, 3, static_cast<int>(x), '-');
        ncplane_set_channels(plane, 0);
    }
}

void OrganizeScreen::drawDetailPane(ncplane *plane, int startY, int height,
                                    int startX, unsigned width) const
{
    int sel = m_fileList.selected();
    std::lock_guard<std::mutex> lock(m_entriesMutex);

    if (sel < 0 || sel >= static_cast<int>(m_entries.size())) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x55, 0x55, 0x55);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, startY + 1, startX + 1, "No selection");
        ncplane_set_channels(plane, 0);
        return;
    }

    const auto &e = m_entries[static_cast<size_t>(sel)];

    auto put = [&](int row, const char *label, const std::string &value, bool highlight = false) {
        if (row >= startY + height) return;
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, row, startX + 1, label);

        uint64_t vc = 0;
        if (highlight) ncchannels_set_fg_rgb8(&vc, 0xFF, 0xCC, 0x00);
        else           ncchannels_set_fg_rgb8(&vc, 0xCC, 0xCC, 0xCC);
        ncplane_set_channels(plane, vc);

        int maxW = static_cast<int>(width) - static_cast<int>(strlen(label)) - 2;
        std::string val = value;
        if (static_cast<int>(val.size()) > maxW && maxW > 3)
            val = val.substr(0, static_cast<size_t>(maxW - 3)) + "...";
        ncplane_putstr(plane, val.c_str());
    };

    int y = startY + 1;
    put(y++, "Title:    ", e.title);
    put(y++, "System:   ", e.system);
    put(y++, "Region:   ", e.region);
    put(y++, "Confid:   ", std::to_string(e.confidence) + "%");
    ++y;
    put(y++, "Old: ", e.oldPath.empty() ? "(none)" : e.oldPath, false);
    put(y++, "New: ", e.newPath.empty() ? "(run preview first)" : e.newPath, !e.newPath.empty());

    if (!e.errorMsg.empty()) {
        ++y;
        uint64_t ec = 0;
        ncchannels_set_fg_rgb8(&ec, 0xFF, 0x44, 0x44);
        ncplane_set_channels(plane, ec);
        int maxW = static_cast<int>(width) - 3;
        std::string err = "! " + e.errorMsg;
        if (static_cast<int>(err.size()) > maxW) err = err.substr(0, static_cast<size_t>(maxW));
        ncplane_putstr_yx(plane, y, startX + 1, err.c_str());
    }

    ncplane_set_channels(plane, 0);
}

void OrganizeScreen::drawFooter(ncplane *plane, unsigned rows, unsigned cols) const
{
    const char *hint = "Tab:focus  p:preview  e:execute  m:move/copy  n:No-Intro  r:Redump  Esc:back";
    uint64_t ch = 0;
    ncchannels_set_fg_rgb8(&ch, 0x55, 0x55, 0x55);
    ncplane_set_channels(plane, ch);
    int len = static_cast<int>(strlen(hint));
    int x   = std::max(0, (static_cast<int>(cols) - len) / 2);
    ncplane_putstr_yx(plane, static_cast<int>(rows) - 1, x, hint);
    ncplane_set_channels(plane, 0);
}

// ════════════════════════════════════════════════════════════
// Keybindings
// ════════════════════════════════════════════════════════════

std::vector<std::pair<std::string, std::string>> OrganizeScreen::keybindings() const
{
    return {
        {"Tab",   "cycle focus"},
        {"p",     "preview (dry-run)"},
        {"e",     "execute organize"},
        {"m",     "toggle move/copy"},
        {"n",     "No-Intro template"},
        {"r",     "Redump template"},
        {"j/k",   "navigate list"},
        {"Esc",   "back to menu"},
    };
}

// ════════════════════════════════════════════════════════════
// Static helpers
// ════════════════════════════════════════════════════════════

std::string OrganizeScreen::statusIcon(EntryStatus s)
{
    switch (s) {
    case EntryStatus::Pending: return "  ";
    case EntryStatus::Preview: return "→ ";
    case EntryStatus::OK:      return "✓ ";
    case EntryStatus::Skipped: return "- ";
    case EntryStatus::Error:   return "✗ ";
    }
    return "  ";
}

void OrganizeScreen::setStatusColor(ncplane *plane, EntryStatus s)
{
    uint64_t ch = 0;
    switch (s) {
    case EntryStatus::Pending:
        ncchannels_set_fg_rgb8(&ch, 0x77, 0x77, 0x77); break;
    case EntryStatus::Preview:
        ncchannels_set_fg_rgb8(&ch, 0x88, 0xCC, 0xFF); break;
    case EntryStatus::OK:
        ncchannels_set_fg_rgb8(&ch, 0x44, 0xCC, 0x44); break;
    case EntryStatus::Skipped:
        ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x44); break;
    case EntryStatus::Error:
        ncchannels_set_fg_rgb8(&ch, 0xFF, 0x44, 0x44); break;
    }
    ncplane_set_channels(plane, ch);
}
