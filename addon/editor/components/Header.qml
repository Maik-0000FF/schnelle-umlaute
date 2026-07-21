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
            source: Theme.appIconSource
            sourceSize.width: Theme.appIconSizeSm * 2 // 2x for HiDPI crispness
            sourceSize.height: Theme.appIconSizeSm * 2
            width: Theme.appIconSizeSm
            height: Theme.appIconSizeSm
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        Wordmark {}

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
        FocusRect {
            id: infoBtn
            implicitWidth: Theme.controlHeightSm
            implicitHeight: Theme.controlHeightSm
            color: (infoBtn.hovered || infoBtn.activeFocus)
                   ? Theme.surfaceHover : "transparent"
            border.color: infoBtn.activeFocus ? Theme.borderFocus : "transparent"
            border.width: 1
            onActivated: root.aboutRequested()

            // Icon-only control, so it carries an explicit accessible name and a
            // tooltip that every text-labelled control gets for free.
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("About")

            ThemedToolTip {
                hovered: infoBtn.hovered
                text: qsTr("About")
            }

            Text {
                anchors.centerIn: parent
                text: Theme.iconInfo
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontStrong
            }
        }
    }
}
