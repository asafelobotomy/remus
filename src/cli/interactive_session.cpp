#include "interactive_session.h"

#include <ncurses.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

#include <QSettings>
#include <QStandardPaths>
#include <QDir>

#include "../core/constants/constants.h"
#include "../core/database.h"

namespace Remus::Cli {
namespace {

// ── Color Pairs ─────────────────────────────────────────────────────────────
enum ColorPair {
    CP_BORDER   = 1,
    CP_TITLE    = 2,
    CP_GOOD     = 3,
    CP_WARN     = 4,
    CP_BAD      = 5,
    CP_STATUS   = 6,
    CP_HIGHLIGHT = 7,
    CP_DIM      = 8,
    CP_ACCENT   = 9,
    CP_HEADER   = 10,
};

struct CursesGuard {
    CursesGuard() {
        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        curs_set(0);
        // Mouse support for scroll wheel
        mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);
        // Colors
        if (has_colors()) {
            start_color();
            use_default_colors();
            init_pair(CP_BORDER,    COLOR_CYAN,   -1);
            init_pair(CP_TITLE,     COLOR_CYAN,   -1);
            init_pair(CP_GOOD,      COLOR_GREEN,  -1);
            init_pair(CP_WARN,      COLOR_YELLOW, -1);
            init_pair(CP_BAD,       COLOR_RED,    -1);
            init_pair(CP_STATUS,    COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_HIGHLIGHT, COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_DIM,       COLOR_WHITE,  -1);
            init_pair(CP_ACCENT,    COLOR_MAGENTA, -1);
            init_pair(CP_HEADER,    COLOR_BLACK,  COLOR_GREEN);
        }
    }
    ~CursesGuard() {
        endwin();
    }
};

// ── Drawing Helpers ─────────────────────────────────────────────────────────

void drawBox(int y, int x, int h, int w, const std::string &title = {})
{
    attron(COLOR_PAIR(CP_BORDER));
    mvaddch(y, x, ACS_ULCORNER);
    mvaddch(y, x + w - 1, ACS_URCORNER);
    mvaddch(y + h - 1, x, ACS_LLCORNER);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
    for (int i = 1; i < w - 1; i++) {
        mvaddch(y, x + i, ACS_HLINE);
        mvaddch(y + h - 1, x + i, ACS_HLINE);
    }
    for (int i = 1; i < h - 1; i++) {
        mvaddch(y + i, x, ACS_VLINE);
        mvaddch(y + i, x + w - 1, ACS_VLINE);
    }
    if (!title.empty()) {
        std::string t = " " + title + " ";
        attron(A_BOLD);
        mvaddstr(y, x + 2, t.c_str());
        attroff(A_BOLD);
    }
    attroff(COLOR_PAIR(CP_BORDER));
}

void drawHLine(int y, int x, int w)
{
    attron(COLOR_PAIR(CP_BORDER));
    mvaddch(y, x, ACS_LTEE);
    for (int i = 1; i < w - 1; i++)
        mvaddch(y, x + i, ACS_HLINE);
    mvaddch(y, x + w - 1, ACS_RTEE);
    attroff(COLOR_PAIR(CP_BORDER));
}

void drawStatusBar(const std::string &left, const std::string &right = {})
{
    int row = LINES - 1;
    attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
    move(row, 0);
    clrtoeol();
    for (int i = 0; i < COLS; i++) mvaddch(row, i, ' ');
    mvaddstr(row, 1, left.c_str());
    if (!right.empty()) {
        int rx = COLS - static_cast<int>(right.size()) - 2;
        if (rx > static_cast<int>(left.size()) + 2)
            mvaddstr(row, rx, right.c_str());
    }
    attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
}

void drawBanner(int y, int x)
{
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvaddstr(y,     x, " ____  _____ __  __ _   _ ____  ");
    mvaddstr(y + 1, x, "|  _ \\| ____|  \\/  | | | / ___| ");
    mvaddstr(y + 2, x, "| |_) |  _| | |\\/| | | | \\___ \\ ");
    mvaddstr(y + 3, x, "|  _ <| |___| |  | | |_| |___) |");
    mvaddstr(y + 4, x, "|_| \\_\\_____|_|  |_|\\___/|____/ ");
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    attron(COLOR_PAIR(CP_DIM));
    mvaddstr(y + 5, x, "  Retro Game Library Manager");
    attroff(COLOR_PAIR(CP_DIM));
}

// Truncate string to fit width
std::string truncStr(const std::string &s, int maxLen)
{
    if (maxLen <= 0) return "";
    if (static_cast<int>(s.size()) <= maxLen) return s;
    if (maxLen <= 3) return s.substr(0, maxLen);
    return s.substr(0, maxLen - 3) + "...";
}

// Right-pad string to width
std::string padStr(const std::string &s, int width)
{
    if (static_cast<int>(s.size()) >= width) return s.substr(0, width);
    return s + std::string(width - s.size(), ' ');
}


// ── Menu Select ─────────────────────────────────────────────────────────────

struct MenuItem {
    std::string label;
    std::string description;
    int colorPair = 0;
};

int menuSelect(const std::string &title, const std::vector<MenuItem> &items,
               const std::string &statusHints, int initial = 0)
{
    int highlight = initial;

    while (true) {
        erase();
        int maxW = COLS;
        int maxH = LINES;

        // Banner
        int bannerX = (maxW - 34) / 2;
        if (bannerX < 2) bannerX = 2;
        drawBanner(1, bannerX);

        // Box around menu
        int menuY = 8;
        int menuH = static_cast<int>(items.size()) + 4;
        int menuW = maxW - 4;
        if (menuW < 40) menuW = 40;
        if (menuY + menuH + 2 > maxH) menuH = maxH - menuY - 2;
        drawBox(menuY, 1, menuH, menuW, title);

        // Menu items
        int itemY = menuY + 2;
        for (size_t i = 0; i < items.size(); ++i) {
            if (itemY + static_cast<int>(i) >= menuY + menuH - 1) break;
            int row = itemY + static_cast<int>(i);
            bool sel = (static_cast<int>(i) == highlight);

            if (sel) {
                attron(COLOR_PAIR(CP_HIGHLIGHT) | A_BOLD);
                std::string bar(menuW - 4, ' ');
                mvaddstr(row, 3, bar.c_str());
                mvaddstr(row, 3, " \xE2\x96\xB8 "); // ▸
            } else {
                mvaddstr(row, 3, "   ");
            }

            int cp = items[i].colorPair;
            if (!sel && cp) attron(COLOR_PAIR(cp));
            mvaddstr(row, 6, items[i].label.c_str());
            if (!sel && cp) attroff(COLOR_PAIR(cp));

            // Description
            if (!items[i].description.empty()) {
                int descX = 6 + static_cast<int>(items[i].label.size()) + 2;
                int maxDesc = menuW - descX - 2;
                if (maxDesc > 0) {
                    if (!sel) attron(COLOR_PAIR(CP_DIM));
                    mvaddstr(row, descX, truncStr(items[i].description, maxDesc).c_str());
                    if (!sel) attroff(COLOR_PAIR(CP_DIM));
                }
            }

            if (sel) {
                attroff(COLOR_PAIR(CP_HIGHLIGHT) | A_BOLD);
            }
        }

        drawStatusBar(statusHints);
        refresh();

        int ch = getch();
        switch (ch) {
        case KEY_UP:   case 'k':
            if (highlight > 0) highlight--;
            break;
        case KEY_DOWN: case 'j':
            if (highlight < static_cast<int>(items.size()) - 1) highlight++;
            break;
        case KEY_HOME: case 'g':
            highlight = 0;
            break;
        case KEY_END:  case 'G':
            highlight = static_cast<int>(items.size()) - 1;
            break;
        case 10: // Enter
            return highlight;
        case 'q': case 'Q': case 27:
            return static_cast<int>(items.size()) - 1;
        case KEY_MOUSE: {
            MEVENT me;
            if (getmouse(&me) == OK) {
                if (me.bstate & BUTTON4_PRESSED) {
                    if (highlight > 0) highlight--;
                } else if (me.bstate & BUTTON5_PRESSED) {
                    if (highlight < static_cast<int>(items.size()) - 1) highlight++;
                }
            }
            break;
        }
        default:
            break;
        }
    }
}

// ── Text Prompt ─────────────────────────────────────────────────────────────

std::string promptText(const std::string &label, const std::string &def = {},
                       const std::string &hint = {})
{
    erase();
    int w = COLS - 4;
    if (w < 40) w = 40;
    drawBox(2, 1, 8, w, "Input");

    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvaddstr(4, 4, label.c_str());
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

    if (!def.empty()) {
        attron(COLOR_PAIR(CP_DIM));
        std::string defLine = "Default: " + def;
        mvaddstr(5, 4, truncStr(defLine, w - 6).c_str());
        attroff(COLOR_PAIR(CP_DIM));
    }
    if (!hint.empty()) {
        attron(COLOR_PAIR(CP_DIM));
        mvaddstr(6, 4, truncStr(hint, w - 6).c_str());
        attroff(COLOR_PAIR(CP_DIM));
    }

    drawStatusBar("Type value, then Enter  |  Enter = use default  |  Esc = cancel");

    attron(COLOR_PAIR(CP_ACCENT));
    mvaddstr(7, 4, "> ");
    attroff(COLOR_PAIR(CP_ACCENT));

    echo();
    curs_set(1);
    char buf[512];
    move(7, 6);
    int res = getnstr(buf, 511);
    noecho();
    curs_set(0);

    if (res == ERR) return def;
    std::string value(buf);
    return value.empty() ? def : value;
}

// ── Yes/No Prompt ───────────────────────────────────────────────────────────

bool promptYesNo(const std::string &label, bool def)
{
    while (true) {
        erase();
        int w = COLS - 4;
        if (w < 40) w = 40;
        drawBox(2, 1, 6, w, "Confirm");

        attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvaddstr(4, 4, label.c_str());
        attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

        int optY = 5;
        if (def) {
            attron(COLOR_PAIR(CP_GOOD) | A_BOLD);
            mvaddstr(optY, 4, "[Y]");
            attroff(COLOR_PAIR(CP_GOOD) | A_BOLD);
            attron(COLOR_PAIR(CP_DIM));
            mvaddstr(optY, 7, "/n");
            attroff(COLOR_PAIR(CP_DIM));
        } else {
            attron(COLOR_PAIR(CP_DIM));
            mvaddstr(optY, 4, "y/");
            attroff(COLOR_PAIR(CP_DIM));
            attron(COLOR_PAIR(CP_BAD) | A_BOLD);
            mvaddstr(optY, 6, "[N]");
            attroff(COLOR_PAIR(CP_BAD) | A_BOLD);
        }

        drawStatusBar("y = Yes  |  n = No  |  Enter = use default");
        refresh();

        int ch = getch();
        if (ch == '\n' || ch == KEY_ENTER) return def;
        if (ch == 'y' || ch == 'Y') return true;
        if (ch == 'n' || ch == 'N') return false;
    }
}

// ── Choice Prompt ───────────────────────────────────────────────────────────

int promptChoice(const std::string &title, const std::vector<std::string> &options, int def = 0)
{
    std::vector<MenuItem> items;
    for (auto &o : options)
        items.push_back({o, {}, 0});
    return menuSelect(title, items,
        "\xE2\x86\x91\xE2\x86\x93 Navigate  Enter Select  q Cancel", def);
}

// ── Confirmation Screen ─────────────────────────────────────────────────────

bool confirmArgs(const QStringList &args)
{
    erase();
    int w = COLS - 4;
    if (w < 40) w = 40;
    int h = static_cast<int>(args.size()) + 6;
    if (h > LINES - 3) h = LINES - 3;
    drawBox(1, 1, h, w, "Command Summary");

    attron(COLOR_PAIR(CP_DIM));
    mvaddstr(3, 4, "The following command will be executed:");
    attroff(COLOR_PAIR(CP_DIM));

    int row = 5;
    for (const QString &arg : args) {
        if (row >= h) break;
        std::string line = arg.toStdString();

        if (line.size() > 2 && line[0] == '-' && line[1] == '-') {
            attron(COLOR_PAIR(CP_ACCENT));
            mvaddstr(row, 6, truncStr(line, w - 8).c_str());
            attroff(COLOR_PAIR(CP_ACCENT));
        } else if (!line.empty() && (line[0] == '/' || line[0] == '~' || line[0] == '.')) {
            attron(COLOR_PAIR(CP_GOOD));
            mvaddstr(row, 6, truncStr(line, w - 8).c_str());
            attroff(COLOR_PAIR(CP_GOOD));
        } else {
            mvaddstr(row, 6, truncStr(line, w - 8).c_str());
        }
        row++;
    }

    drawStatusBar("Enter = Run  |  q = Cancel");
    refresh();

    int ch = getch();
    return (ch != 'q' && ch != 'Q' && ch != 27);
}

// ── Library Browser ─────────────────────────────────────────────────────────

struct BrowseFile {
    int id;
    std::string filename;
    std::string system;
    bool hashed;
    std::string hashType;
    int matchConfidence;
    std::string matchTitle;
    bool selected;
};

void showBrowser(const std::string &dbPath)
{
    Remus::Database db;
    if (!db.initialize(QString::fromStdString(dbPath))) {
        erase();
        attron(COLOR_PAIR(CP_BAD));
        mvaddstr(LINES / 2, 4, "Cannot open database. Run a scan first.");
        attroff(COLOR_PAIR(CP_BAD));
        drawStatusBar("Press any key to return");
        refresh();
        getch();
        return;
    }

    QList<Remus::FileRecord> files = db.getAllFiles();
    QMap<int, Remus::Database::MatchResult> matches = db.getAllMatches();

    if (files.isEmpty()) {
        erase();
        attron(COLOR_PAIR(CP_WARN));
        mvaddstr(LINES / 2, 4, "Library is empty. Scan a directory first.");
        attroff(COLOR_PAIR(CP_WARN));
        drawStatusBar("Press any key to return");
        refresh();
        getch();
        return;
    }

    // Build display list
    std::vector<BrowseFile> items;
    for (const auto &f : files) {
        if (!f.isPrimary) continue;

        BrowseFile bf;
        bf.id = f.id;
        bf.filename = f.filename.toStdString();
        bf.hashed = f.hashCalculated;
        bf.selected = false;

        auto *sysDef = Remus::Constants::Systems::getSystem(f.systemId);
        bf.system = sysDef ? sysDef->internalName.toStdString() : "Unknown";

        if (f.hashCalculated) {
            if (!f.crc32.isEmpty()) bf.hashType = "crc32";
            else if (!f.md5.isEmpty()) bf.hashType = "md5";
            else if (!f.sha1.isEmpty()) bf.hashType = "sha1";
            else bf.hashType = "yes";
        }

        if (matches.contains(f.id)) {
            auto &m = matches[f.id];
            bf.matchConfidence = static_cast<int>(m.confidence);
            bf.matchTitle = m.gameTitle.toStdString();
        } else {
            bf.matchConfidence = -1;
        }

        items.push_back(bf);
    }

    int highlight = 0;
    int scroll = 0;

    while (true) {
        erase();
        int w = COLS - 2;
        int h = LINES - 2;
        if (w < 60) w = 60;
        drawBox(0, 0, h, w, "Library Browser");

        // Column layout
        int headerY = 2;
        int colSel = 2;
        int colSys = 6;
        int colFile = 20;
        int colHash = w - 30;
        int colMatch = w - 18;
        if (colHash < colFile + 10) colHash = colFile + 10;
        if (colMatch < colHash + 8) colMatch = colHash + 8;

        // Column header bar
        attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
        std::string headerBar(w - 2, ' ');
        mvaddstr(headerY, 1, headerBar.c_str());
        mvaddstr(headerY, colSel, "  ");
        mvaddstr(headerY, colSys, "System");
        mvaddstr(headerY, colFile, "Filename");
        mvaddstr(headerY, colHash, "Hash");
        mvaddstr(headerY, colMatch, "Match");
        attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

        // Visible rows
        int listY = headerY + 1;
        int maxVisible = h - listY - 2;
        if (maxVisible < 1) maxVisible = 1;
        int totalItems = static_cast<int>(items.size());

        // Clamp scroll
        if (highlight < scroll) scroll = highlight;
        if (highlight >= scroll + maxVisible) scroll = highlight - maxVisible + 1;
        if (scroll < 0) scroll = 0;
        if (scroll > totalItems - maxVisible) scroll = totalItems - maxVisible;
        if (scroll < 0) scroll = 0;

        for (int vi = 0; vi < maxVisible && scroll + vi < totalItems; vi++) {
            int idx = scroll + vi;
            int row = listY + vi;
            const auto &item = items[idx];
            bool sel = (idx == highlight);

            if (sel) {
                attron(COLOR_PAIR(CP_HIGHLIGHT));
                std::string bar(w - 2, ' ');
                mvaddstr(row, 1, bar.c_str());
            }

            // Checkbox
            if (item.selected) {
                if (!sel) attron(COLOR_PAIR(CP_GOOD));
                mvaddstr(row, colSel, "[\xE2\x9C\x93]"); // [✓]
                if (!sel) attroff(COLOR_PAIR(CP_GOOD));
            } else {
                mvaddstr(row, colSel, "[ ]");
            }

            // System
            int sysW = colFile - colSys - 1;
            if (!sel) attron(COLOR_PAIR(CP_ACCENT));
            mvaddstr(row, colSys, truncStr(item.system, sysW).c_str());
            if (!sel) attroff(COLOR_PAIR(CP_ACCENT));

            // Filename
            int fnW = colHash - colFile - 1;
            mvaddstr(row, colFile, truncStr(item.filename, fnW).c_str());

            // Hash status
            if (item.hashed) {
                if (!sel) attron(COLOR_PAIR(CP_GOOD));
                std::string hs = std::string("\xE2\x9C\x93 ") + item.hashType; // ✓
                mvaddstr(row, colHash, hs.c_str());
                if (!sel) attroff(COLOR_PAIR(CP_GOOD));
            } else {
                if (!sel) attron(COLOR_PAIR(CP_BAD));
                mvaddstr(row, colHash, "\xE2\x9C\x97"); // ✗
                if (!sel) attroff(COLOR_PAIR(CP_BAD));
            }

            // Match confidence
            if (item.matchConfidence >= 0) {
                int cp = CP_GOOD;
                if (item.matchConfidence < 60) cp = CP_BAD;
                else if (item.matchConfidence < 90) cp = CP_WARN;

                char mbuf[8];
                snprintf(mbuf, sizeof(mbuf), "%d%%", item.matchConfidence);

                if (!sel) attron(COLOR_PAIR(cp));
                mvaddstr(row, colMatch, mbuf);
                if (!sel) attroff(COLOR_PAIR(cp));

                int titleX = colMatch + static_cast<int>(strlen(mbuf)) + 1;
                int titleW = w - titleX - 2;
                if (titleW > 0 && !item.matchTitle.empty()) {
                    if (!sel) attron(COLOR_PAIR(CP_DIM));
                    mvaddstr(row, titleX, truncStr(item.matchTitle, titleW).c_str());
                    if (!sel) attroff(COLOR_PAIR(CP_DIM));
                }
            } else {
                if (!sel) attron(COLOR_PAIR(CP_DIM));
                mvaddstr(row, colMatch, "\xE2\x80\x94"); // —
                if (!sel) attroff(COLOR_PAIR(CP_DIM));
            }

            if (sel) {
                attroff(COLOR_PAIR(CP_HIGHLIGHT));
            }
        }

        // Scroll indicator
        if (totalItems > maxVisible) {
            attron(COLOR_PAIR(CP_DIM));
            char scrollInfo[32];
            snprintf(scrollInfo, sizeof(scrollInfo), " %d-%d of %d ",
                     scroll + 1, std::min(scroll + maxVisible, totalItems), totalItems);
            mvaddstr(h - 1, w - static_cast<int>(strlen(scrollInfo)) - 2, scrollInfo);
            attroff(COLOR_PAIR(CP_DIM));
        }

        // Status bar
        int selCount = 0;
        for (auto &it : items) if (it.selected) selCount++;
        std::string rightSt = selCount > 0
            ? std::to_string(selCount) + " selected"
            : std::to_string(totalItems) + " files";
        drawStatusBar(
            "Space:Toggle  a:All  n:None  \xE2\x86\x91\xE2\x86\x93/jk:Navigate  q:Back",
            rightSt);

        refresh();

        int ch = getch();
        switch (ch) {
        case KEY_UP: case 'k':
            if (highlight > 0) highlight--;
            break;
        case KEY_DOWN: case 'j':
            if (highlight < totalItems - 1) highlight++;
            break;
        case KEY_HOME: case 'g':
            highlight = 0;
            break;
        case KEY_END: case 'G':
            highlight = totalItems - 1;
            break;
        case KEY_PPAGE:
            highlight -= maxVisible;
            if (highlight < 0) highlight = 0;
            break;
        case KEY_NPAGE:
            highlight += maxVisible;
            if (highlight >= totalItems) highlight = totalItems - 1;
            break;
        case ' ':
            if (highlight >= 0 && highlight < totalItems)
                items[highlight].selected = !items[highlight].selected;
            break;
        case 'a':
            for (auto &it : items) it.selected = true;
            break;
        case 'n':
            for (auto &it : items) it.selected = false;
            break;
        case 'q': case 'Q': case 27: case 10:
            return;
        case KEY_MOUSE: {
            MEVENT me;
            if (getmouse(&me) == OK) {
                if (me.bstate & BUTTON4_PRESSED) {
                    if (highlight > 0) highlight--;
                } else if (me.bstate & BUTTON5_PRESSED) {
                    if (highlight < totalItems - 1) highlight++;
                } else if (me.bstate & BUTTON1_CLICKED) {
                    int clickIdx = scroll + (me.y - listY);
                    if (clickIdx >= 0 && clickIdx < totalItems) highlight = clickIdx;
                } else if (me.bstate & BUTTON1_DOUBLE_CLICKED) {
                    int clickIdx = scroll + (me.y - listY);
                    if (clickIdx >= 0 && clickIdx < totalItems)
                        items[clickIdx].selected = !items[clickIdx].selected;
                }
            }
            break;
        }
        default:
            break;
        }
    }
}

// ── Statistics View ─────────────────────────────────────────────────────────

void showStats(const std::string &dbPath)
{
    Remus::Database db;
    if (!db.initialize(QString::fromStdString(dbPath))) {
        erase();
        attron(COLOR_PAIR(CP_BAD));
        mvaddstr(LINES / 2, 4, "Cannot open database.");
        attroff(COLOR_PAIR(CP_BAD));
        drawStatusBar("Press any key to return");
        refresh();
        getch();
        return;
    }

    QList<Remus::FileRecord> files = db.getAllFiles();
    QMap<int, Remus::Database::MatchResult> matches = db.getAllMatches();
    QMap<QString, int> bySys = db.getFileCountBySystem();

    int totalFiles = files.size();
    int hashed = 0;
    int matched = static_cast<int>(matches.size());
    for (const auto &f : files) {
        if (f.hashCalculated) hashed++;
    }

    erase();
    int w = COLS - 4;
    if (w < 50) w = 50;
    int h = LINES - 3;
    drawBox(0, 1, h, w, "Library Statistics");

    int y = 2;
    int barWidth = 30;
    if (barWidth > w - 40) barWidth = w - 40;
    if (barWidth < 10) barWidth = 10;

    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvaddstr(y, 4, "Overview");
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    y += 2;

    // Total files
    attron(COLOR_PAIR(CP_DIM));
    mvaddstr(y, 4, "Total Files:");
    attroff(COLOR_PAIR(CP_DIM));
    char buf[64];
    attron(A_BOLD);
    snprintf(buf, sizeof(buf), "%d", totalFiles);
    mvaddstr(y, 18, buf);
    attroff(A_BOLD);

    // Hashed bar
    y++;
    attron(COLOR_PAIR(CP_DIM));
    mvaddstr(y, 4, "Hashed:");
    attroff(COLOR_PAIR(CP_DIM));
    {
        int filled = totalFiles > 0 ? (hashed * barWidth) / totalFiles : 0;
        move(y, 18);
        attron(COLOR_PAIR(CP_GOOD));
        for (int i = 0; i < filled; i++) addch(ACS_CKBOARD);
        attroff(COLOR_PAIR(CP_GOOD));
        attron(COLOR_PAIR(CP_DIM));
        for (int i = filled; i < barWidth; i++) addch(ACS_BULLET);
        attroff(COLOR_PAIR(CP_DIM));
        snprintf(buf, sizeof(buf), " %d/%d", hashed, totalFiles);
        addstr(buf);
        int pct = totalFiles > 0 ? (hashed * 100) / totalFiles : 0;
        char pctBuf[16];
        snprintf(pctBuf, sizeof(pctBuf), " (%d%%)", pct);
        attron(COLOR_PAIR(pct >= 90 ? CP_GOOD : (pct >= 50 ? CP_WARN : CP_BAD)));
        addstr(pctBuf);
        attroff(COLOR_PAIR(pct >= 90 ? CP_GOOD : (pct >= 50 ? CP_WARN : CP_BAD)));
    }

    // Matched bar
    y++;
    attron(COLOR_PAIR(CP_DIM));
    mvaddstr(y, 4, "Matched:");
    attroff(COLOR_PAIR(CP_DIM));
    {
        int filled = totalFiles > 0 ? (matched * barWidth) / totalFiles : 0;
        move(y, 18);
        attron(COLOR_PAIR(CP_GOOD));
        for (int i = 0; i < filled; i++) addch(ACS_CKBOARD);
        attroff(COLOR_PAIR(CP_GOOD));
        attron(COLOR_PAIR(CP_DIM));
        for (int i = filled; i < barWidth; i++) addch(ACS_BULLET);
        attroff(COLOR_PAIR(CP_DIM));
        snprintf(buf, sizeof(buf), " %d/%d", matched, totalFiles);
        addstr(buf);
        int pct = totalFiles > 0 ? (matched * 100) / totalFiles : 0;
        char pctBuf[16];
        snprintf(pctBuf, sizeof(pctBuf), " (%d%%)", pct);
        attron(COLOR_PAIR(pct >= 90 ? CP_GOOD : (pct >= 50 ? CP_WARN : CP_BAD)));
        addstr(pctBuf);
        attroff(COLOR_PAIR(pct >= 90 ? CP_GOOD : (pct >= 50 ? CP_WARN : CP_BAD)));
    }

    // Systems breakdown
    y += 2;
    drawHLine(y, 1, w);
    y++;
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvaddstr(y, 4, "By System");
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    y += 2;

    for (auto it = bySys.constBegin(); it != bySys.constEnd() && y < h - 2; ++it, ++y) {
        std::string sysName = it.key().toStdString();
        int count = it.value();
        int filled = totalFiles > 0 ? (count * barWidth) / totalFiles : 0;
        if (filled < 1 && count > 0) filled = 1;

        attron(COLOR_PAIR(CP_ACCENT));
        mvaddstr(y, 4, padStr(sysName, 14).c_str());
        attroff(COLOR_PAIR(CP_ACCENT));

        snprintf(buf, sizeof(buf), "%3d ", count);
        mvaddstr(y, 18, buf);

        int bx = 22;
        attron(COLOR_PAIR(CP_GOOD));
        for (int i = 0; i < filled; i++) mvaddch(y, bx + i, ACS_CKBOARD);
        attroff(COLOR_PAIR(CP_GOOD));
        attron(COLOR_PAIR(CP_DIM));
        for (int i = filled; i < barWidth; i++) mvaddch(y, bx + i, ACS_BULLET);
        attroff(COLOR_PAIR(CP_DIM));
    }

    drawStatusBar("Press any key to return");
    refresh();
    getch();
}

} // namespace


// ── State Persistence ───────────────────────────────────────────────────────

SessionState InteractiveSession::loadState()
{
    QSettings settings("Remus", "CLI");
    SessionState state;
    state.lastScanPath = settings.value("scanPath").toString();
    state.lastDoHash = settings.value("doHash", true).toBool();
    state.lastDoMatch = settings.value("doMatch", true).toBool();
    state.lastDoOrganize = settings.value("doOrganize", false).toBool();
    state.lastOrganizeDest = settings.value("organizeDest").toString();
    state.lastDryRun = settings.value("dryRun", false).toBool();
    state.lastChdInput = settings.value("chdInput").toString();
    state.lastChdOutputDir = settings.value("chdOutputDir").toString();
    state.lastChdCodec = settings.value("chdCodec", "auto").toString();
    state.lastArchivePath = settings.value("archivePath").toString();
    state.lastArchiveOut = settings.value("archiveOut").toString();
    state.lastPatchBase = settings.value("patchBase").toString();
    state.lastPatchFile = settings.value("patchFile").toString();
    state.lastPatchOutput = settings.value("patchOutput").toString();
    state.lastPatchOriginal = settings.value("patchOriginal").toString();
    state.lastPatchModified = settings.value("patchModified").toString();
    state.lastPatchFormat = settings.value("patchFormat", "bps").toString();
    state.lastExportFormat = settings.value("exportFormat", "csv").toString();
    state.lastExportPath = settings.value("exportPath").toString();
    state.lastExportSystems = settings.value("exportSystems").toString();
    state.lastExportDryRun = settings.value("exportDryRun", true).toBool();
    state.lastTemplate = settings.value("template", Constants::Templates::DEFAULT_NO_INTRO).toString();
    return state;
}

void InteractiveSession::saveState(const SessionState &state)
{
    QSettings settings("Remus", "CLI");
    settings.setValue("scanPath", state.lastScanPath);
    settings.setValue("doHash", state.lastDoHash);
    settings.setValue("doMatch", state.lastDoMatch);
    settings.setValue("doOrganize", state.lastDoOrganize);
    settings.setValue("organizeDest", state.lastOrganizeDest);
    settings.setValue("dryRun", state.lastDryRun);
    settings.setValue("chdInput", state.lastChdInput);
    settings.setValue("chdOutputDir", state.lastChdOutputDir);
    settings.setValue("chdCodec", state.lastChdCodec);
    settings.setValue("archivePath", state.lastArchivePath);
    settings.setValue("archiveOut", state.lastArchiveOut);
    settings.setValue("patchBase", state.lastPatchBase);
    settings.setValue("patchFile", state.lastPatchFile);
    settings.setValue("patchOutput", state.lastPatchOutput);
    settings.setValue("patchOriginal", state.lastPatchOriginal);
    settings.setValue("patchModified", state.lastPatchModified);
    settings.setValue("patchFormat", state.lastPatchFormat);
    settings.setValue("exportFormat", state.lastExportFormat);
    settings.setValue("exportPath", state.lastExportPath);
    settings.setValue("exportSystems", state.lastExportSystems);
    settings.setValue("exportDryRun", state.lastExportDryRun);
    settings.setValue("template", state.lastTemplate);
}

SessionState InteractiveSession::loadStateSnapshot()
{
    InteractiveSession s;
    return s.loadState();
}

void InteractiveSession::saveStateSnapshot(const SessionState &state)
{
    InteractiveSession s;
    s.saveState(state);
}


// ── Main Entry Point ────────────────────────────────────────────────────────

InteractiveResult InteractiveSession::run()
{
    CursesGuard guard;
    InteractiveResult result;
    SessionState state = loadState();

    std::string dbPath = Constants::DATABASE_FILENAME;

    const std::vector<MenuItem> mainMenu = {
        {"Pipeline: scan \xE2\x86\x92 hash \xE2\x86\x92 match \xE2\x86\x92 organize",
         "Full processing pipeline", CP_GOOD},
        {"Browse Library",       "View scanned ROMs with match status", CP_TITLE},
        {"Library Statistics",   "Dashboard with hash/match coverage",  CP_TITLE},
        {"Organize Only",       "Rename and sort existing library",     0},
        {"Export Library",      "RetroArch, LaunchBox, CSV, JSON",      0},
        {"Convert to CHD",      "Compress disc images",                 0},
        {"Extract CHD",         "Decompress CHD to BIN/CUE",           0},
        {"Extract Archive",     "Unpack ZIP/7z/RAR",                    0},
        {"Apply Patch",         "IPS, BPS, UPS, xdelta, PPF",          0},
        {"Create Patch",        "Generate patch from modified ROM",     0},
        {"Quit",                "",                                     CP_BAD},
    };

    while (true) {
        int choice = menuSelect("Main Menu", mainMenu,
            "\xE2\x86\x91\xE2\x86\x93/jk Navigate  Enter Select  q Quit");

        switch (choice) {
        case 0: { // Pipeline
            std::string scanPath = promptText("Scan directory", state.lastScanPath.toStdString(),
                                               "Path to ROM library folder");
            if (scanPath.empty()) break;
            bool doHash = promptYesNo("Calculate hashes?", state.lastDoHash);
            bool doMatch = promptYesNo("Match metadata from providers?", state.lastDoMatch);
            bool doOrganize = promptYesNo("Organize & rename output?", state.lastDoOrganize);
            std::string organizeDest;
            if (doOrganize) {
                organizeDest = promptText("Organize destination", state.lastOrganizeDest.toStdString(),
                                           "Output folder for organized ROM library");
                if (organizeDest.empty()) doOrganize = false;
            }
            bool dryRun = promptYesNo("Dry run (preview only, no file changes)?", state.lastDryRun);

            result.args << "remus-cli";
            result.args << "--scan" << QString::fromStdString(scanPath);
            if (doHash) result.args << "--hash";
            if (doMatch) result.args << "--match";
            if (doOrganize) result.args << "--organize" << QString::fromStdString(organizeDest);
            if (dryRun) result.args << "--dry-run-all";
            result.valid = confirmArgs(result.args);
            if (result.valid) {
                state.lastScanPath = QString::fromStdString(scanPath);
                state.lastDoHash = doHash;
                state.lastDoMatch = doMatch;
                state.lastDoOrganize = doOrganize;
                state.lastOrganizeDest = QString::fromStdString(organizeDest);
                state.lastDryRun = dryRun;
                saveState(state);
                return result;
            }
            result.args.clear();
            break;
        }
        case 1:  // Browse Library
            showBrowser(dbPath);
            break;
        case 2:  // Library Statistics
            showStats(dbPath);
            break;
        case 3: { // Organize only
            std::string dest = promptText("Organize destination", state.lastOrganizeDest.toStdString());
            if (dest.empty()) break;
            std::string tpl = promptText("Naming template (blank = default)", state.lastTemplate.toStdString());
            bool dryRun = promptYesNo("Dry run?", true);

            result.args << "remus-cli";
            result.args << "--organize" << QString::fromStdString(dest);
            if (!tpl.empty()) result.args << "--template" << QString::fromStdString(tpl);
            if (dryRun) result.args << "--dry-run-all";
            result.valid = confirmArgs(result.args);
            if (result.valid) {
                state.lastOrganizeDest = QString::fromStdString(dest);
                state.lastTemplate = QString::fromStdString(tpl);
                state.lastDryRun = dryRun;
                saveState(state);
                return result;
            }
            result.args.clear();
            break;
        }
        case 4: { // Export
            const std::vector<std::string> formats = {"retroarch", "emustation", "launchbox", "csv", "json"};
            int fmtIndex = 0;
            for (size_t i = 0; i < formats.size(); ++i)
                if (state.lastExportFormat.toStdString() == formats[i]) fmtIndex = static_cast<int>(i);
            int fmtChoice = promptChoice("Export format", formats, fmtIndex);
            std::string exportPath = promptText("Export output path", state.lastExportPath.toStdString());
            std::string systems = promptText("Systems (comma-separated, blank = all)", state.lastExportSystems.toStdString());
            bool dryRun = promptYesNo("Dry run?", state.lastExportDryRun);

            result.args << "remus-cli";
            result.args << "--export" << QString::fromStdString(formats[fmtChoice]);
            if (!exportPath.empty()) result.args << "--export-path" << QString::fromStdString(exportPath);
            if (!systems.empty()) result.args << "--export-systems" << QString::fromStdString(systems);
            if (dryRun) result.args << "--dry-run-all";
            result.valid = confirmArgs(result.args);
            if (result.valid) {
                state.lastExportFormat = QString::fromStdString(formats[fmtChoice]);
                state.lastExportPath = QString::fromStdString(exportPath);
                state.lastExportSystems = QString::fromStdString(systems);
                state.lastExportDryRun = dryRun;
                saveState(state);
                return result;
            }
            result.args.clear();
            break;
        }
        case 5: { // Convert to CHD
            std::string input = promptText("Input disc image (cue/iso/img/gdi)");
            if (input.empty()) break;
            std::string outputDir = promptText("Output directory (blank = alongside)");
            int codecChoice = promptChoice("CHD codec", {"auto", "lzma", "zlib", "flac", "huff"}, 0);
            const std::vector<std::string> codecs = {"auto", "lzma", "zlib", "flac", "huff"};
            bool dryRun = promptYesNo("Dry run?", true);

            result.args << "remus-cli";
            result.args << "--convert-chd" << QString::fromStdString(input);
            result.args << "--chd-codec" << QString::fromStdString(codecs[codecChoice]);
            if (!outputDir.empty()) result.args << "--output-dir" << QString::fromStdString(outputDir);
            if (dryRun) result.args << "--dry-run-all";
            result.valid = confirmArgs(result.args);
            if (result.valid) {
                state.lastChdInput = QString::fromStdString(input);
                state.lastChdOutputDir = QString::fromStdString(outputDir);
                state.lastChdCodec = QString::fromStdString(codecs[codecChoice]);
                state.lastDryRun = dryRun;
                saveState(state);
                return result;
            }
            result.args.clear();
            break;
        }
        case 6: { // Extract CHD
            std::string chd = promptText("CHD file to extract");
            if (chd.empty()) break;
            std::string outDir = promptText("Output directory (blank = alongside)");
            bool dryRun = promptYesNo("Dry run?", true);

            result.args << "remus-cli";
            result.args << "--chd-extract" << QString::fromStdString(chd);
            if (!outDir.empty()) result.args << "--output-dir" << QString::fromStdString(outDir);
            if (dryRun) result.args << "--dry-run-all";
            result.valid = confirmArgs(result.args);
            if (result.valid) {
                state.lastChdInput = QString::fromStdString(chd);
                state.lastChdOutputDir = QString::fromStdString(outDir);
                state.lastDryRun = dryRun;
                saveState(state);
                return result;
            }
            result.args.clear();
            break;
        }
        case 7: { // Extract archive
            std::string archive = promptText("Archive file (zip/7z/rar)");
            if (archive.empty()) break;
            std::string outDir = promptText("Output directory (blank = alongside)");
            bool dryRun = promptYesNo("Dry run?", true);

            result.args << "remus-cli";
            result.args << "--extract-archive" << QString::fromStdString(archive);
            if (!outDir.empty()) result.args << "--output-dir" << QString::fromStdString(outDir);
            if (dryRun) result.args << "--dry-run-all";
            result.valid = confirmArgs(result.args);
            if (result.valid) {
                state.lastArchivePath = QString::fromStdString(archive);
                state.lastArchiveOut = QString::fromStdString(outDir);
                state.lastDryRun = dryRun;
                saveState(state);
                return result;
            }
            result.args.clear();
            break;
        }
        case 8: { // Apply patch
            std::string base = promptText("Base/original file");
            if (base.empty()) break;
            std::string patch = promptText("Patch file (ips/bps/ups/xdelta/ppf)");
            if (patch.empty()) break;
            std::string output = promptText("Output file (blank = auto)");
            bool dryRun = promptYesNo("Dry run?", true);

            result.args << "remus-cli";
            result.args << "--patch-apply" << QString::fromStdString(base);
            result.args << "--patch-patch" << QString::fromStdString(patch);
            if (!output.empty()) result.args << "--patch-output" << QString::fromStdString(output);
            if (dryRun) result.args << "--dry-run-all";
            result.valid = confirmArgs(result.args);
            if (result.valid) {
                state.lastPatchBase = QString::fromStdString(base);
                state.lastPatchFile = QString::fromStdString(patch);
                state.lastPatchOutput = QString::fromStdString(output);
                state.lastDryRun = dryRun;
                saveState(state);
                return result;
            }
            result.args.clear();
            break;
        }
        case 9: { // Create patch
            std::string original = promptText("Original file");
            if (original.empty()) break;
            std::string modified = promptText("Modified file");
            if (modified.empty()) break;
            std::string output = promptText("Patch output path (blank = auto)");
            int fmtChoice = promptChoice("Patch format", {"bps", "ips", "ups", "xdelta", "ppf"}, 0);
            const std::vector<std::string> formats = {"bps", "ips", "ups", "xdelta", "ppf"};
            bool dryRun = promptYesNo("Dry run?", true);

            result.args << "remus-cli";
            result.args << "--patch-create" << QString::fromStdString(modified);
            result.args << "--patch-original" << QString::fromStdString(original);
            result.args << "--patch-format" << QString::fromStdString(formats[fmtChoice]);
            if (!output.empty()) result.args << "--patch-patch" << QString::fromStdString(output);
            if (dryRun) result.args << "--dry-run-all";
            result.valid = confirmArgs(result.args);
            if (result.valid) {
                state.lastPatchOriginal = QString::fromStdString(original);
                state.lastPatchModified = QString::fromStdString(modified);
                state.lastPatchOutput = QString::fromStdString(output);
                state.lastPatchFormat = QString::fromStdString(formats[fmtChoice]);
                state.lastDryRun = dryRun;
                saveState(state);
                return result;
            }
            result.args.clear();
            break;
        }
        default: // Quit
            return result;
        }
    }
}

} // namespace Remus::Cli
