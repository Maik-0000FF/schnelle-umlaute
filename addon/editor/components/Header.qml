import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Rectangle {
    id: root
    color: Theme.background
    implicitHeight: 56

    property int mappingCount: 0
    signal aboutRequested()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingLg
        anchors.rightMargin: Theme.spacingMd
        spacing: Theme.spacingMd

        Image {
            source: "qrc:/qt/qml/SchnelleUmlaute/assets/schnelle-umlaute-icon.png"
            sourceSize.width: 48
            sourceSize.height: 48
            width: 24
            height: 24
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        RowLayout {
            spacing: 6
            Text {
                text: "Schnelle"
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontStrong
                font.weight: Font.Medium
            }
            Text {
                text: "Umlaute"
                color: Theme.brand
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontStrong
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
                font.pixelSize: Theme.fontBody
            }
        }

        Item { Layout.fillWidth: true }

        // Opens the About dialog. Sits at the header's right edge, muted until
        // hovered/focused so it stays unobtrusive.
        Rectangle {
            id: infoBtn
            implicitWidth: Theme.controlHeightSm
            implicitHeight: Theme.controlHeightSm
            radius: Theme.radiusSm
            color: (infoMouse.containsMouse || infoBtn.activeFocus)
                   ? Theme.surfaceHover : "transparent"
            border.color: infoBtn.activeFocus ? Theme.borderFocus : "transparent"
            border.width: 1
            activeFocusOnTab: true
            Behavior on color { ColorAnimation { duration: Theme.animShort } }

            Text {
                anchors.centerIn: parent
                text: Theme.iconInfo
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontStrong
            }
            MouseArea {
                id: infoMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.aboutRequested()
            }
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Space || event.key === Qt.Key_Return
                    || event.key === Qt.Key_Enter) {
                    root.aboutRequested();
                    event.accepted = true;
                }
            }
        }
    }
}
