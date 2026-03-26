#include "manual_match_overlay.h"
#include "app.h"
#include "../core/database.h"
#include "../metadata/local_database_provider.h"

#include <QCoreApplication>
#include <QDir>
#include <cstring>
#include <algorithm>
#include <notcurses/notcurses.h>

using Remus::SearchResult;

// ════════════════════════════════════════════════════════════
// Construction / Destruction
// ════════════════════════════════════════════════════════════

ManualMatchOverlay::ManualMatchOverlay(TuiApp &app)
    : m_app(app)
{
}

ManualMatchOverlay::~ManualMatchOverlay()
{
    m_task.stop();
}

// ════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════

void ManualMatchOverlay::open(int fileId, const std::string &system, const std::string &title)
{
    m_active       = true;
    m_searching    = false;
    m_targetFileId = fileId;
    m_targetSystem = system;
    m_targetTitle  = title;
    m_results.clear();
    m_statusMsg    = "Press Enter to search";
    m_focus        = OFocus::Search;

    m_search.setValue(title);
    m_list.setCount(0);

    if (!m_providerReady)
        initProvider();
}

void ManualMatchOverlay::close()
{
    m_task.cancel();
    m_active    = false;
    m_searching = false;
}

// ════════════════════════════════════════════════════════════
// Provider init
// ════════════════════════════════════════════════════════════

void ManualMatchOverlay::initProvider()
{
    if (m_provider) {
        m_providerReady = true;
        return;
    }

    m_statusMsg = "Loading search databases…";
    m_provider = std::make_unique<Remus::LocalDatabaseProvider>();

    m_task.start([this]() {
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList candidates = {
            appDir + "/data/databases",
            appDir + "/../data/databases",
            appDir + "/../../data/databases",
            appDir + "/../../../data/databases",
        };
        QString dbDir;
        for (const auto &c : candidates) {
            if (QDir(c).exists()) { dbDir = c; break; }
        }
        if (dbDir.isEmpty()) dbDir = "data/databases";

        int n = m_provider->loadDatabases(dbDir);

        m_app.post([this, n]() {
            m_providerReady = true;
            if (n > 0) {
                m_statusMsg = "Press Enter to search  (" +
                              std::to_string(n) + " entries loaded)";
            } else {
                m_statusMsg = "No DAT files found — results may be empty";
            }
        });
    });
}

// ════════════════════════════════════════════════════════════
// Search
// ════════════════════════════════════════════════════════════

void ManualMatchOverlay::runSearch(const std::string &query)
{
    if (m_searching) return;
    if (!m_providerReady) {
        m_app.toast("Databases still loading, please wait", Toast::Level::Warning);
        return;
    }
    m_searching = true;
    m_statusMsg = "Searching...";
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_results.clear();
    }
    m_list.setCount(0);

    std::string system = m_targetSystem;

    m_task.start([this, query, system]() {
        QList<SearchResult> qres;
        if (m_provider) {
            qres = m_provider->searchByName(
                QString::fromStdString(query),
                QString::fromStdString(system));
        }

        std::vector<SearchResult> res(qres.begin(), qres.end());

        m_app.post([this, res = std::move(res)]() {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_results = res;
                m_searching = false;
                m_statusMsg = res.empty() ? "No results found"
                            : (std::to_string(res.size()) + " result(s) found");
            }
            m_list.setCount(static_cast<int>(m_results.size()));
            if (!m_results.empty()) {
                m_list.setSelected(0);
                m_focus = OFocus::Results;
            }
        });
    });
}

// ════════════════════════════════════════════════════════════
// Apply match
// ════════════════════════════════════════════════════════════

void ManualMatchOverlay::applyMatch(int idx)
{
    std::vector<SearchResult> results;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        results = m_results;
    }
    if (idx < 0 || idx >= static_cast<int>(results.size())) return;

    const SearchResult &sr = results[static_cast<size_t>(idx)];
    const int fileId = m_targetFileId;
    m_active = false;

    auto &db = m_app.db();
    int systemId = db.getSystemId(QString::fromStdString(m_targetSystem));

    int gameId = db.insertGame(
        sr.title,
        systemId,
        sr.region,
        {},     // publisher
        {},     // developer
        sr.releaseYear > 0 ? QString::number(sr.releaseYear) : QString{},
        {},     // description
        {},     // genres
        {},     // players
        sr.matchScore * 10.0f);

    if (gameId > 0)
        db.insertMatch(fileId, gameId, 100.0f, "manual", sr.matchScore);

    m_app.toast("Manual match applied: " + sr.title.toStdString(), Toast::Level::Success);

    if (onApplied)
        onApplied(fileId, gameId, sr.title.toStdString());
}

// ════════════════════════════════════════════════════════════
// Input handling
// ════════════════════════════════════════════════════════════

bool ManualMatchOverlay::handleInput(int ch, const ncinput & /*ni*/)
{
    if (ch == NCKEY_ESC) {
        close();
        return true;
    }

    if (ch == '\t') {
        if (m_focus == OFocus::Search && !m_results.empty())
            m_focus = OFocus::Results;
        else
            m_focus = OFocus::Search;
        return true;
    }

    // ── Search field ───────────────────────────────────────
    if (m_focus == OFocus::Search) {
        if (TextInput::isSubmit(ch)) {
            std::string q = m_search.value();
            if (!q.empty())
                runSearch(q);
            return true;
        }
        m_search.handleInput(ch);
        return true;
    }

    // ── Results list ───────────────────────────────────────
    if (m_focus == OFocus::Results) {
        auto action = m_list.handleInput(ch);
        if (action == SelectableList::Action::Submit) {
            applyMatch(m_list.selected());
            return true;
        }
        if (action != SelectableList::Action::None)
            return true;
    }

    return true;
}

// ════════════════════════════════════════════════════════════
// Rendering
// ════════════════════════════════════════════════════════════

void ManualMatchOverlay::render(ncplane *plane, unsigned rows, unsigned cols)
{
    const int bw = std::min(static_cast<int>(cols) - 4, 80);
    const int bh = std::min(static_cast<int>(rows) - 4, 22);
    const int bx = (static_cast<int>(cols) - bw) / 2;
    const int by = (static_cast<int>(rows) - bh) / 2;

    // Dark background fill
    {
        uint64_t bg = 0;
        ncchannels_set_bg_rgb8(&bg, 0x10, 0x10, 0x18);
        ncchannels_set_fg_rgb8(&bg, 0x22, 0x22, 0x22);
        for (int r = by; r < by + bh; ++r) {
            for (int c = bx; c < bx + bw; ++c) {
                ncplane_set_channels(plane, bg);
                ncplane_putstr_yx(plane, r, c, " ");
            }
        }
    }

    // Border
    {
        uint64_t border = 0;
        ncchannels_set_fg_rgb8(&border, 0x55, 0x88, 0xFF);
        ncchannels_set_bg_rgb8(&border, 0x10, 0x10, 0x18);
        ncplane_set_channels(plane, border);
        for (int c = bx; c < bx + bw; ++c) {
            ncplane_putchar_yx(plane, by,          c, '-');
            ncplane_putchar_yx(plane, by + bh - 1, c, '-');
        }
        for (int r = by; r < by + bh; ++r) {
            ncplane_putchar_yx(plane, r, bx,          '|');
            ncplane_putchar_yx(plane, r, bx + bw - 1, '|');
        }
    }

    // Title bar
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
        ncchannels_set_bg_rgb8(&ch, 0x10, 0x10, 0x18);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        std::string hdr = " Manual Match: ";
        hdr += m_targetTitle.substr(0, static_cast<size_t>(bw - 20));
        if (!m_targetSystem.empty())
            hdr += "  (" + m_targetSystem + ")";
        ncplane_putstr_yx(plane, by, bx + 1, hdr.c_str());
        ncplane_set_styles(plane, NCSTYLE_NONE);
    }

    int y = by + 1;

    // Search input
    {
        bool sfoc = (m_focus == OFocus::Search);
        int fieldW = bw - 4;
        m_search.render(plane, y, bx + 2, fieldW, sfoc);
        y++;
    }

    // Status line
    {
        uint64_t ch = 0;
        if (m_searching)
            ncchannels_set_fg_rgb8(&ch, 0xFF, 0xCC, 0x00);
        else if (m_statusMsg.find("No") != std::string::npos)
            ncchannels_set_fg_rgb8(&ch, 0xCC, 0x44, 0x44);
        else
            ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
        ncchannels_set_bg_rgb8(&ch, 0x10, 0x10, 0x18);
        ncplane_set_channels(plane, ch);
        std::string st = " " + m_statusMsg;
        if (static_cast<int>(st.size()) > bw - 2) st = st.substr(0, static_cast<size_t>(bw - 5)) + "...";
        ncplane_putstr_yx(plane, y++, bx, st.c_str());
        // divider
        uint64_t dc = 0;
        ncchannels_set_fg_rgb8(&dc, 0x33, 0x33, 0x55);
        ncchannels_set_bg_rgb8(&dc, 0x10, 0x10, 0x18);
        ncplane_set_channels(plane, dc);
        std::string div(static_cast<size_t>(bw), '-');
        ncplane_putstr_yx(plane, y++, bx, div.c_str());
    }

    // Results list
    {
        bool rfoc = (m_focus == OFocus::Results);
        int listH = bh - (y - by) - 2;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_list.ensureVisible(listH);
        m_list.render(plane, y, listH, rfoc,
            [this, bx, bw, rfoc](ncplane *pl, int ry, int ridx, bool sel, bool /*foc*/) {
                if (ridx < 0 || ridx >= static_cast<int>(m_results.size())) return;
                const SearchResult &sr = m_results[static_cast<size_t>(ridx)];

                uint64_t ch = 0;
                if (sel && rfoc) {
                    ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
                    ncchannels_set_bg_rgb8(&ch, 0x22, 0x44, 0x66);
                } else {
                    ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
                    ncchannels_set_bg_rgb8(&ch, 0x10, 0x10, 0x18);
                }
                ncplane_set_channels(pl, ch);
                if (sel) ncplane_set_styles(pl, NCSTYLE_BOLD);

                char line[256];
                int year = sr.releaseYear;
                snprintf(line, sizeof(line), "  %s%s%s%s",
                         sr.title.toStdString().c_str(),
                         sr.system.isEmpty() ? "" : (" [" + sr.system + "]").toStdString().c_str(),
                         year > 0 ? (" " + std::to_string(year)).c_str() : "",
                         sr.region.isEmpty() ? "" : (" " + sr.region).toStdString().c_str());
                if (static_cast<int>(strlen(line)) > bw - 2) line[bw - 2] = '\0';
                ncplane_putstr_yx(pl, ry, bx, line);
                ncplane_set_styles(pl, NCSTYLE_NONE);
            });

        if (m_results.empty() && !m_searching) {
            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0x55, 0x55, 0x55);
            ncchannels_set_bg_rgb8(&ch, 0x10, 0x10, 0x18);
            ncplane_set_channels(plane, ch);
            ncplane_putstr_yx(plane, y + 1, bx + 3, "No results. Try a different search query.");
        }
    }

    // Footer hint
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x55, 0x55, 0x55);
        ncchannels_set_bg_rgb8(&ch, 0x10, 0x10, 0x18);
        ncplane_set_channels(plane, ch);
        const char *hint = (m_focus == OFocus::Search)
            ? "Enter:search  Tab:results  Esc:close"
            : "j/k:navigate  Enter:apply match  Tab:search  Esc:close";
        int hx = bx + (bw - static_cast<int>(strlen(hint))) / 2;
        if (hx < bx + 1) hx = bx + 1;
        ncplane_putstr_yx(plane, by + bh - 1, hx, hint);
    }
}
