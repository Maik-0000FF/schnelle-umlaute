import QtQuick
import QtQuick.Controls
import SchnelleUmlaute

// The app's toggle switch: a theme-coloured track and thumb, geometry pinned
// to the indicator so the row height never depends on the active Quick
// Controls style or Qt version. Single source for the switch look, shared by
// LabeledSwitch and DirectionalLeaderRow. Emits toggled(checked) like a plain
// Switch. The pixel geometry lives here once instead of being restated per
// call site.
Switch {
    id: sw

    // Track and thumb geometry, defined once.
    readonly property int trackWidth: 40
    readonly property int trackHeight: 22
    readonly property int thumbSize: 18
    readonly property int thumbInset: 2

    padding: 0
    spacing: 0
    implicitWidth: sw.indicator.implicitWidth
    implicitHeight: sw.indicator.implicitHeight

    indicator: Rectangle {
        implicitWidth: sw.trackWidth
        implicitHeight: sw.trackHeight
        radius: sw.trackHeight / 2
        color: sw.checked ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.animShort } }

        Rectangle {
            x: sw.checked ? parent.width - width - sw.thumbInset : sw.thumbInset
            y: sw.thumbInset
            width: sw.thumbSize
            height: sw.thumbSize
            radius: sw.thumbSize / 2
            color: Theme.switchThumb
            Behavior on x { NumberAnimation { duration: Theme.animShort } }
        }
    }
    background: Rectangle { color: "transparent" }
}
