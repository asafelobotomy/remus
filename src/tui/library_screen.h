#pragma once

#include "screen.h"
#include "widgets/text_input.h"
#include "widgets/selectable_list.h"
#include "widgets/split_pane.h"

#include <string>
#include <vector>
#include <mutex>

/**
 * @brief Library screen — browse, filter, and manage ROM library from database.
 *
 * Layout:
 *   ┌──────────────────────────────────────────────────────┐
 *   │  LIBRARY          Filter: [system filter]    REMUS  │
 *   │  Stats: 245 files, 12 systems, 180 matched          │
 *   ├──────────────────────────┬───────────────────────────┤
 *   │  File list (by system)   │  File details / match     │
 *   │  ▸ NES (45)              │  Title: Game Name         │
 *   │    file1.nes   match ✓   │  System: NES              │
 *   │    file2.nes   match ?   │  Developer: ...           │
 *   │  ▸ SNES (30)             │  Confidence: 95%          │
 *   ├──────────────────────────┴───────────────────────────┤
 *   │  [f]ilter  [r]efresh  [c]onfirm  [x]reject          │
 *   └──────────────────────────────────────────────────────┘
 */
class LibraryScreen : public Screen {
public:
    explicit LibraryScreen(TuiApp &app);
    ~LibraryScreen() override = default;

    void    onEnter() override;
    bool    handleInput(struct notcurses *nc, const ncinput &ni, int ch) override;
    void    render(struct notcurses *nc) override;
    void    onResize(struct notcurses *nc) override;
    bool    tick() override;
    std::string name() const override { return "Library"; }
    std::vector<std::pair<std::string, std::string>> keybindings() const override;
    void forceRefresh() override;

    // ── Public query API (for tests and external consumers) ──
    enum class ConfirmationStatus { Pending, Confirmed, Rejected };

    struct FileEntry {
        int      fileId = 0;
        std::string filename;
        std::string system;
        std::string hash;
        std::string matchStatus;
        int      confidence = 0;
        bool     isHeader = false;   // System group header (not a real file)
        ConfirmationStatus confirmStatus = ConfirmationStatus::Pending;

        // Detail info
        std::string title;
        std::string developer;
        std::string publisher;
        std::string description;
        std::string region;
        std::string matchMethod;
        std::string path;
    };

    int entryCount() const { return static_cast<int>(m_entries.size()); }
    int allEntryCount() const { return static_cast<int>(m_allEntries.size()); }
    const FileEntry& entryAt(int i) const { return m_entries.at(static_cast<size_t>(i)); }
    const FileEntry& allEntryAt(int i) const { return m_allEntries.at(static_cast<size_t>(i)); }
    int fileCount() const { return m_totalFiles; }
    int systemCount() const { return m_totalSystems; }
    int matchedCount() const { return m_totalMatched; }
    std::string filterText() const { return m_filterInput.value(); }

    // ── Actions (public for testability) ────────────────────
    void loadFromDatabase();
    void applyFilter();
    void confirmMatch();
    void rejectMatch();
    void setFilter(const std::string &f) { m_filterInput.setValue(f); }
    void clearFilter() { m_filterInput.clear(); }
    void setSelectedIndex(int i) { m_fileList.setSelected(i); }

private:
    enum class Focus { FilterInput, FileList, DetailPane };
    Focus         m_focus = Focus::FileList;

    // Stats
    int m_totalFiles = 0;
    int m_totalSystems = 0;
    int m_totalMatched = 0;

    // ── Widgets ────────────────────────────────────────────
    TextInput      m_filterInput{"Filter: ", "(all systems)"};
    SelectableList m_fileList;
    SplitPane      m_splitPane;
    SplitPane::Layout m_lastLayout{};

    // ── File data ──────────────────────────────────────────
    std::vector<FileEntry> m_allEntries;    // full unfiltered dataset
    std::vector<FileEntry> m_entries;       // filtered (displayed)
    std::mutex             m_mutex;

    // ── Render helpers ─────────────────────────────────────
    void drawHeader(ncplane *plane, unsigned cols);
    void drawDetailPane(ncplane *plane, int startY, int height, int startX, unsigned width);
    void drawFooter(ncplane *plane, unsigned rows, unsigned cols);

    // ── Helpers ────────────────────────────────────────────
    static std::string confidenceIcon(int confidence);
    static void setConfidenceColor(ncplane *plane, int confidence);
};
