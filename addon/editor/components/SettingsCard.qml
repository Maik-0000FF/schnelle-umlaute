import QtQuick
import QtQuick.Layouts
import SchnelleUmlaute

Rectangle {
    id: root
    radius: Theme.radiusLg
    color: Theme.surface
    border.color: Theme.border
    border.width: 1
    Layout.fillWidth: true
    implicitHeight: col.implicitHeight + Theme.spacingLg * 2

    property string titleText: ""
    default property alias content: inner.data

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        Text {
            text: root.titleText
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1
        }

        ColumnLayout {
            id: inner
            Layout.fillWidth: true
            spacing: Theme.spacingMd
        }
    }
}
