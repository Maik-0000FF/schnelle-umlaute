import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Popup {
    id: root
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: 0
    // Cap the dialog at a comfortable reading width so long message
    // bodies stop at one column instead of stretching to the host
    // window's edges. 420 px ≈ 60–70 chars of Inter at 13 px, which is
    // inside the recommended line-length range for body copy.
    implicitWidth: 420

    property string titleText: qsTr("Confirm")
    property string messageText: ""
    property string confirmText: qsTr("Delete")
    property string cancelText: qsTr("Cancel")
    property var onConfirmed: null

    // "destructive" → red confirm button (delete-style, default to keep
    // existing call-sites unchanged). "primary" → accent-coloured confirm
    // button for constructive actions like "set up", "apply", "install".
    property string confirmStyle: "destructive"
    readonly property color _confirmBase: confirmStyle === "primary"
                                          ? Theme.accent : Theme.error
    readonly property color _confirmHover: confirmStyle === "primary"
                                           ? Theme.accentHover : "#ef4444"

    // When true, the cancel button is hidden and only the confirm button
    // is shown. Use for informational dialogs where there is no
    // destructive option to opt out of — e.g. "Logout pending" reminders.
    property bool singleButton: false

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLg
        border.color: Theme.border
        border.width: 1
    }

    Overlay.modal: Rectangle {
        color: Theme.scrim
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingLg

        Text {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingLg
            Layout.bottomMargin: 0
            text: root.titleText
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontStrong
            font.weight: Font.Medium
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingLg
            Layout.rightMargin: Theme.spacingLg
            text: root.messageText
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingLg
            Layout.topMargin: 0
            spacing: Theme.spacingSm

            Item { Layout.fillWidth: true }

            // Plain Rectangle + MouseArea instead of Button: the Quick
            // Controls Basic Button silently substitutes a system-palette
            // colour for the contentItem text on light desktops, even when
            // contentItem.color and palette.buttonText are both bound to
            // a theme colour. Same workaround pattern as PositionPicker's
            // selection cell — Rectangle leaves the colour pipeline alone.
            Rectangle {
                id: cancelBtn
                visible: !root.singleButton
                implicitHeight: 34
                implicitWidth: cancelLabel.implicitWidth + 2 * Theme.spacingMd
                radius: Theme.radiusSm
                color: cancelMouse.containsMouse ? Theme.surfaceHover
                                                 : Theme.background
                border.color: Theme.border
                border.width: 1
                Behavior on color { ColorAnimation { duration: Theme.animShort } }

                Text {
                    id: cancelLabel
                    anchors.centerIn: parent
                    text: root.cancelText
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                }
                MouseArea {
                    id: cancelMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }

            Rectangle {
                id: confirmBtn
                implicitHeight: 34
                implicitWidth: confirmLabel.implicitWidth + 2 * Theme.spacingMd
                radius: Theme.radiusSm
                color: confirmMouse.containsMouse ? root._confirmHover
                                                  : root._confirmBase
                Behavior on color { ColorAnimation { duration: Theme.animShort } }

                Text {
                    id: confirmLabel
                    anchors.centerIn: parent
                    text: root.confirmText
                    color: Theme.switchThumb
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.Medium
                }
                MouseArea {
                    id: confirmMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.onConfirmed) root.onConfirmed();
                        root.close();
                    }
                }
                Keys.onReturnPressed: {
                    if (root.onConfirmed) root.onConfirmed();
                    root.close();
                }
            }
        }
    }

    onOpened: confirmBtn.forceActiveFocus()
}
