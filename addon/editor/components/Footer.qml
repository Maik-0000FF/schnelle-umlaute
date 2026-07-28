import QtQuick
import QtQuick.Layouts
import SchnelleUmlaute

Rectangle {
    color: Theme.background
    implicitHeight: 36

    // Shown as-is: already translated by the model.
    property string saveStatus: ""
    // The untranslated counterpart, and the only thing the dot colour is
    // allowed to look at. Matching on saveStatus would compare a tr()'d string
    // against English literals, so the dot would read every state as an error
    // the moment a translation is loaded.
    property int saveState: MappingListModel.NoState

    readonly property color dotColor:
        saveState === MappingListModel.Saved ? Theme.success :
        saveState === MappingListModel.Loaded ? Theme.textMuted :
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
            font.pixelSize: Theme.fontBody
        }

        Item { Layout.fillWidth: true }

        Text {
            text: qsTr("Changes are saved automatically")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            font.italic: true
        }
    }
}
