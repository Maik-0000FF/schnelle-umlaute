import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

ColumnLayout {
    id: root
    Layout.fillWidth: true
    spacing: Theme.spacingSm

    property string labelText: ""
    property var items: []
    signal addRequested(string entry)
    signal removeRequested(int index)

    Text {
        text: root.labelText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        font.weight: Font.Medium
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingSm

        ThemedTextField {
            id: inputField
            Layout.fillWidth: true
            placeholderText: qsTr("e.g. firefox or libreoffice")
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontBody
            background: Rectangle {
                radius: Theme.radiusSm
                color: Theme.background
                border.color: inputField.activeFocus ? Theme.accent : Theme.border
                border.width: 1
            }
            onAccepted: commit()
        }

        Button {
            id: addBtn
            // Keyboard-reachable via Tab, but must not grab focus on click.
            focusPolicy: Qt.TabFocus
            text: Theme.iconAdd
            enabled: inputField.text.trim().length > 0
            implicitWidth: 36
            implicitHeight: Theme.controlHeight
            contentItem: Text {
                text: addBtn.text
                color: addBtn.enabled ? Theme.switchThumb : Theme.textMuted
                font.pixelSize: Theme.fontStrong
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                radius: Theme.radiusSm
                color: addBtn.enabled
                    ? (addBtn.hovered ? Theme.accentHover : Theme.accent)
                    : Theme.surfaceHover
            }
            onClicked: commit()
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingXxs
        visible: root.items.length > 0

        Repeater {
            model: root.items
            delegate: Rectangle {
                required property int index
                required property string modelData
                Layout.fillWidth: true
                height: Theme.rowHeight
                radius: Theme.radiusSm
                color: Theme.background
                border.color: Theme.border
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMd
                    anchors.rightMargin: Theme.spacingSm
                    spacing: Theme.spacingSm

                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: Theme.text
                        font.family: Theme.fontFamilyMono
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                    }

                    ToolButton {
                        // Keyboard-reachable via Tab, but must not grab focus
                        // on click.
                        focusPolicy: Qt.TabFocus
                        text: Theme.iconTrash
                        contentItem: Text {
                            text: parent.text
                            color: parent.hovered ? Theme.error : Theme.textMuted
                            font.pixelSize: Theme.fontIcon
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? Theme.surface : "transparent"
                            radius: Theme.radiusSm
                        }
                        onClicked: {
                            confirmDialog.messageText = qsTr(
                                "Remove “%1” from the list?").arg(modelData);
                            confirmDialog.onConfirmed = () => root.removeRequested(index);
                            confirmDialog.open();
                        }
                    }
                }
            }
        }
    }

    Text {
        visible: root.items.length === 0
        text: qsTr("No entries")
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        font.italic: true
    }

    ConfirmDialog {
        id: confirmDialog
        titleText: qsTr("Remove entry")
        confirmText: qsTr("Remove")
    }

    function commit() {
        const t = inputField.text.trim();
        if (t.length === 0) return;
        root.addRequested(t);
        inputField.clear();
    }
}
