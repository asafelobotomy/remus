import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Remus.Gui
// Note: Theme singleton is available via Remus.Gui module

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

    FolderDialog {
        id: organizeDestFolderDialog
        title: "Select organize destination"
        onAccepted: {
            const path = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""))
            settingsController.setValue("gui/organize_destination", path)
        }
    }

    FolderDialog {
        id: romSourceFolderDialog
        title: "Select ROM source folder"
        onAccepted: {
            const path = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""))
            settingsController.setValue("gui/rom_source_directory", path)
            if (path.length > 0)
                scanController.lastDirectory = path
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
            font.pixelSize: Theme.fontHero
            font.bold: true
            color: Theme.textPrimary
        }

        // ── Library ──────────────────────────────────────────────────────────
        Label { text: "Library"; font.bold: true; color: Theme.textBody }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSm
            color: Theme.textDim
            text: "Configure where your original ROM files live. Update library scans this folder into the open database."
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.preferredWidth: 200
                text: "ROM source folder"
                font.pixelSize: 12
            }

            TextField {
                id: romSourceField
                Layout.fillWidth: true
                font.pixelSize: 12
                placeholderText: "Select the folder containing your ROM collection"
                text: settingsController.stringValue("gui/rom_source_directory", scanController.lastDirectory)
                onEditingFinished: {
                    settingsController.setValue("gui/rom_source_directory", text.trim())
                    if (text.trim().length > 0)
                        scanController.lastDirectory = text.trim()
                }

                Connections {
                    target: settingsController
                    function onSettingsChanged() {
                        romSourceField.text = settingsController.stringValue(
                            "gui/rom_source_directory", scanController.lastDirectory)
                    }
                }
            }

            Button {
                text: "Browse"
                flat: true
                font.pixelSize: Theme.fontSm
                padding: 6
                onClicked: romSourceFolderDialog.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.preferredWidth: 200
                text: "Default library database"
                font.pixelSize: 12
            }

            TextField {
                id: defaultDbField
                Layout.fillWidth: true
                font.pixelSize: 12
                placeholderText: appController.defaultLibraryPath()
                text: settingsController.stringValue("gui/default_library_path", appController.defaultLibraryPath())
                onEditingFinished: settingsController.setValue("gui/default_library_path", text.trim())

                Connections {
                    target: settingsController
                    function onSettingsChanged() {
                        defaultDbField.text = settingsController.stringValue(
                            "gui/default_library_path", appController.defaultLibraryPath())
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        // ── Metadata Providers ───────────────────────────────────────────────
        Label { text: "Metadata Providers"; font.bold: true; color: Theme.textBody }

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
                    font.pixelSize: Theme.fontMd
                    color: Theme.textMuted
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
                        font.pixelSize: Theme.fontSm
                        padding: 6
                        onClicked: {
                            const msg = settingsController.authenticateProvider(modelData.groupKey)
                            authResultLabel.text = msg
                            authResultLabel.color = msg === "Credentials saved." ? Theme.success : Theme.error
                        }
                    }

                    Label {
                        id: authResultLabel
                        font.pixelSize: Theme.fontSm
                        color: Theme.textMuted
                    }

                    Item { Layout.fillWidth: true }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.borderSub
                }
            }
        }

        Label { text: "Metadata rate limits (ms)"; font.bold: true; font.pixelSize: Theme.fontMd; color: Theme.textBody }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSm
            color: Theme.textDim
            text: "Optional spacing between HTTP metadata requests. Leave blank to use built-in defaults."
        }

        Repeater {
            model: [
                { key: "metadata/rate_limit_ms", label: "Global override" },
                { key: "metadata/rate_limit/hasheous", label: "Hasheous" },
                { key: "metadata/rate_limit/screenscraper", label: "ScreenScraper" },
                { key: "metadata/rate_limit/igdb", label: "IGDB" },
                { key: "metadata/rate_limit/thegamesdb", label: "TheGamesDB" },
                { key: "metadata/rate_limit/playmatch", label: "PlayMatch" },
                { key: "metadata/rate_limit/retroachievements", label: "RetroAchievements" }
            ]

            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true

                Label {
                    Layout.preferredWidth: 200
                    text: modelData.label
                    font.pixelSize: 12
                }

                TextField {
                    id: rateLimitField
                    Layout.preferredWidth: 120
                    font.pixelSize: 12
                    placeholderText: "default"
                    text: settingsController.stringValue(modelData.key, "")
                    inputMethodHints: Qt.ImhDigitsOnly
                    onEditingFinished: settingsController.setValue(modelData.key, text.trim())

                    Connections {
                        target: settingsController
                        function onSettingsChanged() {
                            rateLimitField.text = settingsController.stringValue(modelData.key, "")
                        }
                    }
                }
            }
        }

        // ── Tools and Paths ──────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Tools and Paths"; font.bold: true; Layout.fillWidth: true; color: Theme.textBody }
            Button {
                text:    "Auto-Detect Tools"
                font.pixelSize: Theme.fontSm
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
                    font.pixelSize: Theme.fontSm
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
            color: Theme.border
        }

        // ── Rename & Organize ────────────────────────────────────────────────
        Label { text: "Rename & Organize"; font.bold: true; color: Theme.textBody }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.preferredWidth: 200
                text: "Naming template"
                font.pixelSize: 12
            }

            ComboBox {
                id: namingTemplateCombo
                Layout.fillWidth: true
                editable: true
                model: ["{title} ({region})",
                        "{title}",
                        "{title} ({year})",
                        "{title} ({system})",
                        "{title} ({region}) [{system}]"]
                font.pixelSize: 12
                Component.onCompleted: syncNamingTemplate()
                onActivated: saveNamingTemplate()
                onEditTextChanged: {
                    if (editable && activeFocus)
                        organizeController.namingTemplate = editText.length > 0 ? editText : model[0]
                }
                // ComboBox dropped editingFinished in Qt 6.7+; use accepted (Return key)
                // and focus loss instead.
                onAccepted: saveNamingTemplate()
                onActiveFocusChanged: if (!activeFocus) saveNamingTemplate()

                function syncNamingTemplate() {
                    const current = organizeController.namingTemplate
                    const idx = model.indexOf(current)
                    if (idx >= 0)
                        currentIndex = idx
                    else
                        editText = current
                }

                function saveNamingTemplate() {
                    organizeController.namingTemplate = editText.length > 0 ? editText : model[0]
                }

                Connections {
                    target: organizeController
                    function onNamingTemplateChanged() {
                        namingTemplateCombo.syncNamingTemplate()
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSm
            color: Theme.textDim
            text: "Variables: {title}, {region}, {year}, {system}, {publisher}, {ext}, and more."
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.preferredWidth: 200
                text: "Folder naming scheme"
                font.pixelSize: 12
            }

            ComboBox {
                id: folderSchemeCombo
                Layout.fillWidth: true
                model: organizeController.folderSchemeChoices()
                textRole: "label"
                valueRole: "value"
                font.pixelSize: 12
                Component.onCompleted: syncFolderScheme()
                onActivated: {
                    if (currentIndex >= 0 && model[currentIndex])
                        settingsController.setValue("organize/folder_scheme", model[currentIndex].value)
                }

                function syncFolderScheme() {
                    const current = settingsController.stringValue("organize/folder_scheme", "default")
                    let idx = 0
                    for (let i = 0; i < count; ++i) {
                        if (model[i] && model[i].value === current) {
                            idx = i
                            break
                        }
                    }
                    currentIndex = idx
                }

                Connections {
                    target: settingsController
                    function onSettingsChanged() { folderSchemeCombo.syncFolderScheme() }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.preferredWidth: 200
                text: "Organize destination"
                font.pixelSize: 12
            }

            TextField {
                id: organizeDestField
                Layout.fillWidth: true
                font.pixelSize: 12
                placeholderText: "/path/to/emulator-libraries"
                text: settingsController.stringValue("gui/organize_destination", "")
                onEditingFinished: settingsController.setValue("gui/organize_destination", text)

                Connections {
                    target: settingsController
                    function onSettingsChanged() {
                        organizeDestField.text = settingsController.stringValue("gui/organize_destination", "")
                    }
                }
            }

            Button {
                text: "Browse"
                flat: true
                font.pixelSize: Theme.fontSm
                padding: 6
                onClicked: organizeDestFolderDialog.open()
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSm
            color: Theme.textDim
            leftPadding: 200
            text: "ROMs are organized into <i>destination/Remus Library/{system}/\u2026</i>"
            textFormat: Text.RichText
        }

        CheckBox {
            id: chkOrganizeBySystem
            text: "Group folders by system"
            checked: settingsController.boolValue("organize/by_system", true)
            onCheckedChanged: settingsController.setValue("organize/by_system", checked)

            Connections {
                target: settingsController
                function onSettingsChanged() {
                    chkOrganizeBySystem.checked = settingsController.boolValue("organize/by_system", true)
                }
            }
        }

        CheckBox {
            id: chkTrashOriginal
            text: "Trash original ROM after bundle completes"
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
            font.pixelSize: Theme.fontSm
            color: Theme.textDim
            leftPadding: 28
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        // ── Danger Zone ──────────────────────────────────────────────────────
        Label {
            text: "Danger Zone"
            font.bold: true
            color: Theme.error
        }

        Label {
            Layout.fillWidth: true
            text: "Erase Library Database removes selected data from the library. Choose what to clear below. ROM files on disk are never affected."
            wrapMode: Text.WordWrap
            color: Theme.textMuted
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
                    color: Theme.textMuted
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

                Rectangle { height: 1; color: Theme.border; Layout.fillWidth: true }

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
                    font.pixelSize: Theme.fontSm
                    color: Theme.textDim
                    leftPadding: 28
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                CheckBox {
                    id: chkMatches
                    text: "Match results & game metadata"
                    checked: true
                    enabled: !chkFiles.checked
                    onCheckedChanged: updateOkButton()
                }
                Label {
                    text: "Identified game titles, systems, and match confidence scores."
                    font.pixelSize: Theme.fontSm
                    color: Theme.textDim
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
                    font.pixelSize: Theme.fontSm
                    color: Theme.textDim
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
                    font.pixelSize: Theme.fontSm
                    color: Theme.textDim
                    leftPadding: 28
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                Label {
                    visible: chkFiles.checked
                    text: "\u26A0  File records removal also clears match results."
                    font.pixelSize: Theme.fontSm
                    color: Theme.warn
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