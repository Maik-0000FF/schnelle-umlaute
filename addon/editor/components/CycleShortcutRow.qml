import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

// One "Cycle next/previous" row on the Mappings page: a fixed-width label, a
// shortcut-capture field, and a wrapping description. Both rows share this exact
// layout; the parent wires captured() to the matching profilesModel property.
RowLayout {
    id: row
    Layout.fillWidth: true
    spacing: Theme.spacingMd

    property string labelText: ""
    property int labelWidth: 0
    property string shortcut: ""
    property string description: ""
    signal captured(string combo)

    Text {
        text: row.labelText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        Layout.preferredWidth: row.labelWidth
    }
    KeyCaptureField {
        Layout.preferredWidth: Theme.shortcutFieldWidth
        value: row.shortcut
        onCaptured: (combo) => row.captured(combo)
    }
    Text {
        Layout.fillWidth: true
        text: row.description
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontCaption
        wrapMode: Text.WordWrap
    }
}
