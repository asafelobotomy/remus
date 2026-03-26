#include "options_screen.h"
#include "app.h"

#include <QSettings>
#include <cstring>

// Use the constants from the project
#include "../core/constants/settings.h"

// ════════════════════════════════════════════════════════════
// Construction / Lifecycle
// ════════════════════════════════════════════════════════════

OptionsScreen::OptionsScreen(TuiApp &app)
    : Screen(app)
{
    // Define all settings fields
    m_fields = {
        // Section: Metadata Providers (generated from ALL_PROVIDER_FIELDS)
        { "METADATA PROVIDERS", "", "", FieldType::Text, true },
    };

    // Populate provider fields from the constants array
    for (const auto &pf : Remus::Constants::ALL_PROVIDER_FIELDS) {
        SettingField sf;
        sf.label = pf.label;
        sf.key = pf.key;
        sf.type = pf.isPassword ? FieldType::Password : FieldType::Text;
        sf.isSection = false;
        m_fields.push_back(std::move(sf));
    }

    // Section: Organize
    m_fields.push_back({ "ORGANIZE", "", "", FieldType::Text, true });
    m_fields.push_back({ "Naming Template",    Remus::Constants::Settings::Organize::NAMING_TEMPLATE,    "", FieldType::Text,   false });
    m_fields.push_back({ "Organize by System", Remus::Constants::Settings::Organize::BY_SYSTEM,          "true", FieldType::Toggle, false });
    m_fields.push_back({ "Preserve Originals", Remus::Constants::Settings::Organize::PRESERVE_ORIGINALS, "true", FieldType::Toggle, false });

    // Section: Matching
    m_fields.push_back({ "MATCHING", "", "", FieldType::Text, true });
    m_fields.push_back({ "Organize Confidence Threshold (%)", Remus::Constants::Settings::Match::CONFIDENCE_THRESHOLD,
                          Remus::Constants::Settings::Defaults::CONFIDENCE_THRESHOLD.toStdString(), FieldType::Text, false });

    // Section: Performance
    m_fields.push_back({ "PERFORMANCE", "", "", FieldType::Text, true });
    m_fields.push_back({ "Parallel Hashing",   Remus::Constants::Settings::Performance::PARALLEL_HASHING, "true", FieldType::Toggle, false });

    // Start selection on first non-header field
    m_selected = 1;
}

void OptionsScreen::onEnter()
{
    // Only load on first visit; preserve state on back-navigation
    if (m_fields.empty())
        loadSettings();
    m_statusMsg.clear();
}

// ════════════════════════════════════════════════════════════
// Input handling
// ════════════════════════════════════════════════════════════

bool OptionsScreen::handleInput(struct notcurses *, const ncinput &, int ch)
{
    int count = static_cast<int>(m_fields.size());

    // ── Editing mode ───────────────────────────────────────
    if (m_editing) {
        if (ch == NCKEY_ENTER || ch == '\n' || ch == '\r' || ch == NCKEY_ESC) {
            m_editing = false;
            m_dirty = true;
            return true;
        }
        if (ch == NCKEY_BACKSPACE || ch == 127) {
            auto &val = m_fields[m_selected].value;
            if (!val.empty()) val.pop_back();
            m_dirty = true;
            return true;
        }
        if (ch >= 32 && ch < 127) {
            m_fields[m_selected].value.push_back(static_cast<char>(ch));
            m_dirty = true;
            return true;
        }
        return false;
    }

    // ── Normal mode ────────────────────────────────────────
    if (ch == 'j' || ch == NCKEY_DOWN) {
        do {
            m_selected++;
            if (m_selected >= count) m_selected = count - 1;
        } while (m_selected < count && m_fields[m_selected].isSection);
        return true;
    }
    if (ch == 'k' || ch == NCKEY_UP) {
        do {
            m_selected--;
            if (m_selected < 0) m_selected = 0;
        } while (m_selected > 0 && m_fields[m_selected].isSection);
        return true;
    }

    if (ch == NCKEY_ENTER || ch == '\n' || ch == '\r') {
        if (m_selected >= 0 && m_selected < count && !m_fields[m_selected].isSection) {
            if (m_fields[m_selected].type == FieldType::Toggle) {
                toggleField();
            } else {
                m_editing = true;
            }
        }
        return true;
    }

    if (ch == ' ') {
        // Space also toggles
        if (m_selected >= 0 && m_selected < count &&
            !m_fields[m_selected].isSection &&
            m_fields[m_selected].type == FieldType::Toggle) {
            toggleField();
            return true;
        }
    }

    if (ch == 's' || ch == 'S') {
        saveSettings();
        return true;
    }

    // Esc first-refusal: warn about unsaved changes before popping
    if (ch == NCKEY_ESC) {
        if (m_dirty) {
            m_app.toast("Unsaved changes — press 's' to save or Esc again to discard", Toast::Level::Warning);
            m_dirty = false;  // clear so next Esc falls through
            return true;
        }
        return false;  // let app pop screen
    }

    return false;
}

// ════════════════════════════════════════════════════════════
// Rendering
// ════════════════════════════════════════════════════════════

void OptionsScreen::render(struct notcurses *nc)
{
    unsigned rows = m_app.rows();
    unsigned cols = m_app.cols();
    ncplane *std = notcurses_stdplane(nc);

    int headerH = 2;
    int footerH = 1;
    int bodyH = static_cast<int>(rows) - headerH - footerH;
    if (bodyH < 3) bodyH = 3;

    drawHeader(std, cols);
    drawFields(std, headerH, bodyH, cols);
    drawFooter(std, rows, cols);

    ncplane_set_channels(std, 0);
    ncplane_set_styles(std, NCSTYLE_NONE);
}

void OptionsScreen::onResize(struct notcurses *)
{
}

// ════════════════════════════════════════════════════════════
// Render helpers
// ════════════════════════════════════════════════════════════

void OptionsScreen::drawHeader(ncplane *plane, unsigned cols)
{
    // Row 0: Title
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0x00);
        ncplane_set_channels(plane, ch);
        ncplane_set_styles(plane, NCSTYLE_BOLD);
        ncplane_putstr_yx(plane, 0, 2, "OPTIONS");
        ncplane_set_styles(plane, NCSTYLE_NONE);
    }

    // Dirty indicator
    if (m_dirty) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0xCC, 0xAA, 0x00);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, 0, 11, "(unsaved)");
    }

    // Status message
    if (!m_statusMsg.empty()) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x00, 0xCC, 0x00);
        ncplane_set_channels(plane, ch);
        int x = static_cast<int>(cols) / 2;
        ncplane_putstr_yx(plane, 0, x, m_statusMsg.c_str());
    }

    // "REMUS" right-aligned
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x66, 0x66, 0x66);
        ncplane_set_channels(plane, ch);
        ncplane_putstr_yx(plane, 0, static_cast<int>(cols) - 7, "REMUS");
    }

    // Row 1: separator
    {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb8(&ch, 0x44, 0x44, 0x44);
        ncplane_set_channels(plane, ch);
        std::string sep(cols, '-');
        ncplane_putstr_yx(plane, 1, 0, sep.c_str());
    }
}

void OptionsScreen::drawFields(ncplane *plane, int startY, int height, unsigned cols)
{
    int count = static_cast<int>(m_fields.size());

    // Auto-scroll
    if (m_selected >= 0) {
        if (m_selected < m_scroll) m_scroll = m_selected;
        if (m_selected >= m_scroll + height) m_scroll = m_selected - height + 1;
    }
    if (m_scroll < 0) m_scroll = 0;

    int y = startY;
    int labelW = 26; // fixed label width

    for (int i = m_scroll; i < count && y < startY + height; ++i) {
        const auto &f = m_fields[i];
        bool selected = (i == m_selected);

        if (f.isSection) {
            // Section header
            if (y > startY) y++; // blank line before section (except first)
            if (y >= startY + height) break;

            uint64_t ch = 0;
            ncchannels_set_fg_rgb8(&ch, 0xAA, 0xAA, 0xFF);
            ncplane_set_channels(plane, ch);
            ncplane_set_styles(plane, NCSTYLE_BOLD | NCSTYLE_UNDERLINE);
            ncplane_putstr_yx(plane, y, 2, f.label.c_str());
            ncplane_set_styles(plane, NCSTYLE_NONE);
            y++;
            continue;
        }

        // Regular field
        {
            uint64_t ch = 0;
            if (selected) {
                ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
                ncchannels_set_bg_rgb8(&ch, 0x22, 0x44, 0x66);
            } else {
                ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
            }
            ncplane_set_channels(plane, ch);

            // Label
            std::string label = "  " + f.label + ":";
            label.resize(static_cast<size_t>(labelW), ' ');
            ncplane_putstr_yx(plane, y, 2, label.c_str());

            // Value field
            int fieldStart = 2 + labelW + 1;
            int fieldWidth = static_cast<int>(cols) - fieldStart - 2;
            if (fieldWidth < 8) fieldWidth = 8;

            if (f.type == FieldType::Toggle) {
                bool on = (f.value == "true" || f.value == "1");
                ch = 0;
                if (selected) ncchannels_set_bg_rgb8(&ch, 0x22, 0x44, 0x66);
                ncchannels_set_fg_rgb8(&ch, on ? 0x00 : 0xCC, on ? 0xCC : 0x00, 0x00);
                ncplane_set_channels(plane, ch);
                ncplane_putstr_yx(plane, y, fieldStart, on ? "[ON] " : "[OFF]");
            } else if (f.type == FieldType::Password) {
                ch = 0;
                if (selected && m_editing) {
                    ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
                    ncchannels_set_bg_rgb8(&ch, 0x44, 0x33, 0x33);
                } else if (selected) {
                    ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
                    ncchannels_set_bg_rgb8(&ch, 0x22, 0x44, 0x66);
                } else {
                    ncchannels_set_fg_rgb8(&ch, 0x88, 0x88, 0x88);
                }
                ncplane_set_channels(plane, ch);

                std::string display;
                if (m_editing && selected) {
                    display = f.value + "_";
                } else {
                    display = f.value.empty() ? "(not set)" : std::string(f.value.size(), '*');
                }
                if (static_cast<int>(display.size()) > fieldWidth)
                    display = display.substr(display.size() - static_cast<size_t>(fieldWidth));
                display.resize(static_cast<size_t>(fieldWidth), ' ');
                ncplane_putstr_yx(plane, y, fieldStart, display.c_str());
            } else {
                // Text
                ch = 0;
                if (selected && m_editing) {
                    ncchannels_set_fg_rgb8(&ch, 0xFF, 0xFF, 0xFF);
                    ncchannels_set_bg_rgb8(&ch, 0x33, 0x33, 0x33);
                } else if (selected) {
                    ncchannels_set_fg_rgb8(&ch, 0xCC, 0xCC, 0xCC);
                    ncchannels_set_bg_rgb8(&ch, 0x22, 0x44, 0x66);
                } else {
                    ncchannels_set_fg_rgb8(&ch, 0xAA, 0xAA, 0xAA);
                }
                ncplane_set_channels(plane, ch);

                std::string display = f.value;
                if (m_editing && selected) display += "_";
                if (display.empty()) display = "(not set)";
                if (static_cast<int>(display.size()) > fieldWidth)
                    display = "..." + display.substr(display.size() - static_cast<size_t>(fieldWidth) + 3);
                display.resize(static_cast<size_t>(fieldWidth), ' ');
                ncplane_putstr_yx(plane, y, fieldStart, display.c_str());
            }

            // Clear bg
            ch = 0;
            ncplane_set_channels(plane, ch);
        }
        y++;
    }
}

void OptionsScreen::drawFooter(ncplane *plane, unsigned rows, unsigned cols)
{
    const char *hint;
    if (m_editing)
        hint = "Type value  Enter/Esc:finish editing  s:save";
    else
        hint = "j/k:navigate  Enter:edit  Space:toggle  s:save  Esc:back";

    uint64_t ch = 0;
    ncchannels_set_fg_rgb8(&ch, 0x55, 0x55, 0x55);
    ncplane_set_channels(plane, ch);
    int x = (static_cast<int>(cols) - static_cast<int>(strlen(hint))) / 2;
    ncplane_putstr_yx(plane, static_cast<int>(rows) - 1, x, hint);
}

// ════════════════════════════════════════════════════════════
// Actions
// ════════════════════════════════════════════════════════════

void OptionsScreen::loadSettings()
{
    QSettings settings;
    for (auto &f : m_fields) {
        if (f.isSection || f.key.empty()) continue;
        QString val = settings.value(QString::fromStdString(f.key)).toString();
        if (!val.isEmpty())
            f.value = val.toStdString();
    }
    m_dirty = false;
}

void OptionsScreen::saveSettings()
{
    QSettings settings;
    for (const auto &f : m_fields) {
        if (f.isSection || f.key.empty()) continue;
        settings.setValue(QString::fromStdString(f.key), QString::fromStdString(f.value));
    }
    settings.sync();
    m_dirty = false;
    m_statusMsg = "Saved!";
}

void OptionsScreen::toggleField()
{
    if (m_selected < 0 || m_selected >= static_cast<int>(m_fields.size())) return;
    auto &f = m_fields[m_selected];
    if (f.type != FieldType::Toggle) return;

    bool on = (f.value == "true" || f.value == "1");
    f.value = on ? "false" : "true";
    m_dirty = true;
}

void OptionsScreen::forceRefresh()
{
    m_fields.clear();
    loadSettings();
    m_statusMsg.clear();
}

std::vector<std::pair<std::string, std::string>> OptionsScreen::keybindings() const
{
    return {
        {"j/k",   "Navigate fields"},
        {"Enter", "Edit field"},
        {"Space", "Toggle on/off"},
        {"s",     "Save settings"},
        {"Esc",   "Stop editing / back"},
    };
}
