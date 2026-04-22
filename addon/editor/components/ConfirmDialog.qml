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

    property string titleText: qsTr("Confirm")
    property string messageText: ""
    property string confirmText: qsTr("Delete")
    property string cancelText: qsTr("Cancel")
    property var onConfirmed: null

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLg
        border.color: Theme.border
        border.width: 1
    }

    Overlay.modal: Rectangle {
        color: "#99000000"
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
            font.pixelSize: 15
            font.weight: Font.Medium
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingLg
            Layout.rightMargin: Theme.spacingLg
            text: root.messageText
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.spacingLg
            Layout.topMargin: 0
            spacing: Theme.spacingSm

            Item { Layout.fillWidth: true }

            Button {
                id: cancelBtn
                text: root.cancelText
                implicitHeight: 34
                contentItem: Text {
                    text: cancelBtn.text
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    leftPadding: Theme.spacingMd
                    rightPadding: Theme.spacingMd
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: cancelBtn.hovered ? Theme.surfaceHover : Theme.background
                    border.color: Theme.border
                    border.width: 1
                }
                onClicked: root.close()
            }

            Button {
                id: confirmBtn
                text: root.confirmText
                implicitHeight: 34
                contentItem: Text {
                    text: confirmBtn.text
                    color: "#ffffff"
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    leftPadding: Theme.spacingMd
                    rightPadding: Theme.spacingMd
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: confirmBtn.hovered ? "#ef4444" : Theme.error
                    Behavior on color { ColorAnimation { duration: Theme.animShort } }
                }
                Keys.onReturnPressed: clicked()
                onClicked: {
                    if (root.onConfirmed) root.onConfirmed();
                    root.close();
                }
            }
        }
    }

    onOpened: confirmBtn.forceActiveFocus()
}
