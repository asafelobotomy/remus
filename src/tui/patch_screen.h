#pragma once

#include "screen.h"
#include "background_task.h"
#include "widgets/text_input.h"
#include "widgets/selectable_list.h"
#include "widgets/progress_bar.h"
#include "widgets/split_pane.h"

#include <string>
#include <vector>
#include <mutex>

#include <QString>

namespace Remus {
class PatchService;
struct PatchInfo;
}

/**
 * @brief Patch screen — apply patches (IPS/BPS/UPS/XDelta3/PPF) to ROMs.
 *
 * Layout:
 *   ┌──────────────────────────────────────────────────────┐
 *   │  PATCH                                       REMUS  │
 *   │  ROM:   [path input]                                │
 *   │  Patch: [path input]                                │
 *   ├──────────────────────────┬───────────────────────────┤
 *   │  Patch queue (list)      │  Patch info / details     │
 *   │  ☐ patch1.bps            │  Format: BPS              │
 *   │  ☐ patch2.ips            │  Source CRC: abc123       │
 *   │                          │  Status: Ready            │
 *   ├──────────────────────────┴───────────────────────────┤
 *   │  Progress: [####       ] patching 1/3                │
 *   └──────────────────────────────────────────────────────┘
 */
class PatchScreen : public Screen {
public:
    explicit PatchScreen(TuiApp &app);
    ~PatchScreen() override;

    void    onEnter() override;
    void    onLeave() override;
    bool    handleInput(struct notcurses *nc, const ncinput &ni, int ch) override;
    void    render(struct notcurses *nc) override;
    void    onResize(struct notcurses *nc) override;
    bool    tick() override;
    std::string name() const override { return "Patch"; }
    std::vector<std::pair<std::string, std::string>> keybindings() const override;

private:
    // ── Data types ─────────────────────────────────────────
    struct PatchEntry {
        std::string path;
        std::string filename;
        std::string formatName;    // "IPS", "BPS", "UPS", "XDelta3", "PPF"
        int64_t     sizeBytes = 0;
        std::string sourceCrc;     // expected source checksum
        std::string targetCrc;     // expected target checksum
        std::string status;        // "Ready", "Applied", "Error: ..."
        bool        checked = true;
        bool        valid = true;
    };

    // ── UI state ───────────────────────────────────────────
    enum class Focus { RomInput, PatchInput, PatchList, DetailPane };
    Focus        m_focus = Focus::RomInput;
    bool         m_createBackup = true;

    // ── Widgets ────────────────────────────────────────────
    TextInput         m_romInput{"ROM:   ", "Enter ROM file path..."};
    TextInput         m_patchInput{"Patch: ", "Enter patch file/directory..."};
    SelectableList    m_patchList;
    ProgressBarWidget m_progressBar;
    SplitPane         m_splitPane;
    SplitPane::Layout  m_lastLayout{};

    // ── Patch data ─────────────────────────────────────────
    std::vector<PatchEntry> m_patches;
    std::mutex              m_patchesMutex;

    // ── Processing ─────────────────────────────────────────
    BackgroundTask m_task;

    // ── Core engine ────────────────────────────────────────
    Remus::PatchService *m_patchService = nullptr;

    // ── Actions ────────────────────────────────────────────
    void scanPatches();
    void startPatching();
    void applyPatches();
    void applySinglePatch(size_t idx, const QString &qRomPath);

    // ── Render helpers ─────────────────────────────────────
    void drawHeader(ncplane *plane, unsigned cols);
    void drawDetailPane(ncplane *plane, int startY, int height, int startX, unsigned width);
    void drawFooter(ncplane *plane, unsigned rows, unsigned cols);
    void drawToolStatus(ncplane *plane, int &y, int startX, int height);

    // ── Helpers ────────────────────────────────────────────
    static std::string formatSize(int64_t bytes);
};
