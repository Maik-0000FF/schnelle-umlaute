import QtQuick
import SchnelleUmlaute

// Shared dropdown chevron so every dropdown-like control (ThemedComboBox, the
// profile selector) draws the same triangle instead of mixing a Canvas
// triangle with text glyphs. Points down by default; set pointingUp while the
// popup is open.
Canvas {
    id: indicator
    property bool pointingUp: false
    width: 10
    height: 6
    contextType: "2d"

    onPointingUpChanged: requestPaint()
    // Theme.textMuted is a binding, but Canvas only repaints on an explicit
    // requestPaint, so nudge it when the palette changes.
    Connections {
        target: Theme
        function onCurrentChanged() { indicator.requestPaint() }
    }

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.fillStyle = Theme.textMuted;
        ctx.beginPath();
        if (pointingUp) {
            ctx.moveTo(0, height);
            ctx.lineTo(width, height);
            ctx.lineTo(width / 2, 0);
        } else {
            ctx.moveTo(0, 0);
            ctx.lineTo(width, 0);
            ctx.lineTo(width / 2, height);
        }
        ctx.closePath();
        ctx.fill();
    }
}
