import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import SchnelleUmlauteOverlay

Window {
    id: win
    flags: Qt.FramelessWindowHint
    color: "transparent"
    width: frame.implicitWidth
    height: frame.implicitHeight
    // Start hidden so main() can configure the layer-shell surface
    // (layer/anchors/screen) before the first commit. main() then calls
    // show() once the surface role is fully set up.
    visible: false

    Rectangle {
        id: frame
        anchors.fill: parent
        color: "#ee12101d"
        radius: 16
        border.color: "#2a2640"
        border.width: 1
        implicitWidth: row.implicitWidth + 32
        implicitHeight: 64

        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: 8

            Repeater {
                model: OverlayController.variants
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    readonly property bool active: index === OverlayController.currentIndex
                    width: 44
                    height: 44
                    radius: 10
                    color: active ? "#4ade80" : "#1a1728"
                    border.color: active ? "#4ade80" : "#2a2640"
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: 120 } }

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: active ? "#08060f" : "#f0fdf4"
                        font.family: "JetBrains Mono"
                        font.pixelSize: modelData.length > 1 ? 16 : 20
                        font.weight: Font.Medium
                    }
                }
            }
        }
    }
}
