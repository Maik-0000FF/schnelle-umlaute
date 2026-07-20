import QtQuick
import QtQuick.Layouts
import SchnelleUmlaute

// A leader row that also carries a cycle direction. Layout, left to right:
// label, an arrow marker, the direction toggle, then the enable toggle (the
// enable toggle keeps the same rightmost spot as the plain LabeledSwitch it
// replaces). The arrow flips (→ forward, ← reverse) with the
// direction toggle so the chosen direction reads at a glance.
//
// Enable gates whether the key is a leader at all; direction only matters
// while enabled, so the arrow and direction toggle dim and disable when enable
// is off.
RowLayout {
    id: root
    Layout.fillWidth: true
    spacing: Theme.spacingMd

    property string labelText: ""
    property bool enabledValue: false   // is this key a leader
    property bool reverseValue: false   // false = forward (+1), true = reverse (-1)
    signal enabledToggled(bool v)
    signal reverseToggled(bool v)

    Text {
        text: root.labelText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        Layout.fillWidth: true
    }

    // Direction marker.
    Text {
        text: root.reverseValue ? "←" : "→"
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        opacity: root.enabledValue ? 1.0 : 0.4
        Behavior on opacity { NumberAnimation { duration: Theme.animShort } }
    }

    // Direction toggle, left of the enable toggle.
    ThemedSwitch {
        checked: root.reverseValue
        enabled: root.enabledValue
        opacity: root.enabledValue ? 1.0 : 0.4
        Behavior on opacity { NumberAnimation { duration: Theme.animShort } }
        onToggled: root.reverseToggled(checked)
    }

    // Enable toggle.
    ThemedSwitch {
        checked: root.enabledValue
        onToggled: root.enabledToggled(checked)
    }
}
