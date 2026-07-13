import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

// A custom leader is a PHYSICAL key, so it is captured as a real key press
// rather than typed as a character. One press yields both halves: the character
// (shown here and checked against the mappings) and the keycode, which is what
// the addon matches and hand-classifies. See addon/src/hand_classifier.h.
ColumnLayout {
    id: root
    Layout.fillWidth: true
    spacing: Theme.spacingSm

    property string labelText: ""
    property bool enabledValue: false
    property string keyValue: ""
    // evdev+8, matching fcitx5's Key::code(). kNoKeyCode = no key assigned.
    property int keyCodeValue: 0
    property var mappingsModel: null
    signal enabledEdited(bool v)
    signal keyEdited(string v)
    signal keyCodeEdited(int v)

    readonly property int noKeyCode: 0

    property bool capturing: false

    readonly property bool invalidChar:
        keyValue.length > 0 && !isValidSingleChar(keyValue)

    // A leader with no key assigned cannot trigger anything. The character on
    // its own is not enough, so say so instead of looking configured.
    readonly property bool needsKey:
        enabledValue && keyCodeValue === noKeyCode

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
            font.pixelSize: Theme.fontBody
            Layout.preferredWidth: 40
        }

        // Click to arm, then press the key you want as the leader. Focus is
        // required for Keys.onPressed to see the press at all, so the whole
        // field is a focus scope that grabs the keyboard while capturing.
        Rectangle {
            id: captureField
            Layout.preferredWidth: 120
            Layout.preferredHeight: Theme.controlHeight
            radius: Theme.radiusSm
            color: Theme.background
            focus: true
            activeFocusOnTab: true
            border.color: root.invalidChar
                ? Theme.error
                : (root.capturing
                    ? Theme.accent
                    : (root.needsKey
                        ? Theme.warning
                        : (captureField.activeFocus ? Theme.accent : Theme.border)))
            border.width: 1
            Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

            Text {
                anchors.centerIn: parent
                text: root.capturing
                    ? qsTr("Press a key…")
                    : (root.keyValue.length > 0
                        ? root.keyValue
                        : qsTr("Click to set"))
                color: root.capturing || root.keyValue.length === 0
                    ? Theme.textMuted
                    : Theme.text
                font.family: root.capturing || root.keyValue.length === 0
                    ? Theme.fontFamily
                    : Theme.fontFamilyMono
                font.pixelSize: root.capturing || root.keyValue.length === 0
                    ? Theme.fontBody
                    : Theme.fontStrong
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    captureField.forceActiveFocus();
                    root.capturing = true;
                }
            }

            // Dropping focus mid-capture would leave the field armed forever.
            onActiveFocusChanged: {
                if (!activeFocus)
                    root.capturing = false;
            }

            Keys.onPressed: (event) => {
                if (!root.capturing)
                    return;
                event.accepted = true;

                // Wait for the real key; a standalone modifier press is the user
                // reaching for Shift, not the leader they mean.
                if (event.key === Qt.Key_Shift || event.key === Qt.Key_Control
                    || event.key === Qt.Key_Alt || event.key === Qt.Key_AltGr
                    || event.key === Qt.Key_Meta || event.key === Qt.Key_CapsLock)
                    return;

                if (event.key === Qt.Key_Escape) {
                    root.capturing = false;
                    return;
                }

                // The character is still shown to the user and checked against
                // the mappings, so a key that produces none (F1, arrows) cannot
                // serve as a leader. Stay armed and let them press another.
                const ch = event.text;
                if (!root.isValidSingleChar(ch))
                    return;

                root.capturing = false;
                // Keycode first: it is the authoritative half, and writing the
                // character first would briefly pair the new character with the
                // previous key's code.
                root.keyCodeEdited(event.nativeScanCode);
                root.keyEdited(ch);
            }
        }

        Text {
            visible: root.needsKey && !root.capturing
            Layout.fillWidth: true
            text: qsTr("No key assigned — click the field and press the key you want")
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }

        Text {
            visible: root.conflictsWithMapping && !root.needsKey
            Layout.fillWidth: true
            text: qsTr("Warning: this key is already a mapping input")
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }
    }
}
