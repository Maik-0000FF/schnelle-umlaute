import QtQuick
import QtQuick.Controls
import SchnelleUmlaute

// Drop-in ToolTip that styles its surface from the app's Theme instead of
// the Quick Controls system palette. The attached ToolTip the controls used
// before rendered with the platform style, which ignored Theme.qml and looked
// out of place on the dark editor. Use as a child of the hovered control:
//   ThemedToolTip { visible: control.hovered; text: qsTr("…") }
ToolTip {
    id: tip
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBody
    padding: Theme.spacingSm

    contentItem: Text {
        text: tip.text
        color: Theme.text
        font: tip.font
        wrapMode: Text.WordWrap
    }

    background: Rectangle {
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        radius: Theme.radiusSm
    }
}
