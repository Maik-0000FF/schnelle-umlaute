import QtQuick
import SchnelleUmlaute

// Keyboard-focus indicator: a ring in the theme accent drawn just inside its
// target, so it stays fully visible and identical everywhere even when the
// control sits inside a clipping container (e.g. a dropdown ListView). Anchor
// it to fill the focusable control and bind `visible` to that control's
// activeFocus:
//
//   FocusRing { visible: parent.activeFocus }
//
// Override `radius` to match rounded controls (defaults to the small radius).
// Purely decorative and non-interactive: a plain Rectangle ignores pointer
// events, so it never steals clicks or hover from the control it decorates.
Rectangle {
    anchors.fill: parent
    z: 10
    color: "transparent"
    radius: Theme.radiusSm
    border.color: Theme.focusRing
    border.width: Theme.focusRingWidth
    visible: false
}
