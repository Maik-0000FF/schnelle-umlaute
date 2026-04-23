import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

ColumnLayout {
    id: root
    Layout.fillWidth: true
    spacing: Theme.spacingSm

    property string value: "TopCol4"
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

                        Behavior on color { ColorAnimation { duration: Theme.animShort } }

                        Text {
                            anchors.centerIn: parent
                            visible: parent.parent.active
                            text: "✓"
                            color: Theme.onAccent
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
    }

    Text {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Click on the monitor to choose overlay position")
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: 11
        font.italic: true
    }
}
