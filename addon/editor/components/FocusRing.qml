import QtQuick
import SchnelleUmlaute

// Keyboard-focus indicator: a ring in the theme accent drawn just outside its
// target, so it reads on top of any control background or border. Anchor it to
// fill the focusable control and bind `visible` to that control's activeFocus:
//
//   FocusRing { visible: parent.activeFocus }
//
// Override `radius` to match rounded controls (defaults to a small radius plus
// the ring width so the outset corners stay concentric). Purely decorative and
// non-interactive: a plain Rectangle ignores pointer events, so it never steals
// clicks or hover from the control it decorates.
Rectangle {
    anchors.fill: parent
    anchors.margins: -Theme.focusRingWidth
    z: 10
    color: "transparent"
    radius: Theme.radiusSm + Theme.focusRingWidth
    border.color: Theme.focusRing
    border.width: Theme.focusRingWidth
    visible: false
}
