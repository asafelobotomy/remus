#pragma once

#include "screen.h"
#include "pipeline.h"
#include "widgets/text_input.h"
#include "widgets/selectable_list.h"
#include "widgets/progress_bar.h"
#include "widgets/split_pane.h"

#include <string>
#include <vector>
#include <mutex>
#include <atomic>

/**
 * @brief Match screen — scan → hash → match pipeline with results list.
 *
 * Layout:
 *   ┌──────────────────────────────────────────────────────┐
 *   │  MATCH              [path input]   DIRECTORY  SCAN   │
 *   ├──────────────────────────┬───────────────────────────┤
 *   │  File list (scrollable)  │  Detail / metadata pane   │
 *   │  ☐ Filename              │  Title                    │
 *   │  ☐ Filename              │  System                   │
 *   │    system — hash — match │  Developer                │
 *   │                          │  Description...           │
 *   │                          │  Confidence 100%          │
 *   ├──────────────────────────┴───────────────────────────┤
 *   │  Progress: [####       ] scanning 12/120             │
 *   └──────────────────────────────────────────────────────┘
 */
class MatchScreen : public Screen {
public:
    explicit MatchScreen(TuiApp &app);
    ~MatchScreen() override;

    void    onEnter() override;
    void    onLeave() override;
    bool    handleInput(struct notcurses *nc, const ncinput &ni, int ch) override;
    void    render(struct notcurses *nc) override;
    void    onResize(struct notcurses *nc) override;
    bool    tick() override;
    std::string name() const override { return "Match"; }
    std::vector<std::pair<std::string, std::string>> keybindings() const override;
    void forceRefresh() override;

private:
    // ── Data types ─────────────────────────────────────────
    struct FileEntry {
        int      fileId = 0;
        std::string filename;
        std::string system;
        std::string hash;           // short display hash (CRC32)
        std::string matchStatus;    // "match ✓", "match ?", "no match"
        int      confidence = 0;
        bool     checked = false;

        // Detail metadata
        std::string title;
        std::string developer;
        std::string publisher;
        std::string description;
        std::string region;
        std::string matchMethod;
    };

    // ── UI state ───────────────────────────────────────────
    enum class Focus { PathInput, FileList, DetailPane };
    Focus               m_focus = Focus::PathInput;

    // ── Widgets ────────────────────────────────────────────
    TextInput           m_pathInput{"Path: ", "Enter ROM directory..."};
    SelectableList      m_fileList;
    ProgressBarWidget   m_progressBar;
    SplitPane           m_splitPane;
    SplitPane::Layout   m_lastLayout{};

    // ── File data ──────────────────────────────────────────
    std::vector<FileEntry> m_files;
    std::mutex             m_filesMutex;

    // ── Pipeline ───────────────────────────────────────────
    TuiPipeline         m_pipeline;
    std::atomic<bool>   m_pipelineRunning{false};

    // ── Actions ────────────────────────────────────────────
    void startScan();
    void loadFromDatabase();

    // ── Render helpers ─────────────────────────────────────
    void drawHeader(ncplane *plane, unsigned cols);
    void drawDetailPane(ncplane *plane, int startY, int height, int startX, unsigned width);
    void drawFooter(ncplane *plane, unsigned rows, unsigned cols);

    // ── Helpers ────────────────────────────────────────────
    static std::string confidenceIcon(int confidence);
    static void setConfidenceColor(ncplane *plane, int confidence);
};
