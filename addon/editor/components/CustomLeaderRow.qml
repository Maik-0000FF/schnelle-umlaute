import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

ColumnLayout {
    id: root
    Layout.fillWidth: true
    spacing: Theme.spacingSm

    property string labelText: ""
    property string placeholderHint: "e.g. ; or #"
    property bool enabledValue: false
    property string keyValue: ""
    property var mappingsModel: null
    signal enabledEdited(bool v)
    signal keyEdited(string v)

    readonly property bool conflictsWithMapping:
        keyValue.length > 0 && mappingsModel &&
        !mappingsModel.validateInput(keyValue, -1) &&
        mappingsModel.inputErrorFor(keyValue, -1).indexOf("already") >= 0

    LabeledSwitch {
        labelText: root.labelText
        checked: root.enabledValue
        onToggled: (v) => root.enabledEdited(v)
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Theme.spacingMd
        spacing: Theme.spacingMd
        visible: root.enabledValue

        Text {
            text: qsTr("Key")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: 12
            Layout.preferredWidth: 40
        }

        TextField {
            id: keyField
            Layout.preferredWidth: 80
            text: root.keyValue
            placeholderText: root.placeholderHint
            maximumLength: 4
            font.family: Theme.fontFamilyMono
            font.pixelSize: 14
            horizontalAlignment: TextInput.AlignHCenter
            color: Theme.text
            placeholderTextColor: Theme.textMuted
            selectByMouse: true
            background: Rectangle {
                radius: Theme.radiusSm
                color: Theme.background
                border.color: root.conflictsWithMapping
                    ? Theme.warning
                    : (keyField.activeFocus ? Theme.accent : Theme.border)
                border.width: 1
                Behavior on border.color { ColorAnimation { duration: Theme.animShort } }
            }
            onEditingFinished: root.keyEdited(text)
        }

        Text {
            visible: root.conflictsWithMapping
            Layout.fillWidth: true
            text: qsTr("Warning: this key is already a mapping input")
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }
}
