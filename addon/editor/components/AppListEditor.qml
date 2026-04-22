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
        font.pixelSize: 13
        font.weight: Font.Medium
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingSm

        TextField {
            id: inputField
            Layout.fillWidth: true
            placeholderText: qsTr("e.g. firefox or libreoffice")
            font.family: Theme.fontFamilyMono
            font.pixelSize: 13
            color: Theme.text
            placeholderTextColor: Theme.textMuted
            selectByMouse: true
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
            text: "+"
            enabled: inputField.text.trim().length > 0
            implicitWidth: 36
            implicitHeight: 34
            contentItem: Text {
                text: addBtn.text
                color: addBtn.enabled ? "#ffffff" : Theme.textMuted
                font.pixelSize: 16
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
        spacing: 2
        visible: root.items.length > 0

        Repeater {
            model: root.items
            delegate: Rectangle {
                required property int index
                required property string modelData
                Layout.fillWidth: true
                height: 36
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
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }

                    ToolButton {
                        text: "🗑"
                        contentItem: Text {
                            text: parent.text
                            color: parent.hovered ? Theme.error : Theme.textMuted
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? Theme.surface : "transparent"
                            radius: Theme.radiusSm
                        }
                        onClicked: root.removeRequested(index)
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
        font.pixelSize: 11
        font.italic: true
    }

    function commit() {
        const t = inputField.text.trim();
        if (t.length === 0) return;
        root.addRequested(t);
        inputField.clear();
    }
}
