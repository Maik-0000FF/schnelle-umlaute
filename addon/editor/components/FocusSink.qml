import QtQuick

// "Click anywhere empty to drop keyboard focus" catcher. It fills its page on
// top of everything, but every press passes straight through (accepted =
// false), so controls, the Settings ScrollView, the mapping list's drag
// handles and the dropdowns all keep working untouched. The one thing it does
// is pull keyboard focus onto itself on each press, which makes an armed
// key-capture field (the custom leader rows on Settings, the cycle and
// per-profile select-key fields on Mappings) disarm through its own
// onActiveFocusChanged, no matter where on the page the click lands. One
// source instead of a per-field patch.
MouseArea {
    anchors.fill: parent
    acceptedButtons: Qt.LeftButton
    onPressed: (mouse) => {
        forceActiveFocus(Qt.MouseFocusReason);
        mouse.accepted = false;
    }
}
