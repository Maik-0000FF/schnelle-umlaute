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
    // Cycle direction, like the built-in leaders: false = forward, true = reverse.
    property bool reverseValue: false
    property string keyValue: ""
    // Whether a physical key has been captured. The model answers this, so the
    // "no key" sentinel keeps a single definition in C++ and is never restated
    // as a bare number here.
    property bool keyAssigned: false
    // The captured keycode, used to name no-character keys (Home, End, …). The
    // model owns the name map, so it stays the single source.
    property int keyValueCode: -1
    property var mappingsModel: null
    property var settingsModel: null
    signal enabledEdited(bool v)
    signal reverseEdited(bool v)
    // One key press, one signal: the character and the physical key belong
    // together and are stored in a single write. A no-character key (Home, End,
    // …) is captured with an empty character and named from its keycode.
    signal keyCaptured(string ch, int code)
    // Backspace/Delete clear the assignment instead of becoming a leader.
    signal keyCleared()

    // Human name of a captured no-character key, empty for a printable or unset
    // key. Sourced from the model's map so QML never restates it.
    readonly property string specialKeyName:
        settingsModel && keyValueCode >= 0
            ? settingsModel.specialLeaderName(keyValueCode) : ""
    // Whether a key is set to show (a printable character or a named key).
    readonly property bool hasShownKey:
        keyValue.length > 0 || specialKeyName.length > 0

    property bool capturing: false
    // Set while the user holds a modifier during capture, so the field can say
    // why it is not taking the press.
    property bool modifierHeld: false

    // A modifier never becomes part of a leader: matching compares the physical
    // key alone, so it is wrong to let one into the capture. Holding Shift and
    // pressing '/' would store the plain '/' KEY labelled '?', and the bare key
    // would trigger from then on. Holding AltGr and pressing 'q' to pick '@'
    // would arm the plain 'q' key. Requiring a clean press keeps the stored
    // character equal to what the bare key prints.
    //
    // This is capture only. While TYPING, modifiers are irrelevant by design:
    // Shift+A followed by the (shifted) leader key still fires it, which is how
    // uppercase mappings work.
    readonly property int captureBlockingModifiers:
        Qt.ShiftModifier | Qt.ControlModifier | Qt.AltModifier
        | Qt.MetaModifier | Qt.GroupSwitchModifier

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

    // Enable and direction share the same row and column layout as the built-in
    // directional leaders, so a custom leader lines up with and reads like them.
    // The key-capture field sits below (visible only while enabled).
    DirectionalLeaderRow {
        labelText: root.labelText
        enabledValue: root.enabledValue
        reverseValue: root.reverseValue
        onEnabledToggled: (v) => root.enabledEdited(v)
        onReverseToggled: (v) => root.reverseEdited(v)
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
                    ? (root.modifierHeld ? qsTr("Without modifiers") : qsTr("Press a key, or Backspace to clear"))
                    : (root.keyValue.length > 0
                        ? root.keyValue
                        : (root.specialKeyName.length > 0
                            ? root.specialKeyName
                            : qsTr("Click to set")))
                color: (!root.capturing && root.hasShownKey)
                    ? Theme.text
                    : Theme.textMuted
                // Mono only for a printable single character; a key name reads
                // as a word, so it uses the normal font.
                font.family: (!root.capturing && root.keyValue.length > 0)
                    ? Theme.fontFamilyMono
                    : Theme.fontFamily
                font.pixelSize: (!root.capturing && root.hasShownKey)
                    ? Theme.fontStrong
                    : Theme.fontBody
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    captureField.forceActiveFocus();
                    root.modifierHeld = false;
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

                // A key pressed WITH a modifier is not the key they will get.
                // Stay armed and say so.
                if (event.modifiers & root.captureBlockingModifiers) {
                    root.modifierHeld = true;
                    event.accepted = true;
                    return;
                }
                root.modifierHeld = false;

                // Backspace/Delete clear the assignment instead of becoming a
                // leader, so a captured key can be unset.
                if (event.key === Qt.Key_Backspace || event.key === Qt.Key_Delete) {
                    root.capturing = false;
                    event.accepted = true;
                    root.keyCleared();
                    return;
                }

                // A printable character is stored as the leader's character and
                // also matched by keycode.
                const ch = event.text;
                if (root.isValidSingleChar(ch)) {
                    root.capturing = false;
                    event.accepted = true;
                    root.keyCaptured(ch, event.nativeScanCode);
                    return;
                }

                // No character: accept only recognized navigation keys (Home,
                // End, …), captured by keycode with an empty character. Any other
                // no-character key (F-keys, arrows) stays armed.
                if (root.settingsModel
                    && root.settingsModel.specialLeaderName(event.nativeScanCode).length > 0) {
                    root.capturing = false;
                    event.accepted = true;
                    root.keyCaptured("", event.nativeScanCode);
                    return;
                }

                // Unrecognized key: stay armed and let them press another.
                event.accepted = true;
            }
        }

        Text {
            visible: root.capturing && root.modifierHeld
            Layout.fillWidth: true
            text: qsTr("A modifier is not part of the leader. Press the key on its own.")
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
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
