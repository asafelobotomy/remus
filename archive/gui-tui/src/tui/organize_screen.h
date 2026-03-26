#pragma once

#include "screen.h"
#include "background_task.h"
#include "widgets/text_input.h"
#include "widgets/selectable_list.h"
#include "widgets/progress_bar.h"
#include "widgets/split_pane.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Remus { class OrganizeEngine; }

/**
 * @brief Organize screen — preview and execute rename/move of confirmed matches.
 *
 * Layout:
 *   ┌──────────────────────────────────────────────────────┐
 *   │  ORGANIZE       42 confirmed matches         REMUS  │
 *   │  Dest: [/home/user/organized.................] [M]  │
 *   │  Tmpl: [blank = No-Intro default.............]      │
 *   ├────────────────────────┬───────────────────────────  │
 *   │  ✓ Mario.nes    (100%) │  Old: /roms/Mario.nes      │
 *   │  ✓ Zelda.sfc     (93%) │  New: /org/NES/Zelda…      │
 *   │  ✗ Broken.gb    (err)  │  Status: Preview           │
 *   ├────────────────────────┴───────────────────────────  │
 *   │  Progress: [################     ] 12/42             │
 *   └──────────────────────────────────────────────────────┘
 */
class OrganizeScreen : public Screen {
public:
    explicit OrganizeScreen(TuiApp &app);
    ~OrganizeScreen() override;

    void    onEnter() override;
    void    onLeave() override;
    bool    handleInput(struct notcurses *nc, const ncinput &ni, int ch) override;
    void    render(struct notcurses *nc) override;
    void    onResize(struct notcurses *nc) override;
    bool    tick() override;
    std::string name() const override { return "Organize"; }
    std::vector<std::pair<std::string, std::string>> keybindings() const override;
    void forceRefresh() override;

    // ── Public query API (for tests) ───────────────────────────
    enum class EntryStatus {
        Pending,    ///< Not yet previewed
        Preview,    ///< Dry-run path computed, ready to execute
        OK,         ///< Successfully moved/copied
        Skipped,    ///< Skipped (collision / no dest set)
        Error       ///< Failed (error in errorMsg)
    };

    enum class OpMode { Move, Copy };

    struct OrganizeEntry {
        int         fileId     = 0;
        int         confidence = 0;
        std::string filename;
        std::string system;
        std::string title;          ///< game title from match
        std::string region;
        std::string publisher;
        std::string developer;
        int         releaseYear = 0;
        std::string oldPath;
        std::string newPath;        ///< populated after dry-run
        EntryStatus status    = EntryStatus::Pending;
        std::string errorMsg;
        std::vector<int> linkedFileIds; ///< child/track files to co-move
    };

    int  entryCount() const { return static_cast<int>(m_entries.size()); }
    const OrganizeEntry& entryAt(int i) const { return m_entries.at(static_cast<size_t>(i)); }
    bool isRunning() const { return m_task.running(); }

    /// Reload entries from database (confirmed matches).
    void loadFromDatabase();

    /// Run dry-run preview with current dest/template settings.
    void runDryRun();

    /// Set destination directory (for testing).
    void setDestination(const std::string &v) { m_destInput.setValue(v); }
    /// Set template string (for testing).
    void setTemplate(const std::string &v) { m_templInput.setValue(v); }

private:
    enum class Focus { DestInput, TemplInput, FileList };
    Focus   m_focus = Focus::DestInput;
    OpMode  m_opMode = OpMode::Move;

    // ── Widgets ────────────────────────────────────────────────
    TextInput         m_destInput{"Dest: ", "Destination directory..."};
    TextInput         m_templInput{"Tmpl: ", "(blank = No-Intro default)"};
    SelectableList    m_fileList;
    ProgressBarWidget m_progressBar;
    SplitPane         m_splitPane;
    SplitPane::Layout m_lastLayout{};

    // ── Entry data ─────────────────────────────────────────────
    std::vector<OrganizeEntry>  m_entries;
    mutable std::mutex          m_entriesMutex;

    // ── Background execute task ────────────────────────────────
    BackgroundTask    m_task;
    std::atomic<int>  m_taskDone{0};
    std::atomic<int>  m_taskTotal{0};

    // ── Helpers ────────────────────────────────────────────────
    void runExecute();
    void drawHeader(ncplane *plane, unsigned cols) const;
    void drawDetailPane(ncplane *plane, int startY, int height,
                        int startX, unsigned width) const;
    void drawFooter(ncplane *plane, unsigned rows, unsigned cols) const;

    static std::string statusIcon(EntryStatus s);
    static std::string formatOpMode(OpMode m) { return m == OpMode::Move ? "MOVE" : "COPY"; }
    static void setStatusColor(ncplane *plane, EntryStatus s);
};
