import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

// A two-handle range slider that defines an accent window [lower, upper] in
// milliseconds. The lower handle is the minimum hold time (drag it up for a
// PowerToys-style "hold first" feel); the upper handle is the latest moment a
// leader still triggers the accent. The window's start time is shown on the
// left, its end time on the right, and the computed window duration above the
// track. Dragging the filled line between the handles moves the whole window.
//
// Both handles stay reachable even when they sit on top of each other: the
// track captures the press, picks the lower handle on the left side of the
// cluster and the upper on the right, and treats a press on the line between
// them as a window move.
RowLayout {
    id: root
    Layout.fillWidth: true
    spacing: Theme.spacingMd

    property string labelText: ""
    property int from: 0
    property int to: 2000
    property int step: 25
    property int lowerValue: 0
    property int upperValue: 400
    property string suffix: "ms"

    signal lowerEdited(int v)
    signal upperEdited(int v)

    Text {
        text: root.labelText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: 13
        Layout.preferredWidth: 120
    }

    // Window start (lower bound)
    Text {
        text: root.lowerValue + " " + root.suffix
        color: Theme.textMuted
        font.family: Theme.fontFamilyMono
        font.pixelSize: 12
        Layout.preferredWidth: 56
        horizontalAlignment: Text.AlignRight
    }

    Item {
        id: track
        Layout.fillWidth: true
        implicitHeight: 36

        readonly property int handleW: 16
        readonly property int hit: 10
        // Top band holds the duration label, the lower band the track + handles.
        readonly property int rowY: 16
        readonly property int rowH: implicitHeight - rowY
        readonly property real lineY: rowY + rowH / 2
        readonly property real spanW: Math.max(1, width - handleW)

        // 0 = none, 1 = lower handle, 2 = upper handle, 3 = move window.
        property int dragMode: 0

        function clamp(v, lo, hi) {
            return Math.max(lo, Math.min(hi, v));
        }
        function xForValue(v) {
            return handleW / 2 + (v - root.from) / (root.to - root.from) * spanW;
        }
        function snap(v) {
            var s = Math.round(v / root.step) * root.step;
            return clamp(s, root.from, root.to);
        }
        function valueForX(px) {
            var t = clamp((px - handleW / 2) / spanW, 0, 1);
            return snap(root.from + t * (root.to - root.from));
        }

        // Computed window duration, centered above the track.
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 0
            text: (root.upperValue - root.lowerValue) + " " + root.suffix
            color: Theme.accent
            font.family: Theme.fontFamilyMono
            font.pixelSize: 12
        }

        // Track line
        Rectangle {
            x: 0
            width: track.width
            height: 4
            radius: 2
            y: track.lineY - height / 2
            color: Theme.border
        }
        // Filled window between the two handles. Also the "move the window"
        // drag surface.
        Rectangle {
            x: track.xForValue(root.lowerValue)
            width: Math.max(0, track.xForValue(root.upperValue) - x)
            height: 4
            radius: 2
            y: track.lineY - height / 2
            color: track.dragMode === 3 ? Theme.accentHover : Theme.accent
        }

        // Handle visuals (input is handled by trackArea below so overlapping
        // handles never fight over the press).
        Rectangle {
            width: track.handleW
            height: track.handleW
            radius: width / 2
            x: track.xForValue(root.lowerValue) - width / 2
            y: track.lineY - height / 2
            z: track.dragMode === 1 ? 2 : 1
            color: track.dragMode === 1 ? Theme.accentHover : Theme.accent
            border.color: Theme.background
            border.width: 2
        }
        Rectangle {
            width: track.handleW
            height: track.handleW
            radius: width / 2
            x: track.xForValue(root.upperValue) - width / 2
            y: track.lineY - height / 2
            z: track.dragMode === 2 ? 2 : 1
            color: track.dragMode === 2 ? Theme.accentHover : Theme.accent
            border.color: Theme.background
            border.width: 2
        }

        MouseArea {
            id: trackArea
            x: 0
            y: track.rowY
            width: track.width
            height: track.rowH
            preventStealing: true

            property real moveStartX: 0
            property int startLower: 0
            property int startUpper: 0

            onPressed: (mouse) => {
                var lx = track.xForValue(root.lowerValue);
                var ux = track.xForValue(root.upperValue);
                var dl = Math.abs(mouse.x - lx);
                var du = Math.abs(mouse.x - ux);
                if (dl <= track.hit && du <= track.hit) {
                    // Overlapping handles: the side decides.
                    track.dragMode = mouse.x <= lx ? 1 : 2;
                } else if (dl <= track.hit) {
                    track.dragMode = 1;
                } else if (du <= track.hit) {
                    track.dragMode = 2;
                } else if (mouse.x > lx && mouse.x < ux) {
                    // Press on the line between the handles → move the window.
                    track.dragMode = 3;
                    moveStartX = mouse.x;
                    startLower = root.lowerValue;
                    startUpper = root.upperValue;
                } else {
                    // Outside the window → grab the nearer end.
                    track.dragMode = mouse.x < lx ? 1 : 2;
                }

                if (track.dragMode === 1)
                    root.lowerEdited(Math.min(track.valueForX(mouse.x), root.upperValue));
                else if (track.dragMode === 2)
                    root.upperEdited(Math.max(track.valueForX(mouse.x), root.lowerValue));
            }

            onPositionChanged: (mouse) => {
                if (track.dragMode === 1) {
                    root.lowerEdited(track.clamp(track.valueForX(mouse.x), root.from, root.upperValue));
                } else if (track.dragMode === 2) {
                    root.upperEdited(track.clamp(track.valueForX(mouse.x), root.lowerValue, root.to));
                } else if (track.dragMode === 3) {
                    var valDelta = (mouse.x - moveStartX) / track.spanW * (root.to - root.from);
                    var gap = startUpper - startLower;
                    var nl = track.clamp(track.snap(startLower + valDelta), root.from, root.to - gap);
                    root.lowerEdited(nl);
                    root.upperEdited(nl + gap);
                }
            }

            onReleased: track.dragMode = 0
        }
    }

    // Window end (upper bound)
    Text {
        text: root.upperValue + " " + root.suffix
        color: Theme.textMuted
        font.family: Theme.fontFamilyMono
        font.pixelSize: 12
        Layout.preferredWidth: 60
        horizontalAlignment: Text.AlignRight
    }
}
