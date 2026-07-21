import QtQuick
import QtQuick.Layouts
import SchnelleUmlaute

// A leader row that also carries a cycle direction. Layout, left to right:
// the label (fills the row), then the direction toggle with its arrow marker
// right beside it (→ forward, ← reverse), a gap for visual separation, and
// finally the enable toggle flush at the right edge, aligned with every other
// toggle row. Built on SettingRow, so the whole strip gets a hover highlight
// linking the far-apart label and toggles.
//
// Enable gates whether the key is a leader at all; direction only matters
// while enabled, so the direction toggle and arrow dim and the toggle disables
// when enable is off.
SettingRow {
    id: root

    property string labelText: ""
    property bool enabledValue: false   // is this key a leader
    property bool reverseValue: false   // false = forward (+1), true = reverse (-1)
    // Hover descriptions (see issue #120): one for the enable toggle, one for
    // the direction toggle. The direction meaning is the same for every leader,
    // so it carries a shared default.
    property string tooltipText: ""
    property string directionTooltipText:
        qsTr("Cycle backward through the variants instead of forward.")
    signal enabledToggled(bool v)
    signal reverseToggled(bool v)

    // The label fills the row so the enable toggle lands flush at the right
    // edge, aligned with every other toggle row. A label longer than the row
    // wraps onto a second line instead of truncating, matching LabeledSwitch.
    Text {
        text: root.labelText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }

    // Direction toggle, left of the enable toggle. Direction only matters while
    // enabled, so it dims and disables when enable is off.
    ThemedSwitch {
        id: directionSw
        checked: root.reverseValue
        enabled: root.enabledValue
        opacity: root.enabledValue ? 1.0 : 0.4
        Behavior on opacity { NumberAnimation { duration: Theme.animShort } }
        onToggled: root.reverseToggled(checked)
        ThemedToolTip {
            hovered: root.directionTooltipText.length > 0 && directionSw.hovered
            text: root.directionTooltipText
        }
    }

    // Direction marker right beside its toggle: → forward, ← reverse. The arrow
    // alone carries the direction now (the Forward/Reverse word was dropped).
    Text {
        text: root.reverseValue ? "←" : "→"
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody + 2
        font.bold: true
        opacity: root.enabledValue ? 1.0 : 0.4
        Behavior on opacity { NumberAnimation { duration: Theme.animShort } }
    }

    // Enable toggle, flush at the right edge in the shared toggle column. The
    // extra left margin opens a gap after the direction marker so the two
    // toggles read as separate controls, not one.
    ThemedSwitch {
        id: enableSw
        Layout.leftMargin: Theme.spacingLg
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
        ThemedToolTip {
            hovered: root.tooltipText.length > 0 && enableSw.hovered
            text: root.tooltipText
        }
    }
}
