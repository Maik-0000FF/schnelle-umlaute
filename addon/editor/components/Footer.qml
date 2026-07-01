import QtQuick
import QtQuick.Layouts
import SchnelleUmlaute

Rectangle {
    color: Theme.background
    implicitHeight: 36

    property string saveStatus: ""

    readonly property color dotColor:
        saveStatus === "Saved" ? Theme.success :
        saveStatus === "Loaded" ? Theme.textMuted :
        Theme.error

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingLg
        anchors.rightMargin: Theme.spacingLg
        spacing: Theme.spacingSm

        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: dotColor
            Behavior on color { ColorAnimation { duration: Theme.animShort } }
        }

        Text {
            text: saveStatus
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontCaption
        }

        Item { Layout.fillWidth: true }

        Text {
            text: qsTr("Changes are saved automatically")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontCaption
            font.italic: true
        }
    }
}
