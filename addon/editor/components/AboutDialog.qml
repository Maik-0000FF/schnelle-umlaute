import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

// Modal "About" dialog opened from the header ⓘ button. Shows the app
// identity + version and offers external links (repo, issue tracker,
// license). Mirrors ConfirmDialog's modal / scrim / paddings so it reads as
// the same dialog family. The version comes from the appVersion context
// property (a single source fed from the CMake project version).
Popup {
    id: root
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0
    implicitWidth: Theme.aboutDialogWidth

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLg
        border.color: Theme.border
        border.width: 1
    }
    Overlay.modal: Rectangle { color: Theme.scrim }

    // One reusable link row: an accent-coloured label that opens `url`
    // externally on click, Space or Enter. Keyboard-reachable like the rest
    // of the editor.
    component LinkRow: FocusRect {
        id: link
        property string label: ""
        property string url: ""
        Layout.fillWidth: true
        implicitHeight: Theme.controlHeight
        color: (link.hovered || link.activeFocus)
               ? Theme.surfaceHover : "transparent"
        border.color: link.activeFocus ? Theme.borderFocus : "transparent"
        border.width: 1
        onActivated: Qt.openUrlExternally(link.url)

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacingMd
            anchors.verticalCenter: parent.verticalCenter
            text: link.label
            color: Theme.accent
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingMd

        // Identity: icon + name + version.
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingLg
            Layout.bottomMargin: 0
            spacing: Theme.spacingMd

            Image {
                source: Theme.appIconSource
                sourceSize.width: Theme.appIconSizeLg * 2 // 2x for HiDPI crispness
                sourceSize.height: Theme.appIconSizeLg * 2
                width: Theme.appIconSizeLg
                height: Theme.appIconSizeLg
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
            ColumnLayout {
                spacing: Theme.spacingXxs
                Wordmark {}
                Text {
                    text: qsTr("Version %1").arg(appVersion)
                    color: Theme.textMuted
                    font.family: Theme.fontFamilyMono
                    font.pixelSize: Theme.fontBody
                }
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingLg
            Layout.rightMargin: Theme.spacingLg
            text: qsTr("Fast input of any Unicode character via fcitx5.")
            color: Theme.textMuted
            wrapMode: Text.WordWrap
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingLg
            Layout.rightMargin: Theme.spacingLg
            text: qsTr("Developed by %1").arg(Theme.developerName)
            color: Theme.textMuted
            wrapMode: Text.WordWrap
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingMd
            Layout.rightMargin: Theme.spacingMd
            spacing: Theme.spacingXs

            LinkRow { label: qsTr("View on GitHub");  url: Theme.repoUrl }
            LinkRow { label: qsTr("Report an issue"); url: Theme.issuesUrl }
            LinkRow {
                label: qsTr("License: %1").arg(Theme.licenseName)
                url: Theme.licenseUrl
            }
        }

        // Close button, reusing ConfirmDialog's neutral-button styling.
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingLg
            Layout.topMargin: 0
            Item { Layout.fillWidth: true }
            FocusRect {
                id: closeBtn
                implicitHeight: Theme.controlHeight
                implicitWidth: closeLabel.implicitWidth + 2 * Theme.spacingMd
                color: (closeBtn.hovered || closeBtn.activeFocus)
                       ? Theme.surfaceHover : Theme.background
                border.color: closeBtn.activeFocus ? Theme.borderFocus : Theme.border
                border.width: 1
                onActivated: root.close()

                Text {
                    id: closeLabel
                    anchors.centerIn: parent
                    text: qsTr("Close")
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                }
            }
        }
    }

    onOpened: closeBtn.forceActiveFocus()
}
