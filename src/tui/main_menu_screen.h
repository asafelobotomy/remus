#pragma once

#include "screen.h"
#include <string>
#include <vector>

/**
 * @brief Main menu screen with navigation to feature pages.
 *
 * Matches the wireframe "Main Screen":
 *   REMUS title   ─  MATCH (red)
 *   LIBRARY       ─  COMPRESSOR
 *   PATCH (red)   ─  OPTIONS
 */
class MainMenuScreen : public Screen {
public:
    explicit MainMenuScreen(TuiApp &app);

    void    onEnter() override;
    bool    handleInput(struct notcurses *nc, const ncinput &ni, int ch) override;
    void    render(struct notcurses *nc) override;
    std::string name() const override { return "MainMenu"; }
    std::vector<std::pair<std::string, std::string>> keybindings() const override;

private:
    struct MenuItem {
        std::string label;
        bool        highlighted; // drawn in red/bold
    };

    std::vector<MenuItem> m_items;
    int m_selected = 0;

    void activateSelected();
};
