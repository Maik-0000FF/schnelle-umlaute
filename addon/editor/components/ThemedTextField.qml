import QtQuick
import QtQuick.Controls
import SchnelleUmlaute

// Drop-in TextField replacement that carries a themed right-click
// context menu (Cut/Copy/Paste/Select All) and the input defaults shared
// by every editor field. A plain TextField on the Basic style falls back
// to the unstyled platform/Basic context menu, which ignores Theme.qml and
// looks out of place against the dark editor (see issue #43). Centralising
// the menu here keeps its styling in one place; the field-specific
// background and font stay at the call site, since those vary per input
// (error/warning/accent borders, mono sizes).
TextField {
    id: control

    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectByMouse: true

    // One styling for every menu row, so the four entries below don't each
    // repeat their contentItem/background. Per-instance text/enabled/action
    // are set on the MenuEntry uses themselves.
    component MenuEntry: MenuItem {
        id: entry
        implicitHeight: Theme.controlHeight
        implicitWidth: 160
        contentItem: Text {
            text: entry.text
            color: entry.enabled ? Theme.text : Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            verticalAlignment: Text.AlignVCenter
            leftPadding: Theme.spacingMd
            rightPadding: Theme.spacingMd
        }
        background: Rectangle {
            color: entry.highlighted ? Theme.surfaceHover : "transparent"
            radius: Theme.radiusSm
            Behavior on color { ColorAnimation { duration: Theme.animShort } }
        }
    }

    // Right button only, so left-button selection still reaches the
    // TextInput. point.position is in control-local coordinates, which is
    // also the menu's parent frame, so the menu opens under the cursor.
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: contextMenu.popup(point.position)
    }

    Menu {
        id: contextMenu
        padding: Theme.spacingXs

        background: Rectangle {
            implicitWidth: 160
            color: Theme.surface
            radius: Theme.radiusSm
            border.color: Theme.border
            border.width: 1
        }

        MenuEntry {
            text: qsTr("Cut")
            enabled: !control.readOnly && control.selectedText.length > 0
            onTriggered: control.cut()
        }
        MenuEntry {
            text: qsTr("Copy")
            enabled: control.selectedText.length > 0
            onTriggered: control.copy()
        }
        MenuEntry {
            text: qsTr("Paste")
            enabled: !control.readOnly && control.canPaste
            onTriggered: control.paste()
        }
        MenuSeparator {
            padding: 0
            topPadding: Theme.spacingXs
            bottomPadding: Theme.spacingXs
            contentItem: Rectangle { implicitHeight: 1; color: Theme.border }
        }
        MenuEntry {
            text: qsTr("Select All")
            enabled: control.length > 0
            onTriggered: control.selectAll()
        }
    }
}
