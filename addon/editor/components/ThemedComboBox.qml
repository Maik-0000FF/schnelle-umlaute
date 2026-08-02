import QtQuick
import QtQuick.Controls
import SchnelleUmlaute

// Custom dropdown (deliberately NOT a QtQuick Controls ComboBox). The built-in
// ComboBox cannot do two things this UI needs at once:
//   1. A click on the open header should close it. The built-in one closes
//      (press-outside) then reopens on the same click.
//   2. Arrow keys should navigate after opening with the mouse. The built-in
//      popup closes on focus-out, which clashes with the app-wide FocusSink.
// A custom header + custom Popup (whose list owns keyboard navigation and whose
// Popup does not close on focus-out) has neither problem. This mirrors the
// Profile/Library dropdown headers.
//
// Public API kept compatible with the previous ComboBox use: model (a
// plain-string or object array), textRole, valueRole, currentIndex (read and
// bindable), and the activated() signal. Object models may carry an
// `unavailable: true` field, which dims the row and the collapsed box.
Item {
    id: combo
    implicitHeight: Theme.controlHeight
    implicitWidth: 120

    property var model: []
    property string textRole: ""
    // Accepted for call-site compatibility; the callers read model[currentIndex]
    // directly, so the value role is not needed internally.
    property string valueRole: ""
    property int currentIndex: 0
    signal activated(int index)

    function isUnavailable(d) {
        return d && typeof d === "object" && d.unavailable === true;
    }
    // Row label: the textRole field for object models, the element itself for
    // plain-string models.
    function labelFor(d) {
        return (combo.textRole && d && typeof d === "object") ? d[combo.textRole] : d;
    }

    readonly property var currentData:
        (combo.model && combo.currentIndex >= 0 && combo.currentIndex < combo.model.length)
            ? combo.model[combo.currentIndex] : null
    readonly property string displayText:
        combo.currentData !== null ? combo.labelFor(combo.currentData) : ""
    readonly property bool currentUnavailable: combo.isUnavailable(combo.currentData)

    // Commit a choice: notify with the picked index and close. Deliberately
    // does NOT assign currentIndex — call sites bind that to their model, and
    // an imperative write here would destroy the binding on the first pick,
    // after which the box would keep showing whatever was chosen last even if
    // the model rejected the value or changed from elsewhere. The handler gets
    // the index as its argument, so the write is not needed for the round trip:
    // the model changes, the binding re-evaluates, the box follows.
    function select(i) {
        combo.activated(i);
        popup.close();
    }

    // Collapsed box (header).
    Rectangle {
        id: header
        anchors.fill: parent
        radius: Theme.radiusSm
        color: Theme.comboBoxSurface
        border.color: (header.activeFocus || popup.visible)
                      ? Theme.borderFocus : Theme.border
        border.width: 1
        Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

        // Reachable by Tab; Space/Enter/Down open the dropdown (keyboard mode).
        activeFocusOnTab: true
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return
                || event.key === Qt.Key_Enter || event.key === Qt.Key_Down) {
                if (!popup.visible) {
                    popup.keyboardSession = true;
                    popup.open();
                }
                event.accepted = true;
            }
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacingMd
            anchors.right: chevron.left
            anchors.rightMargin: Theme.spacingSm
            anchors.verticalCenter: parent.verticalCenter
            text: combo.displayText
            // Dim the collapsed box to match the open dropdown's delegate when
            // the current entry is flagged unavailable.
            color: combo.currentUnavailable ? Theme.textMuted : Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            elide: Text.ElideRight
        }
        DropdownIndicator {
            id: chevron
            anchors.right: parent.right
            anchors.rightMargin: Theme.spacingMd
            anchors.verticalCenter: parent.verticalCenter
            pointingUp: popup.visible
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (popup.visible) {
                    popup.close();
                } else {
                    popup.keyboardSession = false;
                    popup.open();
                }
            }
        }
    }

    Popup {
        id: popup
        y: header.height + 2
        width: header.width
        padding: Theme.spacingXs
        // Close on a press outside the header (the popup's parent), so a click
        // on the header toggles it shut (the MouseArea decides) instead of
        // auto-closing; a press anywhere else dismisses it. Unlike a ComboBox
        // popup this one does not close on focus-out, so the list can hold
        // keyboard focus while the FocusSink is around.
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        // Cap at 6 rows plus the popup's top and bottom padding so a long model
        // still fits on small screens.
        implicitHeight: Math.min(contentItem.implicitHeight + 2 * Theme.spacingXs,
                                 6 * Theme.controlHeight + 2 * Theme.spacingXs)

        // True only when opened via keyboard, so focus returns to the header on
        // close for keyboard users but is left alone for mouse users.
        property bool keyboardSession: false
        // On open, start the keyboard cursor on the current selection and give
        // the list focus so arrow keys work straight away, mouse-opened too.
        // Start in mouse mode; the first arrow flips it, a hover flips it back.
        onOpened: {
            list.currentIndex = combo.currentIndex;
            list.keyboardActive = popup.keyboardSession;
            list.forceActiveFocus();
        }
        onClosed: {
            if (popup.keyboardSession)
                header.forceActiveFocus();
            popup.keyboardSession = false;
        }

        contentItem: ListView {
            id: list
            clip: true
            implicitHeight: contentHeight
            model: combo.model
            keyNavigationEnabled: true
            ScrollIndicator.vertical: ScrollIndicator {}
            // Active input mode for the single-highlight rule (mirrors the
            // Profile/Library lists): a navigation key means keyboard, a row
            // hover means mouse.
            property bool keyboardActive: false

            Keys.onPressed: (event) => {
                switch (event.key) {
                case Qt.Key_Up:
                case Qt.Key_Down:
                case Qt.Key_PageUp:
                case Qt.Key_PageDown:
                case Qt.Key_Home:
                case Qt.Key_End:
                    // Let keyNavigationEnabled move currentIndex; just flag the
                    // mode so the highlight follows the keyboard cursor.
                    list.keyboardActive = true;
                    event.accepted = false;
                    break;
                case Qt.Key_Return:
                case Qt.Key_Enter:
                case Qt.Key_Space:
                    combo.select(list.currentIndex);
                    event.accepted = true;
                    break;
                }
            }

            delegate: Rectangle {
                id: row
                required property int index
                required property var modelData
                width: ListView.view.width
                height: Theme.controlHeight
                radius: Theme.radiusSm
                readonly property bool selected: combo.currentIndex === index
                readonly property bool itemUnavailable: combo.isUnavailable(modelData)
                // Exactly one highlight at a time, following the active input
                // mode: the keyboard cursor while navigating by keys, otherwise
                // the mouse-hovered row. No colour Behavior, so it snaps.
                color: (list.keyboardActive
                        ? (row.ListView.isCurrentItem && list.activeFocus)
                        : rowHover.hovered)
                       ? Theme.surfaceHover : "transparent"

                HoverHandler {
                    id: rowHover
                    onHoveredChanged: if (hovered) list.keyboardActive = false
                }

                Text {
                    anchors.fill: parent
                    leftPadding: Theme.spacingMd
                    rightPadding: Theme.spacingMd
                    verticalAlignment: Text.AlignVCenter
                    text: combo.labelFor(row.modelData)
                    // Theme.accent (per-theme) marks the selected entry, not
                    // Theme.brand (constant green), so it reads as part of the
                    // active theme. Unavailable rows dim to muted.
                    color: row.itemUnavailable ? Theme.textMuted
                           : row.selected ? Theme.accent : Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    font.weight: row.selected ? Font.Medium : Font.Normal
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: combo.select(row.index)
                }
            }
        }

        background: Rectangle {
            // Shared dropdown look: darker than the surface cards behind it, so
            // the open list reads as a distinct floating layer like the
            // Profile/Library dropdowns.
            color: Theme.dropdownSurface
            radius: Theme.radiusSm
            border.color: Theme.dropdownBorder
            border.width: 1
        }
    }
}
