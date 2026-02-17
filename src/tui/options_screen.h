#pragma once

#include "screen.h"

#include <string>
#include <vector>

/**
 * @brief Options screen — manage provider credentials and app settings.
 *
 * Layout:
 *   ┌──────────────────────────────────────────────────────┐
 *   │  OPTIONS                                     REMUS  │
 *   ├──────────────────────────────────────────────────────┤
 *   │  METADATA PROVIDERS                                  │
 *   │  ScreenScraper Username: [________]                  │
 *   │  ScreenScraper Password: [________]                  │
 *   │  TheGamesDB API Key:     [________]                  │
 *   │  IGDB Client ID:         [________]                  │
 *   │  IGDB Client Secret:     [________]                  │
 *   │  Hasheous API Key:       [________]                  │
 *   │                                                      │
 *   │  ORGANIZE                                            │
 *   │  Naming Template:        [________]                  │
 *   │  Organize by System:     [ON/OFF]                    │
 *   │  Preserve Originals:     [ON/OFF]                    │
 *   │                                                      │
 *   │  PERFORMANCE                                         │
 *   │  Parallel Hashing:       [ON/OFF]                    │
 *   ├──────────────────────────────────────────────────────┤
 *   │  j/k:navigate  Enter:edit  s:save  Esc:back         │
 *   └──────────────────────────────────────────────────────┘
 */
class OptionsScreen : public Screen {
public:
    explicit OptionsScreen(TuiApp &app);
    ~OptionsScreen() override = default;

    void    onEnter() override;
    bool    handleInput(struct notcurses *nc, const ncinput &ni, int ch) override;
    void    render(struct notcurses *nc) override;
    void    onResize(struct notcurses *nc) override;
    std::string name() const override { return "Options"; }
    std::vector<std::pair<std::string, std::string>> keybindings() const override;
    void forceRefresh() override;

private:
    // ── Data types ─────────────────────────────────────────
    enum class FieldType { Text, Password, Toggle };

    struct SettingField {
        std::string label;
        std::string key;          // QSettings key
        std::string value;
        FieldType   type = FieldType::Text;
        bool        isSection = false;  // section header, not editable
    };

    // ── UI state ───────────────────────────────────────────
    std::vector<SettingField> m_fields;
    int  m_selected = 0;
    int  m_scroll = 0;
    bool m_editing = false;       // currently editing a field
    bool m_dirty = false;         // unsaved changes
    std::string m_statusMsg;      // "Saved!" or error message

    // ── Actions ────────────────────────────────────────────
    void loadSettings();
    void saveSettings();
    void toggleField();

    // ── Render helpers ─────────────────────────────────────
    void drawHeader(ncplane *plane, unsigned cols);
    void drawFields(ncplane *plane, int startY, int height, unsigned cols);
    void drawFooter(ncplane *plane, unsigned rows, unsigned cols);
};
