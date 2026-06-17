pragma Singleton
import QtQuick

QtObject {
    // ── Palette ───────────────────────────────────────────────────────────────
    // Gruvbox dark — one source of truth for every hex value in the GUI.

    // Backgrounds
    readonly property color background:  "#1d2021"   // deepest panel bg
    readonly property color surface:     "#282828"   // card / drawer bg
    readonly property color surfaceAlt:  "#32302f"   // alternate surface (sidebar, gradient)
    readonly property color surfaceHigh: "#252525"   // group row highlight bg

    // Borders & chrome
    readonly property color border:      "#504945"   // default border
    readonly property color borderSub:   "#3c3836"   // subtle divider / track
    readonly property color hover:       "#383838"   // row hover state
    readonly property color selected:    "#3f4d4f"   // selected / highlighted row

    // Text
    readonly property color textPrimary: "#fbf1c7"   // headings, titles, bright text
    readonly property color textBody:    "#ebdbb2"   // normal readable text
    readonly property color textMuted:   "#a89984"   // secondary labels, captions
    readonly property color textDim:     "#928374"   // tertiary / hint text
    readonly property color textDisabled:"#665c54"   // placeholder / disabled text

    // Accent
    readonly property color accent:      "#458588"   // highlight / link
    readonly property color accentAlt:   "#83a598"   // softer accent (info messages)

    // Semantic status — one canonical color per state
    readonly property color success:     "#b8bb26"   // green — matched, done, ok
    readonly property color warn:        "#fabd2f"   // yellow — needs attention
    readonly property color error:       "#fb4934"   // red — failed, error, danger

    // Status icon glyphs (slightly different shade for the icon itself)
    readonly property color successIcon: "#b8bb26"
    readonly property color warnIcon:    "#d79921"
    readonly property color errorIcon:   "#cc241d"

    // ── Typography scale ─────────────────────────────────────────────────────
    readonly property int fontXs:    10   // column headers, micro labels
    readonly property int fontSm:    11   // body / secondary text
    readonly property int fontMd:    12   // standard form text, values
    readonly property int fontLg:    13   // section/panel sub-headers
    readonly property int fontXl:    14   // card stage titles, table header
    readonly property int fontTitle: 15   // selected ROM title in inspector
    readonly property int fontHero:  26   // page titles (Settings, Export…)

    // ── Panel chrome ─────────────────────────────────────────────────────────
    readonly property int   panelRadius: 12
    readonly property color panelBg:     background
    readonly property color panelBorder: border
}
