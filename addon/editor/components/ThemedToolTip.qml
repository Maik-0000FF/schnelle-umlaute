import QtQuick
import QtQuick.Controls
import SchnelleUmlaute

// Drop-in ToolTip styled from the app's Theme instead of the Quick Controls
// system palette. Two ways to trigger it:
//   - Hover hint (delayed, the common case): set `hovered` to the control's
//     hovered/containsMouse state. The tip appears after Theme.tooltipDelay and
//     hides at once when the hover ends, so every hover tooltip shares one delay.
//       ThemedToolTip { hovered: control.hovered; text: qsTr("…") }
//   - Immediate state message: bind `visible` directly (no delay), for feedback
//     that must show the moment its condition holds.
// Use as a child of the control it describes.
ToolTip {
    id: tip
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBody
    padding: Theme.spacingSm

    // Hover trigger with the shared delay. `visible` is driven imperatively
    // here, so binding `visible` directly stays available for non-hover callers.
    property bool hovered: false
    Timer {
        id: showTimer
        interval: Theme.tooltipDelay
        onTriggered: tip.visible = true
    }
    onHoveredChanged: {
        if (tip.hovered) {
            showTimer.restart();
        } else {
            showTimer.stop();
            tip.visible = false;
        }
    }

    contentItem: Text {
        text: tip.text
        color: Theme.text
        font: tip.font
        wrapMode: Text.WordWrap
        // Cap the width so long text wraps onto more lines instead of running
        // off the window; short text keeps its natural single-line width.
        width: Math.min(implicitWidth, Theme.tooltipMaxWidth)
    }

    background: Rectangle {
        color: Theme.surface
        border.color: Theme.border
        border.width: 1
        radius: Theme.radiusSm
    }
}
