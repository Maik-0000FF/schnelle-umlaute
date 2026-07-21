import QtQuick
import QtQuick.Layouts
import SchnelleUmlaute

// A plain label + toggle settings row. Built on SettingRow, so the whole strip
// gets a hover highlight linking the left label to the right-aligned switch.
SettingRow {
    id: root

    property string labelText: ""
    property bool checked: false
    // Optional hover description for the toggle (see issue #120). Empty = none.
    property string tooltipText: ""
    signal toggled(bool v)

    opacity: root.enabled ? 1.0 : 0.4
    Behavior on opacity { NumberAnimation { duration: Theme.animShort } }

    // The label fills the row so the toggle lands flush at the right edge, the
    // platform-standard settings layout; every toggle row lines up on that edge.
    // A long label wraps onto a second line instead of being truncated.
    Text {
        text: root.labelText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }

    ThemedSwitch {
        id: sw
        checked: root.checked
        // A refused change (e.g. the leader guard in SettingsModel) must snap the
        // switch back. An interactive toggle breaks the `checked` binding, so
        // re-establish it and let the model stay the single source of truth: if
        // the setter applies, the binding follows; if it refuses, it reverts.
        onToggled: {
            const requested = checked;
            checked = Qt.binding(() => root.checked);
            root.toggled(requested);
        }
        ThemedToolTip {
            hovered: root.tooltipText.length > 0 && sw.hovered
            text: root.tooltipText
        }
    }
}
