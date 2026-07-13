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
    // Whether a physical key has been captured. The model answers this, so the
    // "no key" sentinel keeps a single definition in C++ and is never restated
    // as a bare number here.
    property bool keyAssigned: false
    property var mappingsModel: null
    signal enabledEdited(bool v)
    // One key press, one signal: the character and the physical key belong
    // together and are stored in a single write.
    signal keyCaptured(string ch, int code)

    property bool capturing: false

    readonly property bool invalidChar:
        keyValue.length > 0 && !isValidSingleChar(keyValue)

    // A leader with no key assigned cannot trigger anything. The character on
    // its own is not enough, so say so instead of looking configured.
    readonly property bool needsKey: enabledValue && !keyAssigned

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

                // Tab keeps moving focus, even while armed: swallowing it would
                // trap the keyboard in a field the user cannot leave. Losing
                // focus cancels the capture (onActiveFocusChanged).
                if (event.key === Qt.Key_Tab || event.key === Qt.Key_Backtab)
                    return;

                // Escape cancels. Consume it so it does not also close the
                // window.
                if (event.key === Qt.Key_Escape) {
                    root.capturing = false;
                    event.accepted = true;
                    return;
                }

                // Keep waiting on a bare modifier press: that is the user
                // reaching for Shift, not the leader they mean.
                if (event.key === Qt.Key_Shift || event.key === Qt.Key_Control
                    || event.key === Qt.Key_Alt || event.key === Qt.Key_AltGr
                    || event.key === Qt.Key_Meta || event.key === Qt.Key_CapsLock) {
                    event.accepted = true;
                    return;
                }

                // The character is shown to the user and checked against the
                // mappings, so a key that produces none (F1, arrows) cannot
                // serve as a leader. Stay armed and let them press another.
                const ch = event.text;
                if (!root.isValidSingleChar(ch)) {
                    event.accepted = true;
                    return;
                }

                root.capturing = false;
                event.accepted = true;
                root.keyCaptured(ch, event.nativeScanCode);
            }
        }

        // The red border needs a reason next to it, or the field just looks
        // broken. Only a hand-edited config can get here: a capture never
        // stores a character that fails isValidSingleChar.
        Text {
            visible: root.invalidChar && !root.capturing
            Layout.fillWidth: true
            text: qsTr("Stored character is not a single character")
            color: Theme.error
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }

        Text {
            visible: root.needsKey && !root.capturing && !root.invalidChar
            Layout.fillWidth: true
            text: qsTr("No key assigned. Click the field and press the key you want.")
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }

        Text {
            visible: root.conflictsWithMapping && !root.needsKey && !root.invalidChar
            Layout.fillWidth: true
            text: qsTr("Warning: this key is already a mapping input")
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }
    }
}
