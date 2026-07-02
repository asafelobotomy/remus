import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Remus.Gui

Item {
    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        Label {
            text: "Verification"
            font.pixelSize: Theme.fontHero
            font.bold: true
            color: Theme.textPrimary
        }

        RowLayout {
            Button {
                text: "Verify All"
                enabled: !verificationController.verifying
                onClicked: verificationController.verifyAll()
            }
            Button {
                text: "Verify Selected"
                enabled: !verificationController.verifying && appController.selectedFileId > 0
                onClicked: verificationController.verifySelected()
            }
            Button {
                text: "Clear"
                onClicked: verificationController.clearResults()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textMuted
                text: "Import supplemental DAT catalogs from the Tools \u2192 DAT Import tab before verifying."
            }
        }

        ProgressCard {
            title: "Verification Progress"
            progressValue: verificationController.progress
            progressTotal: verificationController.total
            message: verificationController.currentFile.length > 0 ? verificationController.currentFile : verificationController.lastError
        }

        Label {
            text: "Verified: " + (verificationController.summary.verified || 0) + " • Mismatch: " + (verificationController.summary.mismatched || 0) + " • Missing: " + (verificationController.summary.notInDat || 0)
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: verificationResultModel

            delegate: Frame {
                id: resultDelegate

                required property string filename
                required property string system
                required property string status
                required property string hashType
                required property string notes

                width: ListView.view.width
                padding: 12
                implicitHeight: contentColumn.implicitHeight + topPadding + bottomPadding

                ColumnLayout {
                    id: contentColumn

                    x: resultDelegate.leftPadding
                    y: resultDelegate.topPadding
                    width: resultDelegate.availableWidth
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        text: resultDelegate.filename
                        font.bold: true
                        elide: Text.ElideMiddle
                    }
                    Label {
                        Layout.fillWidth: true
                        text: resultDelegate.system + " • " + resultDelegate.status + " • " + resultDelegate.hashType
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: resultDelegate.notes
                    }
                }
            }
        }
    }
}
