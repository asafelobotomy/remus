pragma Singleton

import QtQuick
import QtQml

QtObject {
    readonly property var windowBg: "#1d2021"
    readonly property var panelBg: "#282828"
    readonly property var cardBg: "#32302f"
    readonly property var cardAltBg: "#3c3836"
    readonly property var border: "#504945"
    readonly property var borderStrong: "#665c54"

    readonly property var textStrong: "#fbf1c7"
    readonly property var text: "#ebdbb2"
    readonly property var textMuted: "#a89984"

    readonly property var accent: "#458588"
    readonly property var accentBright: "#83a598"
    readonly property var success: "#b8bb26"
    readonly property var warning: "#fabd2f"
    readonly property var danger: "#fb4934"
    readonly property var info: "#8ec07c"
    readonly property var orange: "#fe8019"

    readonly property var selectionFill: "#3f4d4f"
    readonly property var successFill: Qt.rgba(0.72, 0.73, 0.15, 0.18)
    readonly property var warningFill: Qt.rgba(0.98, 0.74, 0.18, 0.18)
    readonly property var dangerFill: Qt.rgba(0.98, 0.29, 0.20, 0.18)
}