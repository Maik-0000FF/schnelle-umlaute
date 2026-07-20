import QtQuick
import QtQuick.Layouts
import SchnelleUmlaute

// A leader row that also carries a cycle direction. Layout, left to right:
// fixed-width label, the enable toggle (aligned with every other toggle row),
// then the direction group: direction toggle, the current direction word
// (Forward / Reverse), the arrow marker (→ / ←), and trailing free space.
//
// Enable gates whether the key is a leader at all; direction only matters
// while enabled, so the direction toggle, word and arrow dim and the toggle
// disables when enable is off.
RowLayout {
    id: root
    Layout.fillWidth: true
    spacing: Theme.spacingMd

    property string labelText: ""
    property bool enabledValue: false   // is this key a leader
    property bool reverseValue: false   // false = forward (+1), true = reverse (-1)
    signal enabledToggled(bool v)
    signal reverseToggled(bool v)

    // Fixed-width label column, shared with LabeledSwitch so the enable toggle
    // below lines up with every other toggle. A label longer than the column
    // wraps onto a second line instead of truncating, matching LabeledSwitch.
    Text {
        text: root.labelText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        Layout.preferredWidth: Theme.settingLabelWidth
        wrapMode: Text.WordWrap
    }

    // Enable toggle, in the shared toggle column.
    ThemedSwitch {
        checked: root.enabledValue
        // A refused change (the leader guard in SettingsModel) must snap the
        // switch back. An interactive toggle breaks the `checked` binding, so
        // re-establish it and let the model stay the single source of truth: if
        // the setter applies, the binding follows; if it refuses, it reverts.
        onToggled: {
            const requested = checked;
            checked = Qt.binding(() => root.enabledValue);
            root.enabledToggled(requested);
        }
    }

    // Direction toggle. Extra left margin sets it apart from the enable
    // toggle, so the two are not read as one control.
    ThemedSwitch {
        Layout.leftMargin: Theme.spacingLg
        checked: root.reverseValue
        enabled: root.enabledValue
        opacity: root.enabledValue ? 1.0 : 0.4
        Behavior on opacity { NumberAnimation { duration: Theme.animShort } }
        onToggled: root.reverseToggled(checked)
    }

    // Direction word, naming the current cycle direction.
    Text {
        text: root.reverseValue ? qsTr("Reverse") : qsTr("Forward")
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        opacity: root.enabledValue ? 1.0 : 0.4
        Behavior on opacity { NumberAnimation { duration: Theme.animShort } }
    }

    // Direction marker: bold and a touch larger so the arrow reads clearly.
    Text {
        text: root.reverseValue ? "←" : "→"
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody + 2
        font.bold: true
        opacity: root.enabledValue ? 1.0 : 0.4
        Behavior on opacity { NumberAnimation { duration: Theme.animShort } }
    }

    // Trailing free space (the "freiraum" after the arrow).
    Item { Layout.fillWidth: true }
}
