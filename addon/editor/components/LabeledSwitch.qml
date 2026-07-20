import QtQuick
import QtQuick.Layouts
import SchnelleUmlaute

RowLayout {
    id: root
    Layout.fillWidth: true
    spacing: Theme.spacingMd

    property string labelText: ""
    property bool checked: false
    signal toggled(bool v)

    opacity: root.enabled ? 1.0 : 0.4
    Behavior on opacity { NumberAnimation { duration: Theme.animShort } }

    // Fixed-width label column so the toggle lines up with every other toggle
    // row across the settings cards. A label longer than the column (e.g. a
    // longer translation) wraps onto a second line instead of being truncated,
    // so no text is lost and the column keeps its shared width.
    Text {
        text: root.labelText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        Layout.preferredWidth: Theme.settingLabelWidth
        wrapMode: Text.WordWrap
    }

    ThemedSwitch {
        checked: root.checked
        onToggled: root.toggled(checked)
    }

    // Trailing free space keeps the toggle left-aligned in the shared column.
    Item { Layout.fillWidth: true }
}
