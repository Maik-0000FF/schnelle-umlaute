import QtQuick
import SchnelleUmlaute

// Keyboard- and pointer-activatable Rectangle: the shared base for the editor's
// custom buttons and rows. Quick Controls' Basic Button silently substitutes a
// system-palette colour for its contentItem text on light desktops, even when
// the colour is bound to a theme token, so the editor builds its buttons from
// plain Rectangles instead (same workaround as PositionPicker's selection
// cell). This base collapses the hover + focus + Space/Enter/Return handling
// those buttons each repeated verbatim.
//
// Callers set their own fill, border and content and bind to `hovered`; the
// `activated()` signal fires on click, Space, Return or Enter. Escape is left
// to the host Popup's CloseOnEscape policy.
Rectangle {
    id: control
    signal activated()

    property alias hovered: hoverHandler.hovered

    radius: Theme.radiusSm
    activeFocusOnTab: true
    // Fill highlight switches instantly (no Behavior on color), matching the
    // mapping-row hover. A fade would leave a dark afterglow trailing the old
    // row when the pointer moves across a list of these (e.g. the About links).
    // Only the focus border animates, like MappingRow.
    Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

    HoverHandler { id: hoverHandler; cursorShape: Qt.PointingHandCursor }
    MouseArea { anchors.fill: parent; onClicked: control.activated() }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return
            || event.key === Qt.Key_Enter) {
            control.activated();
            event.accepted = true;
        }
    }
}
