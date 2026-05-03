import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

ScrollView {
    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout {
        width: parent.width
        spacing: 18

        Label {
            text: "Settings"
            font.pixelSize: 26
            font.bold: true
        }

        Label { text: "Metadata Providers"; font.bold: true }
        Repeater {
            model: settingsController.providerFields

            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true

                Label {
                    Layout.preferredWidth: 220
                    text: modelData.label
                }

                TextField {
                    Layout.fillWidth: true
                    echoMode: modelData.password ? TextInput.Password : TextInput.Normal
                    text: settingsController.stringValue(modelData.key)
                    onEditingFinished: settingsController.setValue(modelData.key, text)
                }
            }
        }

        Label { text: "Tools and Paths"; font.bold: true }
        Repeater {
            model: settingsController.toolFields

            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true

                Label {
                    Layout.preferredWidth: 220
                    text: modelData.label
                }

                TextField {
                    Layout.fillWidth: true
                    text: settingsController.stringValue(modelData.key)
                    onEditingFinished: settingsController.setValue(modelData.key, text)
                }
            }
        }

        Button {
            text: "Reset Saved Settings"
            onClicked: settingsController.resetToDefaults()
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#504945"
        }

        Label {
            text: "Danger Zone"
            font.bold: true
            color: "#fb4934"
        }

        Label {
            Layout.fillWidth: true
            text: "Erase Library Database removes selected data from the library. Choose what to clear below. ROM files on disk are never affected."
            wrapMode: Text.WordWrap
            color: "#a89984"
        }

        Button {
            id: eraseButton
            text: "Erase Library Database…"
            enabled: appController.libraryOpen
            palette.button: enabled ? "#cc241d" : "#504945"
            palette.buttonText: "#fbf1c7"
            onClicked: eraseConfirmDialog.open()
        }

        Dialog {
            id: eraseConfirmDialog
            title: "Erase Library Database?"
            modal: true
            standardButtons: Dialog.Ok | Dialog.Cancel
            anchors.centerIn: Overlay.overlay

            // Disable OK when nothing is checked
            onAboutToShow: {
                standardButton(Dialog.Ok).enabled =
                    chkFiles.checked || chkMatches.checked || chkApiCache.checked || chkArtwork.checked
            }

            ColumnLayout {
                spacing: 10
                width: 380

                Label {
                    text: "Select what to permanently erase. ROM files on disk will not be deleted."
                    wrapMode: Text.WordWrap
                    color: "#a89984"
                    Layout.fillWidth: true
                }

                // ── Select-all toggle ─────────────────────────────────────
                CheckBox {
                    id: chkAll
                    text: "Select all"
                    font.bold: true
                    checked: chkFiles.checked && chkMatches.checked && chkApiCache.checked && chkArtwork.checked
                    onClicked: {
                        const v = checked
                        chkFiles.checked    = v
                        chkMatches.checked  = v
                        chkApiCache.checked = v
                        chkArtwork.checked  = v
                    }
                }

                Rectangle { height: 1; color: "#504945"; Layout.fillWidth: true }

                // ── Individual options ────────────────────────────────────
                CheckBox {
                    id: chkFiles
                    text: "File records & scan history"
                    checked: true
                    onCheckedChanged: {
                        // File records removal implies match data is gone too (cascade)
                        if (checked) chkMatches.checked = true
                        updateOkButton()
                    }
                }
                Label {
                    text: "All imported ROMs, hashes, organize history, and patch records."
                    font.pixelSize: 11
                    color: "#928374"
                    leftPadding: 28
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                CheckBox {
                    id: chkMatches
                    text: "Match results & game metadata"
                    checked: true
                    enabled: !chkFiles.checked  // implied when files are erased
                    onCheckedChanged: updateOkButton()
                }
                Label {
                    text: "Identified game titles, systems, and match confidence scores."
                    font.pixelSize: 11
                    color: "#928374"
                    leftPadding: 28
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                CheckBox {
                    id: chkApiCache
                    text: "Metadata API cache"
                    checked: true
                    onCheckedChanged: updateOkButton()
                }
                Label {
                    text: "Cached provider responses (GameTDB, ScreenScraper, etc.). Next enrich will re-fetch from the network."
                    font.pixelSize: 11
                    color: "#928374"
                    leftPadding: 28
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                CheckBox {
                    id: chkArtwork
                    text: "Artwork cache"
                    checked: true
                    onCheckedChanged: updateOkButton()
                }
                Label {
                    text: "Downloaded box art and cover images on disk. Next artwork step will re-download."
                    font.pixelSize: 11
                    color: "#928374"
                    leftPadding: 28
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                Label {
                    visible: chkFiles.checked
                    text: "⚠  File records removal also clears match results."
                    font.pixelSize: 11
                    color: "#fabd2f"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }

            function updateOkButton() {
                if (visible)
                    standardButton(Dialog.Ok).enabled =
                        chkFiles.checked || chkMatches.checked || chkApiCache.checked || chkArtwork.checked
            }

            onAccepted: appController.eraseLibraryDatabase(
                chkFiles.checked,
                chkMatches.checked,
                chkApiCache.checked,
                chkArtwork.checked
            )
        }
    }
}