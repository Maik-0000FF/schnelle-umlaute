import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

RowLayout {
    id: root
    Layout.fillWidth: true
    spacing: Theme.spacingMd

    property string labelText: ""
    property int minValue: 50
    property int maxValue: 2000
    property int stepSize: 25
    property int value: 400
    property string suffix: "ms"
    signal valueEdited(int v)

    Text {
        text: root.labelText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        Layout.preferredWidth: 120
    }

    Slider {
        id: slider
        Layout.fillWidth: true
        from: root.minValue
        to: root.maxValue
        stepSize: root.stepSize
        value: root.value
        snapMode: Slider.SnapAlways

        // Pin geometry to the handle so the control height is independent of
        // the active Quick Controls style / Qt version (whose default Slider
        // padding differs and otherwise shifts the whole row).
        padding: 0
        implicitHeight: 16

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 4
            radius: 2
            color: Theme.border

            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                color: Theme.accent
                radius: 2
            }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition *
               (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 16
            height: 16
            radius: 8
            color: slider.pressed ? Theme.accentHover : Theme.accent
            border.color: Theme.background
            border.width: 2

            // Keyboard-only focus ring (the slider takes focus on click too).
            FocusRing {
                visible: slider.activeFocus
                         && slider.focusReason !== Qt.MouseFocusReason
                radius: width / 2
            }
        }

        onValueChanged: {
            if (value !== root.value) root.valueEdited(value);
        }
    }

    Text {
        text: slider.value + " " + root.suffix
        color: Theme.textMuted
        font.family: Theme.fontFamilyMono
        font.pixelSize: Theme.fontBody
        Layout.preferredWidth: 70
        horizontalAlignment: Text.AlignRight
    }
}
