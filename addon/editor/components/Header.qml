import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Rectangle {
    id: root
    color: Theme.background
    implicitHeight: 56

    property int mappingCount: 0

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingLg
        anchors.rightMargin: Theme.spacingMd
        spacing: Theme.spacingMd

        Image {
            source: "qrc:/qt/qml/SchnelleUmlaute/assets/schnelle-umlaute-icon.png"
            sourceSize.width: 64
            sourceSize.height: 64
            width: 32
            height: 32
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        RowLayout {
            spacing: 6
            Text {
                text: "Schnelle"
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 16
                font.weight: Font.Medium
            }
            Text {
                text: "Umlaute"
                color: Theme.brand
                font.family: Theme.fontFamily
                font.pixelSize: 16
                font.weight: Font.Medium
            }
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
    }
}
