import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

// A two-handle range slider that defines an accent window [lower, upper] in
// milliseconds. The lower handle is the minimum hold time (drag it up for a
// "hold first" feel); the upper handle is the latest moment a
// leader still triggers the accent. The lead (dead-time) duration is centered
// above its own segment and the window duration above the window segment, each
// in its region's colour; the window's end value sits neutral at the right.
// Dragging the filled line between the handles moves the whole window.
//
// Both handles stay reachable even when they sit on top of each other: a
// press on a stacked (or near-stacked) pair is resolved by the first drag
// direction (drag left pulls the lower handle out, drag right the upper), so
// neither handle gets buried under the other. A press on the line between
// separated handles moves the whole window.
RowLayout {
    id: root
    Layout.fillWidth: true
    spacing: Theme.spacingMd

    property string labelText: ""
    property int from: 0
    property int to: 2000
    property int step: 10
    // Floor for the upper handle (max). The lower handle may go down to
    // "from" (no minimum hold), but the engine constrains the window's upper
    // bound to kDelayMin, so the max handle must not drop below upperMin.
    property int upperMin: from
    property int lowerValue: 0
    property int upperValue: 400
    property string suffix: "ms"

    signal lowerEdited(int v)
    signal upperEdited(int v)

    Text {
        text: root.labelText
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontBody
        Layout.preferredWidth: 120
    }

    Item {
        id: track
        Layout.fillWidth: true
        implicitHeight: 36

        readonly property int handleW: 18
        // Generous grab radius so either handle is easy to hit, including the
        // lower one when it sits at the far-left edge (value at "from").
        readonly property int hit: 22
        // Top band holds the lead + window duration labels, the lower band the
        // track + handles.
        readonly property int rowY: 16
        readonly property int rowH: implicitHeight - rowY
        readonly property real lineY: rowY + rowH / 2
        readonly property real spanW: Math.max(1, width - handleW)

        // 0 = none, 1 = lower handle, 2 = upper handle, 3 = move window,
        // 4 = stacked press pending its first-drag direction (→ 1 or 2).
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

        // Lead (dead-time) duration, centered above its own segment
        // [from … lower] in the lead role colour, mirroring the window label
        // below. Shown for any non-zero lead and clamped to the track, so the
        // value appears from the first step instead of staying hidden until the
        // segment is wide enough to host it. At zero lead there is no segment,
        // so it stays hidden (the value would be 0 and overlap the window label).
        Text {
            id: leadLabel
            readonly property real mid:
                (track.xForValue(root.from)
                 + track.xForValue(root.lowerValue)) / 2
            // width is 0 until the first text layout; require it so the label
            // doesn't flash for one frame before its real width is known.
            visible: width > 0 && root.lowerValue > root.from
            x: Math.max(0, Math.min(track.width - width, mid - width / 2))
            y: 0
            text: (root.lowerValue - root.from) + " " + root.suffix
            color: Theme.sliderLead
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontBody
        }

        // Computed window duration, centered above the window itself (the
        // midpoint between the two handles) so it tracks the window as it
        // moves. Clamped to the track so a window pushed to either edge keeps
        // the label fully visible instead of clipping off the side. When the
        // lead label is shown at a small lead, both would otherwise crowd the
        // left end, so this one is nudged right to clear it (rather than hiding
        // either), so both stay readable.
        Text {
            id: durationLabel
            readonly property real mid:
                (track.xForValue(root.lowerValue)
                 + track.xForValue(root.upperValue)) / 2
            x: {
                var ideal = Math.max(0, Math.min(track.width - width,
                                                 mid - width / 2));
                if (leadLabel.visible) {
                    var minX = leadLabel.x + leadLabel.width + Theme.spacingSm;
                    return Math.min(track.width - width, Math.max(ideal, minX));
                }
                return ideal;
            }
            y: 0
            text: (root.upperValue - root.lowerValue) + " " + root.suffix
            color: Theme.sliderWindow
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontBody
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
        // Dead-time (lead) region [from … lower]: the minimum hold before the
        // accent window opens, in the lead role colour (the overlay progress
        // bar's "time elapsing" segment); the window fill below is the window
        // role colour.
        Rectangle {
            x: 0
            width: Math.max(0, track.xForValue(root.lowerValue))
            height: 4
            radius: 2
            y: track.lineY - height / 2
            color: Theme.sliderLead
        }
        // Filled window between the two handles. Also the "move the window"
        // drag surface.
        Rectangle {
            x: track.xForValue(root.lowerValue)
            width: Math.max(0, track.xForValue(root.upperValue) - x)
            height: 4
            radius: 2
            y: track.lineY - height / 2
            color: track.dragMode === 3 ? Theme.sliderWindowHover
                                        : Theme.sliderWindow
        }

        // Handle visuals (input is handled by trackArea below so overlapping
        // handles never fight over the press).
        Rectangle {
            id: lowerHandle
            width: track.handleW
            height: track.handleW
            radius: width / 2
            x: track.xForValue(root.lowerValue) - width / 2
            y: track.lineY - height / 2
            z: (track.dragMode === 1 || lowerHandle.activeFocus) ? 2 : 1
            // Lower handle is the dead-time (lead) bound, so it carries the
            // lead role colour; the upper (window) handle carries the window
            // role colour. The role tokens centralise the per-theme swap.
            color: track.dragMode === 1 ? Theme.sliderLeadHover
                                        : Theme.sliderLead
            // Keyboard focus shows as an accent handle border (the handles only
            // take focus via Tab; dragging is handled by trackArea below).
            border.color: lowerHandle.activeFocus ? Theme.accent
                                                  : Theme.background
            border.width: 2
            Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

            // Reachable by Tab; Left/Right nudge the lead bound by one step.
            activeFocusOnTab: true
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Left) {
                    root.lowerEdited(track.clamp(track.snap(root.lowerValue - root.step),
                                                 root.from, root.upperValue));
                    event.accepted = true;
                } else if (event.key === Qt.Key_Right) {
                    root.lowerEdited(track.clamp(track.snap(root.lowerValue + root.step),
                                                 root.from, root.upperValue));
                    event.accepted = true;
                }
            }
        }
        Rectangle {
            id: upperHandle
            width: track.handleW
            height: track.handleW
            radius: width / 2
            x: track.xForValue(root.upperValue) - width / 2
            y: track.lineY - height / 2
            z: (track.dragMode === 2 || upperHandle.activeFocus) ? 2 : 1
            color: track.dragMode === 2 ? Theme.sliderWindowHover
                                        : Theme.sliderWindow
            // Keyboard focus shows as an accent handle border.
            border.color: upperHandle.activeFocus ? Theme.accent
                                                   : Theme.background
            border.width: 2
            Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

            // Reachable by Tab; Left/Right nudge the window's upper bound.
            activeFocusOnTab: true
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Left) {
                    root.upperEdited(track.clamp(
                        track.snap(root.upperValue - root.step),
                        Math.max(root.lowerValue, root.upperMin), root.to));
                    event.accepted = true;
                } else if (event.key === Qt.Key_Right) {
                    root.upperEdited(track.clamp(
                        track.snap(root.upperValue + root.step),
                        Math.max(root.lowerValue, root.upperMin), root.to));
                    event.accepted = true;
                }
            }
        }

        MouseArea {
            id: trackArea
            x: 0
            y: track.rowY
            width: track.width
            height: track.rowH
            preventStealing: true

            property real moveStartX: 0
            property real pressX: 0
            property int startLower: 0
            property int startUpper: 0

            onPressed: (mouse) => {
                var lx = track.xForValue(root.lowerValue);
                var ux = track.xForValue(root.upperValue);
                var dl = Math.abs(mouse.x - lx);
                var du = Math.abs(mouse.x - ux);
                pressX = mouse.x;
                // Handles on (or almost on) each other can't be told apart by
                // position, so defer the choice: grab the cluster and let the
                // first drag direction decide which handle moves. Nothing is
                // edited until then, so a stacked pair never jumps on press.
                var stacked = Math.abs(lx - ux) <= track.handleW;
                if (stacked && dl <= track.hit) {
                    track.dragMode = 4;
                } else if (dl <= track.hit && du <= track.hit) {
                    // Both reachable but with a gap: the side of the press picks.
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
                    root.upperEdited(Math.max(track.valueForX(mouse.x), root.lowerValue, root.upperMin));
            }

            onPositionChanged: (mouse) => {
                if (track.dragMode === 4) {
                    // First clear movement resolves the stacked press into a
                    // handle by direction (left → lower, right → upper).
                    if (mouse.x < pressX) track.dragMode = 1;
                    else if (mouse.x > pressX) track.dragMode = 2;
                    else return;
                }
                if (track.dragMode === 1) {
                    root.lowerEdited(track.clamp(track.valueForX(mouse.x), root.from, root.upperValue));
                } else if (track.dragMode === 2) {
                    root.upperEdited(track.clamp(track.valueForX(mouse.x), Math.max(root.lowerValue, root.upperMin), root.to));
                } else if (track.dragMode === 3) {
                    var valDelta = (mouse.x - moveStartX) / track.spanW * (root.to - root.from);
                    var gap = startUpper - startLower;
                    // Keep the lower handle within range and the upper handle
                    // at or above its floor (upperMin) while shifting both.
                    var nl = track.clamp(track.snap(startLower + valDelta),
                                         Math.max(root.from, root.upperMin - gap),
                                         root.to - gap);
                    root.lowerEdited(nl);
                    root.upperEdited(nl + gap);
                }
            }

            onReleased: track.dragMode = 0
        }
    }

    // Window end (upper bound): the absolute end of the accent window. Kept
    // neutral (muted) rather than in the window colour so it reads as a plain
    // end-of-range readout, while the coloured in-track labels carry the
    // lead/window roles.
    Text {
        text: root.upperValue + " " + root.suffix
        color: Theme.textMuted
        font.family: Theme.fontFamilyMono
        font.pixelSize: Theme.fontBody
        Layout.preferredWidth: 60
        horizontalAlignment: Text.AlignRight
    }
}
