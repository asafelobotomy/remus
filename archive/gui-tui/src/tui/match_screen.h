#pragma once

#include "screen.h"
#include "pipeline.h"
#include "widgets/text_input.h"
#include "widgets/selectable_list.h"
#include "widgets/progress_bar.h"
#include "widgets/split_pane.h"
#include "background_task.h"
#include "manual_match_overlay.h"
#include "../metadata/metadata_provider.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

namespace Remus {
class LocalDatabaseProvider;
class ProviderOrchestrator;
}

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

    // ── Public query API (for tests) ───────────────────────
    // ── Data types ─────────────────────────────────────────
    enum class Section { Confident, Possible, NoMatch };
    enum class ConfirmStatus { Pending, Confirmed, Rejected };

    struct FileEntry {
        int      fileId = 0;
        std::string filename;
        std::string system;
        std::string hash;           // short display hash (CRC32)
        std::string matchStatus;    // "match ✓", "match ?", "no match"
        std::string extensions;     // aggregated extensions for multi-file sets
        int      confidence = 0;
        bool     checked = false;

        // Section / status
        bool          isHeader = false;       // Section header row (not a real file)
        bool          isPossiblyPatched = false;
        Section       section = Section::NoMatch;
        ConfirmStatus confirmStatus = ConfirmStatus::Pending;

        // Detail metadata
        std::string title;
        std::string developer;
        std::string publisher;
        std::string description;
        std::string region;
        std::string matchMethod;
    };

    int fileCount() const {
        return static_cast<int>(std::count_if(m_files.begin(), m_files.end(),
                                              [](const FileEntry &e){ return !e.isHeader; }));
    }
    const FileEntry& fileAt(int i) const {
        // Return the i-th non-header entry
        int n = 0;
        for (const auto &e : m_files) {
            if (!e.isHeader) {
                if (n == i) return e;
                ++n;
            }
        }
        return m_files.at(static_cast<size_t>(i)); // fallback (shouldn't reach)
    }
    bool isPipelineRunning() const { return m_pipelineRunning.load(); }
    static std::string confidenceIcon(int confidence);

    // ── Actions (public for testability) ────────────────────
    void loadFromDatabase();

private:
    enum class Focus { PathInput, ScanButton, FileList, DetailPane };
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

    // ── Render helpers ─────────────────────────────────────
    void drawHeader(ncplane *plane, unsigned cols);
    void drawDetailPane(ncplane *plane, int startY, int height, int startX, unsigned width);
    void drawFooter(ncplane *plane, unsigned rows, unsigned cols);

    // ── Actions (private) ─────────────────────────────
    void startScan();
    void confirmSelectedMatch();
    void rejectSelectedMatch();
    void openManualMatch();
    void enrichSelectedMetadata();
    void drawManualMatchOverlay(ncplane *plane, unsigned rows, unsigned cols);
    bool handleOverlayInput(int ch, const ncinput &ni);

    // ── Helpers ────────────────────────────────────
    static bool looksPatched(const std::string &filename);
    static void setConfidenceColor(ncplane *plane, int confidence);
    Remus::ProviderOrchestrator &getOrchestrator(); // lazy-init

    // ── Metadata enrichment state ──────────────────────────
    std::unique_ptr<Remus::ProviderOrchestrator> m_orchestrator;
    BackgroundTask     m_enrichTask;

    // ── Manual match overlay (extracted widget) ────────────
    ManualMatchOverlay m_manualOverlay;
};
