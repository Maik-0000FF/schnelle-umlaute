import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

// A small click-to-capture hotkey field. Click it, press a key combo with a
// real modifier (Ctrl/Alt/Super), and it emits the fcitx portable string via
// captured(). Escape or clicking away cancels; the ✕ clears the binding
// (captured("")). The displayed value is owned by the parent (bound to the
// model), so a rejected combo simply leaves the old value showing.
Item {
    id: root

    property string value: ""
    property string placeholder: qsTr("Set key…")
    // Emitted with a portable combo string, or "" to clear. The parent
    // persists it (and may reject duplicates, leaving value unchanged).
    signal captured(string combo)

    property bool capturing: false
    property bool invalid: false // last press lacked a usable modifier

    implicitHeight: Theme.controlHeight - 6
    implicitWidth: Math.max(110, rowL.implicitWidth)

    RowLayout {
        id: rowL
        anchors.fill: parent
        spacing: Theme.spacingXs

        Rectangle {
            id: pill
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusSm
            color: Theme.background
            border.width: 1
            border.color: root.capturing ? Theme.borderFocus
                          : (root.invalid ? Theme.error : Theme.border)
            Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

            Text {
                anchors.fill: parent
                leftPadding: Theme.spacingSm
                rightPadding: Theme.spacingSm
                text: root.capturing ? qsTr("Press keys…")
                      : (root.value.length ? root.value : root.placeholder)
                color: root.capturing ? Theme.accent
                       : (root.value.length ? Theme.text : Theme.textMuted)
                font.family: Theme.fontFamily
                font.pixelSize: 12
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.invalid = false;
                    root.capturing = true;
                    root.forceActiveFocus();
                }
            }
        }

        ToolButton {
            visible: root.value.length > 0 && !root.capturing
            text: "✕"
            implicitWidth: 22
            contentItem: Text {
                text: parent.text
                color: parent.hovered ? Theme.error : Theme.textMuted
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle { color: "transparent" }
            ThemedToolTip { visible: parent.hovered; text: qsTr("Clear shortcut") }
            onClicked: root.captured("")
        }
    }

    Keys.onPressed: (event) => {
        if (!root.capturing)
            return;
        var k = event.key;
        // Wait for the non-modifier key; ignore standalone modifier presses.
        if (k === Qt.Key_Control || k === Qt.Key_Alt || k === Qt.Key_Shift
            || k === Qt.Key_Meta || k === Qt.Key_AltGr || k === Qt.Key_CapsLock) {
            event.accepted = true;
            return;
        }
        if (k === Qt.Key_Escape) {
            root.capturing = false;
            event.accepted = true;
            return;
        }
        var combo = KeyComboUtil.toPortable(k, event.modifiers);
        if (combo.length > 0) {
            root.capturing = false;
            root.invalid = false;
            root.captured(combo);
        } else {
            // No real modifier or an unsupported key; keep listening, flag it.
            root.invalid = true;
        }
        event.accepted = true;
    }

    onActiveFocusChanged: if (!activeFocus) root.capturing = false;
}
