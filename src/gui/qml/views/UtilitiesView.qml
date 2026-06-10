import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

// Utilities workbench: DAT import, verification, patching, and mods.
// Opened from Tools menu (P6) — not a primary sidebar destination.
Item {
    id: root

    Layout.fillWidth:  true
    Layout.fillHeight: true

    function openTab(index) {
        tabBar.currentIndex = Math.max(0, Math.min(index, tabBar.count - 1))
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabBar
            Layout.fillWidth: true

            TabButton { text: "DAT Import" }
            TabButton { text: "Verify"     }
            TabButton { text: "Patch"      }
            TabButton { text: "Mods"       }
        }

        StackLayout {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            currentIndex:      tabBar.currentIndex

            DatView    {}
            VerifyView {}
            PatchView  {}
            ModView    {}
        }
    }
}
