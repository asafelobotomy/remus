import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

// Utilities workbench: DAT import, verification, patching, and mods.
// Groups the four legacy tool views behind a simple TabBar.
Item {
    Layout.fillWidth:  true
    Layout.fillHeight: true

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
