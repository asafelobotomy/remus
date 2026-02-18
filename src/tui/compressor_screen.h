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

namespace Remus {
class ConversionService;
}

/**
 * @brief Compressor screen — CHD conversion & archive extraction.
 *
 * Layout:
 *   ┌──────────────────────────────────────────────────────┐
 *   │  COMPRESSOR                                  REMUS  │
 *   │  Source: [path input]                               │
 *   │  Output: [path input]                               │
 *   ├──────────────────────────┬───────────────────────────┤
 *   │  File list (scrollable)  │  File info / CHD details  │
 *   │  ☐ file.cue              │  Format: BIN/CUE          │
 *   │  ☐ file.iso              │  Size: 700 MB             │
 *   │  ☐ file.chd              │  Status: Ready            │
 *   ├──────────────────────────┴───────────────────────────┤
 *   │  Progress: [####       ] converting 2/10             │
 *   └──────────────────────────────────────────────────────┘
 */
class CompressorScreen : public Screen {
public:
    explicit CompressorScreen(TuiApp &app);
    ~CompressorScreen() override;

    void    onEnter() override;
    void    onLeave() override;
    bool    handleInput(struct notcurses *nc, const ncinput &ni, int ch) override;
    void    render(struct notcurses *nc) override;
    void    onResize(struct notcurses *nc) override;
    bool    tick() override;
    std::string name() const override { return "Compressor"; }
    std::vector<std::pair<std::string, std::string>> keybindings() const override;

    // ── Public query API (for tests) ───────────────────────
    enum class FileType { CUE, ISO, GDI, CHD, ZIP, SevenZ, RAR, Unknown };
    enum class OpMode { Compress, Extract, Archive };

    struct FileEntry {
        std::string path;
        std::string filename;
        FileType    type = FileType::Unknown;
        int64_t     sizeBytes = 0;
        std::string status;
        bool        checked = true;
        double      ratio = 0.0;
    };

    int fileCount() const { return static_cast<int>(m_files.size()); }
    const FileEntry& fileAt(int i) const { return m_files.at(static_cast<size_t>(i)); }
    OpMode mode() const { return m_mode; }
    bool isRunning() const { return m_task.running(); }
    bool deleteOriginals() const { return m_deleteOriginals; }
    static FileType detectFileType(const std::string &filename);
    static std::string fileTypeString(FileType ft);
    static std::string formatSize(int64_t bytes);

private:
    enum class Focus { SourceInput, OutputInput, FileList, DetailPane };
    Focus         m_focus = Focus::SourceInput;
    OpMode        m_mode = OpMode::Compress;
    bool          m_deleteOriginals = false;

    // ── Widgets ────────────────────────────────────────────
    TextInput         m_sourceInput{"Source: ", "Enter source directory..."};
    TextInput         m_outputInput{"Output: ", "(same directory)"};
    SelectableList    m_fileList;
    ProgressBarWidget m_progressBar;
    SplitPane         m_splitPane;
    SplitPane::Layout  m_lastLayout{};

    // ── File data ──────────────────────────────────────────
    std::vector<FileEntry> m_files;
    std::mutex             m_filesMutex;

    // ── Processing state ───────────────────────────────────
    BackgroundTask m_task;

    // ── Core engines ───────────────────────────────────────
    Remus::ConversionService *m_conversionService = nullptr;

    // ── Actions ────────────────────────────────────────────
    void scanSource();
    void startProcessing();
    void processFiles();
    void processSingleFile(size_t idx, const std::string &outDir);

    // ── Render helpers ─────────────────────────────────────
    void drawHeader(ncplane *plane, unsigned cols);
    void drawDetailPane(ncplane *plane, int startY, int height, int startX, unsigned width);
    void drawFooter(ncplane *plane, unsigned rows, unsigned cols);

};
