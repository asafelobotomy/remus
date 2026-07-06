import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

Dialog {
    id: root

    required property var buildController

    title: "Compendium Wizard"
    modal: true
    width: Math.min(760, parent ? parent.width - 48 : 760)
    height: Math.min(620, parent ? parent.height - 48 : 620)
    anchors.centerIn: Overlay.overlay

    property int step: 0
    readonly property int lastStep: 3

    property int buildMode: 0 // 0 = full, 1 = extend
    property int buildPreset: 0
    property bool skipDatUpdate: false
    property bool offlineOnly: false
    property bool strictOffline: false
    property bool forceFullRebuild: false
    property bool onlineEnrichmentAll: false
    property bool recover: false
    property bool forceEnrichment: false
    property bool allowUnresolvedConflicts: false
    property bool skipValidation: false
    property bool pruneAcquisition: false
    property bool thumbnailSnapLossless: false
    property bool skipConsolidate: false
    property bool detached: true
    property bool consolidateArtwork: false
    property var selectedExtendSources: []

    function applyPresetFlags() {
        switch (buildPreset) {
        case 1:
            skipDatUpdate = false;
            offlineOnly = true;
            strictOffline = false;
            onlineEnrichmentAll = false;
            break;
        case 2:
            skipDatUpdate = false;
            offlineOnly = true;
            strictOffline = true;
            onlineEnrichmentAll = false;
            break;
        case 3:
            skipDatUpdate = false;
            offlineOnly = false;
            strictOffline = false;
            forceFullRebuild = true;
            onlineEnrichmentAll = true;
            break;
        default:
            break;
        }
        enforceOptionExclusion();
    }

    function enforceOptionExclusion() {
        if (strictOffline) {
            offlineOnly = true;
            onlineEnrichmentAll = false;
        }
        if (offlineOnly)
            onlineEnrichmentAll = false;
        if (onlineEnrichmentAll) {
            offlineOnly = false;
            strictOffline = false;
        }
    }

    function fullBuildOptionsMap() {
        return {
            skipDatUpdate: skipDatUpdate,
            offlineOnly: offlineOnly,
            strictOffline: strictOffline,
            forceFullRebuild: forceFullRebuild,
            onlineEnrichmentAll: onlineEnrichmentAll,
            recover: recover,
            forceEnrichment: forceEnrichment,
            allowUnresolvedConflicts: allowUnresolvedConflicts,
            skipValidation: skipValidation,
            pruneAcquisition: pruneAcquisition,
            thumbnailSnapLossless: thumbnailSnapLossless,
            skipConsolidate: skipConsolidate,
            detached: detached
        };
    }

    function extendBuildOptionsMap() {
        return {
            enrichSources: selectedExtendSources,
            forceEnrichment: forceEnrichment,
            offlineOnly: offlineOnly,
            onlineEnrichmentAll: onlineEnrichmentAll,
            consolidateArtwork: consolidateArtwork,
            detached: false
        };
    }

    onAboutToShow: {
        step = 0;
        buildController.refreshPreflight();
        buildController.refreshCredentialStatus();
        buildController.reattachToRunningBuild();
    }

    onClosed: {
        if (buildController.building && !buildController.monitoringDetached && !detached)
            buildController.cancelBuild();
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 12

            Label {
                text: ["Overview", "Credentials", "Options", "Build"][root.step]
                font.bold: true
                color: Theme.textPrimary
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: "Step " + (root.step + 1) + " / " + (root.lastStep + 1)
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 14

        // ── Step 0: Overview ─────────────────────────────────────────────
        ColumnLayout {
            visible: root.step === 0
            Layout.fillWidth: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textBody
                text: "Build or extend the offline compendium (remus_compendium.db). Full builds run the complete pipeline and may take several hours."
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                RadioButton {
                    text: "Full build"
                    checked: root.buildMode === 0
                    onToggled: if (checked)
                        root.buildMode = 0
                }
                RadioButton {
                    text: "Extend source"
                    checked: root.buildMode === 1
                    onToggled: if (checked)
                        root.buildMode = 1
                }
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSm
                color: buildController.preflightReady ? Theme.success : Theme.error
                text: buildController.preflightMessage
            }

            Label {
                visible: buildController.monitoringDetached
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSm
                color: Theme.warn
                text: "A compendium build is running in the background. Progress will update here."
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSm
                color: Theme.textMuted
                text: "Output: " + buildController.compendiumDbPath
            }
        }

        // ── Step 1: Credentials ──────────────────────────────────────────
        ColumnLayout {
            visible: root.step === 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
                text: "API keys are stored in your OS keychain. IGDB requires a Twitch developer app (Client ID + Secret). SteamGridDB is runtime artwork only and does not affect compendium builds."
            }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSm
                color: Theme.textMuted
                text: "IGDB: dev.twitch.tv/console/apps  |  ScreenScraper: screenscraper.fr"
                onLinkActivated: link => Qt.openUrlExternally(link)
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 12

                    Repeater {
                        model: settingsController.providerGroups

                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 4

                            function statusForGroup(key) {
                                const rows = buildController.credentialStatusModel;
                                for (let i = 0; i < rows.length; i++) {
                                    if (rows[i].groupKey === key)
                                        return rows[i];
                                }
                                return null;
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    text: modelData.groupName
                                    font.bold: true
                                    color: Theme.textBody
                                }
                                Label {
                                    property var st: statusForGroup(modelData.groupKey)
                                    text: {
                                        if (!st)
                                            return "";
                                        if (st.runtimeOnly)
                                            return "runtime only";
                                        return st.configured ? "configured" : "not configured";
                                    }
                                    font.pixelSize: Theme.fontSm
                                    color: Theme.textMuted
                                }
                            }

                            Repeater {
                                model: modelData.fields

                                delegate: RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: true

                                    Label {
                                        Layout.preferredWidth: 180
                                        text: modelData.label
                                        font.pixelSize: Theme.fontSm
                                    }

                                    TextField {
                                        Layout.fillWidth: true
                                        font.pixelSize: Theme.fontSm
                                        echoMode: modelData.password ? TextInput.Password : TextInput.Normal
                                        text: settingsController.stringValue(modelData.key)
                                        onEditingFinished: settingsController.setValue(modelData.key, text)
                                    }
                                }
                            }

                            RowLayout {
                                Button {
                                    text: "Save"
                                    flat: true
                                    font.pixelSize: Theme.fontSm
                                    onClicked: {
                                        const msg = settingsController.authenticateProvider(modelData.groupKey);
                                        credSaveLabel.text = msg;
                                        credSaveLabel.isSuccess = msg.startsWith("Credentials saved");
                                        buildController.refreshCredentialStatus();
                                    }
                                }
                                Button {
                                    text: "Test credentials"
                                    flat: true
                                    font.pixelSize: Theme.fontSm
                                    onClicked: {
                                        const result = buildController.verifyCredentials(modelData.groupKey);
                                        credSaveLabel.text = result.message;
                                        credSaveLabel.isSuccess = result.ok;
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        id: credSaveLabel
                        property bool isSuccess: false
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontSm
                        color: isSuccess ? Theme.success : Theme.textMuted
                    }
                }
            }
        }

        // ── Step 2: Options ──────────────────────────────────────────────
        ColumnLayout {
            visible: root.step === 2
            Layout.fillWidth: true
            spacing: 10

            Label {
                visible: root.buildMode === 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
                text: "Default: refresh offline DATs, offline enrichment, then online gap-fill when credentials are set."
            }

            // Full build options
            ColumnLayout {
                visible: root.buildMode === 0
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: "Preset"
                    font.bold: true
                    color: Theme.textBody
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Repeater {
                        model: ["Default", "Offline only", "Strict offline", "Developer full"]
                        delegate: RadioButton {
                            required property int index
                            required property string modelData
                            text: modelData
                            checked: root.buildPreset === index
                            onToggled: {
                                if (checked) {
                                    root.buildPreset = index;
                                    root.applyPresetFlags();
                                }
                            }
                        }
                    }
                }

                CheckBox {
                    text: "Skip DAT / offline source refresh"
                    checked: root.skipDatUpdate
                    onToggled: root.skipDatUpdate = checked
                }
                CheckBox {
                    text: "Offline-only enrichment (no API calls)"
                    checked: root.offlineOnly
                    enabled: !root.strictOffline
                    onToggled: {
                        root.offlineOnly = checked;
                        root.enforceOptionExclusion();
                    }
                }
                CheckBox {
                    text: "Strict offline (Tier A mirrors + artwork manifest gate)"
                    checked: root.strictOffline
                    enabled: !root.onlineEnrichmentAll
                    onToggled: {
                        root.strictOffline = checked;
                        root.enforceOptionExclusion();
                    }
                }
                CheckBox {
                    text: "Force full rebuild"
                    checked: root.forceFullRebuild
                    onToggled: root.forceFullRebuild = checked
                }
                CheckBox {
                    text: "Online enrichment all (slow — per-game APIs)"
                    checked: root.onlineEnrichmentAll
                    enabled: !root.offlineOnly && !root.strictOffline
                    onToggled: {
                        root.onlineEnrichmentAll = checked;
                        root.enforceOptionExclusion();
                    }
                }
                CheckBox {
                    text: "Run detached (survives closing this dialog)"
                    checked: root.detached
                    onToggled: root.detached = checked
                }

                ToolButton {
                    text: advancedPanel.expanded ? "Hide advanced" : "Show advanced"
                    onClicked: advancedPanel.expanded = !advancedPanel.expanded
                }

                ColumnLayout {
                    id: advancedPanel
                    Layout.fillWidth: true
                    spacing: 6
                    visible: expanded
                    property bool expanded: false

                    CheckBox {
                        text: "Recover staged build"
                        checked: root.recover
                        onToggled: root.recover = checked
                    }
                    CheckBox {
                        text: "Force enrichment"
                        checked: root.forceEnrichment
                        onToggled: root.forceEnrichment = checked
                    }
                    CheckBox {
                        text: "Allow unresolved merge conflicts (exit 2)"
                        checked: root.allowUnresolvedConflicts
                        onToggled: root.allowUnresolvedConflicts = checked
                    }
                    CheckBox {
                        text: "Skip validation"
                        checked: root.skipValidation
                        onToggled: root.skipValidation = checked
                    }
                    CheckBox {
                        text: "Prune acquisition sources after consolidate"
                        checked: root.pruneAcquisition
                        onToggled: root.pruneAcquisition = checked
                    }
                    CheckBox {
                        text: "Lossless snap thumbnails"
                        checked: root.thumbnailSnapLossless
                        onToggled: root.thumbnailSnapLossless = checked
                    }
                    CheckBox {
                        text: "Skip artwork consolidate"
                        checked: root.skipConsolidate
                        onToggled: root.skipConsolidate = checked
                    }
                }
            }

            // Extend build options
            ColumnLayout {
                visible: root.buildMode === 1
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: "Enrichment sources"
                    font.bold: true
                    color: Theme.textBody
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8

                    Repeater {
                        model: buildController.enrichmentSourceKeys()

                        delegate: CheckBox {
                            required property string modelData
                            text: modelData
                            checked: root.selectedExtendSources.indexOf(modelData) >= 0
                            onToggled: {
                                const list = root.selectedExtendSources.slice();
                                const idx = list.indexOf(modelData);
                                if (checked && idx < 0)
                                    list.push(modelData);
                                else if (!checked && idx >= 0)
                                    list.splice(idx, 1);
                                root.selectedExtendSources = list;
                            }
                        }
                    }
                }

                CheckBox {
                    text: "Force enrichment"
                    checked: root.forceEnrichment
                    onToggled: root.forceEnrichment = checked
                }
                CheckBox {
                    text: "Offline-only enrichment"
                    checked: root.offlineOnly
                    onToggled: {
                        root.offlineOnly = checked;
                        root.enforceOptionExclusion();
                    }
                }
                CheckBox {
                    text: "Online enrichment all"
                    checked: root.onlineEnrichmentAll
                    enabled: !root.offlineOnly
                    onToggled: {
                        root.onlineEnrichmentAll = checked;
                        root.enforceOptionExclusion();
                    }
                }
                CheckBox {
                    text: "Consolidate artwork before enrich"
                    checked: root.consolidateArtwork
                    onToggled: root.consolidateArtwork = checked
                }
            }
        }

        // ── Step 3: Build progress ───────────────────────────────────────
        ColumnLayout {
            visible: root.step === 3
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            ProgressCard {
                Layout.fillWidth: true
                title: buildController.building ? "Building compendium…" : (buildController.hadMergeConflicts ? "Build finished with conflicts" : "Build")
                progressValue: buildController.progressValue
                progressTotal: buildController.progressTotal
                message: buildController.progressMessage
            }

            Label {
                visible: buildController.buildPhase.length > 0
                text: "Phase: " + buildController.buildPhase
                font.pixelSize: Theme.fontSm
                color: Theme.textMuted
            }

            Label {
                visible: buildController.monitoringDetached
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSm
                color: Theme.warn
                text: "Build continues in background if you close this dialog."
            }

            Label {
                visible: buildController.hadMergeConflicts && !buildController.building
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.warn
                text: "Unresolved merge conflicts (exit 2). Review the log and compendium DB before using match/enrich."
            }

            Label {
                visible: !buildController.building && buildController.buildSummary.signatureCount !== undefined
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSm
                color: Theme.textMuted
                text: "Signatures: " + buildController.buildSummary.signatureCount + "  |  Coverage: " + buildController.buildSummary.coveragePath
            }

            Label {
                visible: buildController.lastError.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.error
                text: buildController.lastError
            }

            RowLayout {
                Button {
                    text: "Open log"
                    flat: true
                    onClicked: buildController.openLogFile()
                }
                Button {
                    text: "Open output folder"
                    flat: true
                    onClicked: buildController.openOutputFolder()
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                visible: buildController.logTail.length > 0

                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    font.family: "monospace"
                    font.pixelSize: Theme.fontSm
                    color: Theme.textMuted
                    text: buildController.logTail
                }
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            text: root.step === 0 ? "Cancel" : "Back"
            enabled: !buildController.building || root.step < 3
            onClicked: {
                if (root.step === 0)
                    root.close();
                else
                    root.step -= 1;
            }
        }

        Button {
            text: root.step === root.lastStep ? "Close" : "Next"
            highlighted: true
            enabled: {
                if (root.step === 0)
                    return buildController.preflightReady || buildController.monitoringDetached;
                if (root.step === 3)
                    return !buildController.building;
                if (root.step === 2 && root.buildMode === 1)
                    return root.selectedExtendSources.length > 0;
                return true;
            }
            onClicked: {
                if (root.step === 2) {
                    buildController.syncEnrichmentCredentials();
                    root.step = 3;
                    if (root.buildMode === 0)
                        buildController.startFullBuild(root.fullBuildOptionsMap());
                    else
                        buildController.startExtendBuild(root.extendBuildOptionsMap());
                    return;
                }
                if (root.step === root.lastStep) {
                    root.close();
                    return;
                }
                root.step += 1;
            }
        }

        Button {
            visible: root.step === 3 && buildController.building && !buildController.monitoringDetached
            text: "Cancel build"
            onClicked: buildController.cancelBuild()
        }
    }
}
