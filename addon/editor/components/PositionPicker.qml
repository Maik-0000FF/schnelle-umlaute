import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

ColumnLayout {
    id: root
    Layout.fillWidth: true
    spacing: Theme.spacingSm

    property string value: "TopCol4"
    // When true the overlay follows the mouse pointer; the grid below is only
    // the fallback, so the chosen cell stays marked but dimmed and a pointer
    // marker is drawn on the monitor preview.
    property bool atCursorMode: false
    signal edited(string newValue)

    readonly property int cols: 7
    readonly property int rows: 3
    readonly property var rowPrefixes: ["Top", "Center", "Bottom"]

    function positionFor(r, c) {
        return root.rowPrefixes[r] + "Col" + (c + 1)
    }

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        width: 420
        height: 180
        radius: Theme.radiusMd
        color: Theme.background
        border.color: Theme.border
        border.width: 1

        GridLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingMd
            columns: root.cols
            rows: root.rows
            columnSpacing: 0
            rowSpacing: 0

            Repeater {
                model: root.cols * root.rows
                delegate: Item {
                    required property int index
                    readonly property int r: Math.floor(index / root.cols)
                    readonly property int c: index % root.cols
                    readonly property string pos: root.positionFor(r, c)
                    readonly property bool active: root.value === pos
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // Every cell centers its square — this produces even
                    // gaps across the 7×3 grid. Earlier versions docked
                    // the outer cells to the frame edge, which worked at
                    // 3×3 but made 7-column gaps visibly uneven.
                    Rectangle {
                        width: 34
                        height: 34
                        radius: Theme.radiusSm
                        anchors.centerIn: parent

                        color: parent.active
                            ? Theme.accent
                            : (mouse.containsMouse ? Theme.surfaceHover : Theme.surface)
                        border.color: parent.active ? Theme.accent : Theme.border
                        border.width: 1
                        // In cursor mode the active cell is only the fallback —
                        // keep it marked but dimmed so the pointer marker reads
                        // as the primary placement.
                        opacity: (parent.active && root.atCursorMode) ? 0.4 : 1.0

                        Behavior on color { ColorAnimation { duration: Theme.animShort } }

                        Text {
                            anchors.centerIn: parent
                            visible: parent.parent.active
                            text: "✓"
                            color: Theme.switchThumb
                            font.pixelSize: 14
                            font.weight: Font.Bold
                        }

                        ToolTip.visible: mouse.containsMouse
                        ToolTip.text: parent.pos

                        MouseArea {
                            id: mouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.value !== parent.parent.pos) {
                                    root.edited(parent.parent.pos);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Mouse-pointer marker: shown only in cursor mode to signal the menu
        // appears wherever the pointer is (not at a fixed grid cell). Drawn as
        // a classic arrow so it reads as a cursor regardless of theme.
        Canvas {
            id: cursorMarker
            visible: root.atCursorMode
            anchors.centerIn: parent
            width: 40
            height: 40
            onVisibleChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                if (!visible)
                    return;
                var s = 2.0;
                ctx.beginPath();
                ctx.moveTo(2 * s, 1 * s);
                ctx.lineTo(2 * s, 13 * s);
                ctx.lineTo(5.5 * s, 9.7 * s);
                ctx.lineTo(8 * s, 15 * s);
                ctx.lineTo(10 * s, 14 * s);
                ctx.lineTo(7.6 * s, 9 * s);
                ctx.lineTo(12 * s, 9 * s);
                ctx.closePath();
                ctx.fillStyle = Theme.text;
                ctx.fill();
                ctx.lineWidth = 1.2;
                ctx.strokeStyle = Theme.background;
                ctx.stroke();
            }
        }
    }

    Text {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: root.atCursorMode
            ? qsTr("The overlay follows the mouse pointer (fallback position dimmed)")
            : qsTr("Click on the monitor to choose overlay position")
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: 11
        font.italic: true
    }
}
