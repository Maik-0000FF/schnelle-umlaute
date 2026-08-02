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
                                           ? Theme.accentHover : Theme.errorHover

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
            // Callers embed user data (a mapping value, a profile name, an app
            // id), so the message is always shown literally.
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingLg
            Layout.topMargin: 0
            spacing: Theme.spacingSm

            Item { Layout.fillWidth: true }

            // Neutral cancel button. FocusRect supplies the hover + focus +
            // Space/Enter handling (and the reason these are Rectangles rather
            // than Quick Controls Buttons).
            FocusRect {
                id: cancelBtn
                visible: !root.singleButton
                implicitHeight: Theme.controlHeight
                implicitWidth: cancelLabel.implicitWidth + 2 * Theme.spacingMd
                color: (cancelBtn.hovered || cancelBtn.activeFocus)
                       ? Theme.surfaceHover : Theme.background
                // Keyboard focus adds an accent border on top of the hover fill.
                border.color: cancelBtn.activeFocus ? Theme.borderFocus
                                                    : Theme.border
                border.width: 1
                onActivated: root.close()

                Text {
                    id: cancelLabel
                    anchors.centerIn: parent
                    text: root.cancelText
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                }
            }

            // Filled confirm button: hover and keyboard focus both show as the
            // hover shade of the fill, no separate ring.
            FocusRect {
                id: confirmBtn
                implicitHeight: Theme.controlHeight
                implicitWidth: confirmLabel.implicitWidth + 2 * Theme.spacingMd
                color: (confirmBtn.hovered || confirmBtn.activeFocus)
                       ? root._confirmHover : root._confirmBase
                onActivated: {
                    if (root.onConfirmed) root.onConfirmed();
                    root.close();
                }

                Text {
                    id: confirmLabel
                    anchors.centerIn: parent
                    text: root.confirmText
                    color: Theme.accentText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    font.weight: Font.Medium
                }
            }
        }
    }

    onOpened: confirmBtn.forceActiveFocus()
}
