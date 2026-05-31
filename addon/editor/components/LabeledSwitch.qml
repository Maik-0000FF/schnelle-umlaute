import QtQuick
import QtQuick.Controls
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

    Text {
        text: root.labelText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: 13
        Layout.fillWidth: true
    }

    Switch {
        id: sw
        checked: root.checked

        // Pin geometry to the indicator so the row height does not depend on
        // the active Quick Controls style. Different styles (and Qt versions)
        // give Switch different default padding/implicitSize, which otherwise
        // grows the row, the card, and shifts every label inside it.
        padding: 0
        spacing: 0
        implicitWidth: 40
        implicitHeight: 22

        indicator: Rectangle {
            implicitWidth: 40
            implicitHeight: 22
            radius: 11
            color: sw.checked ? Theme.accent : Theme.border
            Behavior on color { ColorAnimation { duration: Theme.animShort } }

            Rectangle {
                x: sw.checked ? parent.width - width - 2 : 2
                y: 2
                width: 18
                height: 18
                radius: 9
                color: Theme.switchThumb
                Behavior on x { NumberAnimation { duration: Theme.animShort } }
            }
        }
        background: Rectangle { color: "transparent" }
        onToggled: root.toggled(checked)
    }
}
