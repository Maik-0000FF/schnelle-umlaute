import QtQuick
import QtQuick.Layouts
import SchnelleUmlaute

// Shared settings-row base with a full-width hover highlight. With the
// right-aligned layout a row's label (left) and its control (far right) can sit
// far apart, so hovering anywhere on the strip tints the whole row and keeps
// the two visually linked. The fill highlight switches instantly (no fade),
// matching the mapping-row hover. Content is laid out in a RowLayout via the
// default property: callers add the label and controls, and a fillWidth label
// pushes the control to the right edge.
Rectangle {
    id: root
    Layout.fillWidth: true
    default property alias content: rowContent.data

    radius: Theme.radiusSm
    color: hoverHandler.hovered ? Theme.surfaceHover : "transparent"
    implicitHeight: rowContent.implicitHeight + 2 * Theme.spacingXs

    // Passive grabber: it stays hovered while the pointer is over the switches
    // and their tooltips, so the row highlight never flickers as the cursor
    // reaches a toggle at the far edge.
    HoverHandler { id: hoverHandler }

    RowLayout {
        id: rowContent
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.spacingMd
    }
}
