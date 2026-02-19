#pragma once

#include "widgets/text_input.h"
#include "widgets/selectable_list.h"
#include "background_task.h"
#include "../metadata/metadata_provider.h"

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <functional>

struct notcurses;
struct ncplane;
struct ncinput;

class TuiApp;
namespace Remus { class LocalDatabaseProvider; }

/**
 * @brief Extracted manual-match overlay (previously inline in MatchScreen).
 *
 * Self-contained modal that lets the user search local DAT databases and
 * apply a match to a target file.  MatchScreen creates one instance,
 * delegates `handleInput` / `render` while `isActive()`, and provides
 * an `onApplied` callback for refreshing its own data.
 */
class ManualMatchOverlay {
public:
    explicit ManualMatchOverlay(TuiApp &app);
    ~ManualMatchOverlay();

    /// Open the overlay for the given file.
    void open(int fileId, const std::string &system, const std::string &title);

    /// Close the overlay.
    void close();

    /// True while the overlay is shown.
    bool isActive() const { return m_active; }

    /// Handle a key event.  Returns true if consumed.
    bool handleInput(int ch, const ncinput &ni);

    /// Render the overlay (call only when isActive()).
    void render(ncplane *plane, unsigned rows, unsigned cols);

    /// Callback invoked (on main thread) after a match is applied.
    /// Args: fileId, gameId, title (std::string).
    std::function<void(int, int, const std::string&)> onApplied;

private:
    TuiApp &m_app;
    bool    m_active = false;

    // ── Overlay state ──────────────────────────────────────
    bool        m_searching     = false;
    bool        m_providerReady = false;
    int         m_targetFileId  = 0;
    std::string m_targetSystem;
    std::string m_targetTitle;
    std::vector<Remus::SearchResult> m_results;
    std::string m_statusMsg;
    enum class OFocus { Search, Results };
    OFocus      m_focus = OFocus::Search;

    // ── Widgets ────────────────────────────────────────────
    TextInput      m_search{"Search: ", "Game title..."};
    SelectableList m_list;

    // ── Provider ───────────────────────────────────────────
    std::unique_ptr<Remus::LocalDatabaseProvider> m_provider;
    std::mutex     m_mutex;   ///< guards m_results / m_statusMsg
    BackgroundTask m_task;

    // ── Internal helpers ───────────────────────────────────
    void initProvider();
    void runSearch(const std::string &query);
    void applyMatch(int resultIdx);
};
