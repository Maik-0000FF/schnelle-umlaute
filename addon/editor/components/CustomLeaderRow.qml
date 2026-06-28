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

    readonly property bool invalidChar:
        keyValue.length > 0 && !isValidSingleChar(keyValue)

    // inputErrorFor reads model state that QML can't track through a method
    // call, so bump this tick whenever the mapping model changes and reference
    // it in conflictsWithMapping to force re-evaluation.
    property int mappingTick: 0
    Connections {
        target: root.mappingsModel
        function onRowsInserted() { root.mappingTick++; }
        function onRowsRemoved() { root.mappingTick++; }
        function onDataChanged() { root.mappingTick++; }
        function onModelReset() { root.mappingTick++; }
    }

    readonly property bool conflictsWithMapping: {
        mappingTick; // establish dependency
        return keyValue.length > 0 && mappingsModel &&
            isValidSingleChar(keyValue) &&
            mappingsModel.inputErrorFor(keyValue, -1).indexOf("already") >= 0;
    }

    function isValidSingleChar(s) {
        if (!s || s.length === 0) return false;
        // Array.from iterates by codepoint — correctly handles surrogate pairs
        // (emoji = 1 codepoint, length 2 in UTF-16 units).
        return Array.from(s).length === 1 && !/\s/.test(s);
    }

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

        ThemedTextField {
            id: keyField
            Layout.preferredWidth: 80
            text: root.keyValue
            placeholderText: root.placeholderHint
            maximumLength: 4
            font.family: Theme.fontFamilyMono
            font.pixelSize: 14
            horizontalAlignment: TextInput.AlignHCenter
            background: Rectangle {
                radius: Theme.radiusSm
                color: Theme.background
                border.color: root.invalidChar
                    ? Theme.error
                    : (root.conflictsWithMapping
                        ? Theme.warning
                        : (keyField.activeFocus ? Theme.accent : Theme.border))
                border.width: 1
                Behavior on border.color { ColorAnimation { duration: Theme.animShort } }
            }
            onTextChanged: {
                if (text !== root.keyValue) {
                    root.keyEdited(text);
                }
            }
        }

        Text {
            visible: root.invalidChar
            Layout.fillWidth: true
            text: qsTr("Must be a single non-whitespace character")
            color: Theme.error
            font.family: Theme.fontFamily
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Text {
            visible: root.conflictsWithMapping && !root.invalidChar
            Layout.fillWidth: true
            text: qsTr("Warning: this key is already a mapping input")
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }
}
