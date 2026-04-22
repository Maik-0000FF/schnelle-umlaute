import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Rectangle {
    id: root
    color: Theme.background
    implicitHeight: 56

    property int mappingCount: 0
    signal reloadRequested()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingLg
        anchors.rightMargin: Theme.spacingMd
        spacing: Theme.spacingMd

        Rectangle {
            width: 28
            height: 28
            radius: 8
            color: Theme.brandSoft
            Text {
                anchors.centerIn: parent
                text: "◆"
                color: Theme.brand
                font.pixelSize: 16
            }
        }

        Text {
            textFormat: Text.StyledText
            text: '<span style="color:' + Theme.text + '">Schnelle</span> ' +
                  '<span style="color:' + Theme.brand + '">Umlaute</span>'
            font.family: Theme.fontFamily
            font.pixelSize: 15
            font.weight: Font.Medium
        }

        Rectangle {
            Layout.leftMargin: Theme.spacingSm
            height: 22
            width: countLabel.implicitWidth + Theme.spacingMd * 2
            radius: 11
            color: Theme.surface
            border.color: Theme.border
            border.width: 1

            Text {
                id: countLabel
                anchors.centerIn: parent
                text: qsTr("%1 mappings").arg(root.mappingCount)
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: 11
            }
        }

        Item { Layout.fillWidth: true }

        ToolButton {
            text: "↻"
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Reload (F5)")
            font.pixelSize: 16
            contentItem: Text {
                text: parent.text
                color: parent.hovered ? Theme.accent : Theme.textMuted
                font.pixelSize: parent.font.pixelSize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                Behavior on color { ColorAnimation { duration: Theme.animShort } }
            }
            background: Rectangle {
                color: parent.hovered ? Theme.surfaceHover : "transparent"
                radius: Theme.radiusSm
            }
            onClicked: root.reloadRequested()
        }
    }
}
