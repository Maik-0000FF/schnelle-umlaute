import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

ColumnLayout {
    id: root
    Layout.fillWidth: true
    spacing: Theme.spacingSm

    property string value: "TopCenter"
    signal edited(string newValue)

    readonly property var positions: [
        ["TopLeft",    "TopCenter",    "TopRight"],
        ["CenterLeft", "Center",       "CenterRight"],
        ["BottomLeft", "BottomCenter", "BottomRight"]
    ]

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        width: 280
        height: 170
        radius: Theme.radiusMd
        color: Theme.background
        border.color: Theme.border
        border.width: 1

        GridLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingMd
            columns: 3
            rows: 3
            columnSpacing: 0
            rowSpacing: 0

            Repeater {
                model: 9
                delegate: Item {
                    required property int index
                    readonly property int r: Math.floor(index / 3)
                    readonly property int c: index % 3
                    readonly property string pos: root.positions[r][c]
                    readonly property bool active: root.value === pos
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Rectangle {
                        width: 34
                        height: 34
                        radius: Theme.radiusSm
                        anchors.horizontalCenter: {
                            if (parent.c === 0) return undefined
                            if (parent.c === 2) return undefined
                            return parent.horizontalCenter
                        }
                        anchors.verticalCenter: {
                            if (parent.r === 0) return undefined
                            if (parent.r === 2) return undefined
                            return parent.verticalCenter
                        }
                        anchors.left: parent.c === 0 ? parent.left : undefined
                        anchors.right: parent.c === 2 ? parent.right : undefined
                        anchors.top: parent.r === 0 ? parent.top : undefined
                        anchors.bottom: parent.r === 2 ? parent.bottom : undefined

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
                            color: "#ffffff"
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
