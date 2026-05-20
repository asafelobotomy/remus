import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Remus.Gui

ScrollView {
    Layout.fillWidth: true
    Layout.fillHeight: true

    // Shared dialogs for browsing tool paths
    FileDialog {
        id: toolFileDialog
        title: "Select Tool Executable"
        property string targetKey: ""
        onAccepted: {
            const path = decodeURIComponent(selectedFile.toString().replace(/^file:\/\//, ""))
            settingsController.setValue(targetKey, path)
        }
    }

    FolderDialog {
        id: toolFolderDialog
        title: "Select Directory"
        property string targetKey: ""
        onAccepted: {
            const path = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""))
            settingsController.setValue(targetKey, path)
        }
    }

    // Surface credential-save failures to the user without exposing raw key names.
    Connections {
        target: settingsController
        function onSettingsError(message) {
            saveErrorPopup.open()
        }
    }

    Dialog {
        id: saveErrorPopup
        title: "Settings Error"
        modal: true
        standardButtons: Dialog.Ok
        anchors.centerIn: parent
        Label {
            text: "A credential could not be saved securely. Check that your system keychain is available."
            wrapMode: Text.WordWrap
            width: 340
        }
    }

    ColumnLayout {
        width: parent.width
        spacing: 18

        Label {
            text: "Settings"
            font.pixelSize: 26
            font.bold: true
        }

        // ── Metadata Providers ───────────────────────────────────────────────
        Label { text: "Metadata Providers"; font.bold: true }

        Repeater {
            model: settingsController.providerGroups

            delegate: ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 4

                // Provider group heading
                Label {
                    text: modelData.groupName
                    font.bold: true
                    font.pixelSize: 12
                    color: "#a89984"
                }

                // Fields for this provider
                Repeater {
                    model: modelData.fields

                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true

                        Label {
                            Layout.preferredWidth: 200
                            text: modelData.label
                            font.pixelSize: 12
                        }

                        TextField {
                            id: providerField
                            Layout.fillWidth: true
                            font.pixelSize: 12
                            echoMode: modelData.password ? TextInput.Password : TextInput.Normal
                            text: settingsController.stringValue(modelData.key)
                            onEditingFinished: settingsController.setValue(modelData.key, text)
                        }
                    }
                }

                // Authenticate button + result row
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        text: "Authenticate"
                        font.pixelSize: 11
                        padding: 6
                        onClicked: {
                            const msg = settingsController.authenticateProvider(modelData.groupKey)
                            authResultLabel.text = msg
                            authResultLabel.color = msg === "Credentials saved." ? "#b8bb26" : "#fb4934"
                        }
                    }

                    Label {
                        id: authResultLabel
                        font.pixelSize: 11
                        color: "#a89984"
                    }

                    Item { Layout.fillWidth: true }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#3c3836"
                }
            }
        }

        // ── Tools and Paths ──────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Tools and Paths"; font.bold: true; Layout.fillWidth: true }
            Button {
                text:    "Auto-Detect Tools"
                font.pixelSize: 11
                padding: 6
                onClicked: settingsController.autoDetectTools()
            }
        }

        Repeater {
            model: settingsController.toolFields

            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true

                Label {
                    Layout.preferredWidth: 200
                    text: modelData.label
                    font.pixelSize: 12
                }

                TextField {
                    id: toolField
                    Layout.fillWidth: true
                    font.pixelSize: 12
                    text: settingsController.stringValue(modelData.key)
                    onEditingFinished: settingsController.setValue(modelData.key, text)

                    Connections {
                        target: settingsController
                        function onSettingsChanged() {
                            toolField.text = settingsController.stringValue(modelData.key)
                        }
                    }
                }

                Button {
                    visible: modelData.browsable
                    text:    "Browse"
                    flat:    true
                    font.pixelSize: 11
                    padding: 6
                    onClicked: {
                        if (modelData.isDirectory) {
                            toolFolderDialog.targetKey = modelData.key
                            toolFolderDialog.open()
                        } else {
                            toolFileDialog.targetKey = modelData.key
                            toolFileDialog.open()
                        }
                    }
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

        // ── Bundle & Rename ──────────────────────────────────────────────────
        Label { text: "Bundle && Rename"; font.bold: true }

        CheckBox {
            id: chkTrashOriginal
            text: "Trash original ROM after Bundle && Rename completes?"
            checked: settingsController.boolValue("gui/trash_original_after_bundle")
            onCheckedChanged: settingsController.setValue("gui/trash_original_after_bundle", checked)

            Connections {
                target: settingsController
                function onSettingsChanged() {
                    chkTrashOriginal.checked = settingsController.boolValue("gui/trash_original_after_bundle")
                }
            }
        }
        Label {
            text: chkTrashOriginal.checked
                  ? "After bundling, the original ROM will be sent to the system trash."
                  : "After bundling, the original ROM will be moved to an 'original_roms' folder in the scan directory."
            font.pixelSize: 11
            color: "#928374"
            leftPadding: 28
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
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