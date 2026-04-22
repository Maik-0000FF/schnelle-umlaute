import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Rectangle {
    id: root
    radius: Theme.radiusMd
    color: mouseArea.containsMouse ? Theme.surfaceHover : "transparent"
    border.color: editing ? Theme.borderFocus : "transparent"
    border.width: 1
    height: 52

    Behavior on color { ColorAnimation { duration: Theme.animShort } }
    Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

    property int rowIndex: -1
    property string inputText: ""
    property string outputText: ""
    property var modelRef: null
    property bool editing: false

    signal removeRequested()

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingSm
        spacing: Theme.spacingMd

        Rectangle {
            width: 44
            height: 32
            radius: Theme.radiusSm
            color: Theme.background
            border.color: Theme.border
            border.width: 1
            visible: !root.editing

            Text {
                anchors.centerIn: parent
                text: root.inputText
                color: Theme.text
                font.family: Theme.fontFamilyMono
                font.pixelSize: 15
            }
        }

        TextField {
            id: inputEdit
            visible: root.editing
            Layout.preferredWidth: 80
            text: root.inputText
            maximumLength: 4
            font.family: Theme.fontFamilyMono
            font.pixelSize: 15
            horizontalAlignment: TextInput.AlignHCenter
            color: Theme.text
            selectByMouse: true
            background: Rectangle {
                radius: Theme.radiusSm
                color: Theme.background
                border.color: Theme.accent
                border.width: 1
            }
        }

        Text {
            text: "→"
            color: Theme.textMuted
            font.pixelSize: 14
        }

        Text {
            Layout.fillWidth: true
            visible: !root.editing
            text: root.outputText
            color: Theme.text
            font.family: Theme.fontFamilyMono
            font.pixelSize: 15
            elide: Text.ElideRight
        }

        TextField {
            id: outputEdit
            visible: root.editing
            Layout.fillWidth: true
            text: root.outputText
            font.family: Theme.fontFamilyMono
            font.pixelSize: 15
            color: Theme.text
            selectByMouse: true
            background: Rectangle {
                radius: Theme.radiusSm
                color: Theme.background
                border.color: Theme.accent
                border.width: 1
            }
            onAccepted: confirmEdit()
            Keys.onEscapePressed: cancelEdit()
        }

        ToolButton {
            text: root.editing ? "✓" : "✎"
            ToolTip.visible: hovered
            ToolTip.text: root.editing ? qsTr("Apply") : qsTr("Edit")
            contentItem: Text {
                text: parent.text
                color: parent.hovered ? Theme.accent : Theme.textMuted
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: parent.hovered ? Theme.surface : "transparent"
                radius: Theme.radiusSm
            }
            onClicked: {
                if (root.editing) confirmEdit();
                else startEdit();
            }
        }

        ToolButton {
            text: root.editing ? "✗" : "🗑"
            ToolTip.visible: hovered
            ToolTip.text: root.editing ? qsTr("Cancel") : qsTr("Delete")
            contentItem: Text {
                text: parent.text
                color: parent.hovered ? Theme.error : Theme.textMuted
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: parent.hovered ? Theme.surface : "transparent"
                radius: Theme.radiusSm
            }
            onClicked: {
                if (root.editing) cancelEdit();
                else root.removeRequested();
            }
        }
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Delete && !root.editing) {
            root.removeRequested();
            event.accepted = true;
        }
    }

    function startEdit() {
        inputEdit.text = root.inputText;
        outputEdit.text = root.outputText;
        root.editing = true;
        outputEdit.forceActiveFocus();
    }

    function confirmEdit() {
        if (root.modelRef && root.modelRef.updateMapping(
                root.rowIndex, inputEdit.text, outputEdit.text)) {
            root.editing = false;
        }
    }

    function cancelEdit() {
        root.editing = false;
    }
}
