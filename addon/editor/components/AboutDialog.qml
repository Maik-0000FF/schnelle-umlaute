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
    implicitWidth: 380

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
    component LinkRow: Rectangle {
        id: link
        property string label: ""
        property string url: ""
        Layout.fillWidth: true
        implicitHeight: Theme.controlHeight
        radius: Theme.radiusSm
        color: (linkMouse.containsMouse || link.activeFocus)
               ? Theme.surfaceHover : "transparent"
        border.color: link.activeFocus ? Theme.borderFocus : "transparent"
        border.width: 1
        activeFocusOnTab: true
        Behavior on color { ColorAnimation { duration: Theme.animShort } }

        function open() { Qt.openUrlExternally(link.url); }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacingMd
            anchors.verticalCenter: parent.verticalCenter
            text: link.label
            color: Theme.accent
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
        }
        MouseArea {
            id: linkMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: link.open()
        }
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return
                || event.key === Qt.Key_Enter) {
                link.open();
                event.accepted = true;
            }
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
                source: "qrc:/qt/qml/SchnelleUmlaute/assets/schnelle-umlaute-icon.png"
                sourceSize.width: 96
                sourceSize.height: 96
                width: 48
                height: 48
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
            ColumnLayout {
                spacing: Theme.spacingXxs
                RowLayout {
                    spacing: 6
                    Text {
                        text: "Schnelle"
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontStrong
                        font.weight: Font.Medium
                    }
                    Text {
                        text: "Umlaute"
                        color: Theme.brand
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontStrong
                        font.weight: Font.Medium
                    }
                }
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
            text: qsTr("Fast diacritics for Wayland via fcitx5.")
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
            Rectangle {
                id: closeBtn
                implicitHeight: Theme.controlHeight
                implicitWidth: closeLabel.implicitWidth + 2 * Theme.spacingMd
                radius: Theme.radiusSm
                color: (closeMouse.containsMouse || closeBtn.activeFocus)
                       ? Theme.surfaceHover : Theme.background
                border.color: closeBtn.activeFocus ? Theme.borderFocus : Theme.border
                border.width: 1
                activeFocusOnTab: true
                Behavior on color { ColorAnimation { duration: Theme.animShort } }
                Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

                Text {
                    id: closeLabel
                    anchors.centerIn: parent
                    text: qsTr("Close")
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                }
                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Space || event.key === Qt.Key_Return
                        || event.key === Qt.Key_Enter) {
                        root.close();
                        event.accepted = true;
                    }
                }
                Keys.onEscapePressed: root.close()
            }
        }
    }

    onOpened: closeBtn.forceActiveFocus()
}
