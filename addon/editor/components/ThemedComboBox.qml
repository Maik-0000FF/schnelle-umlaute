import QtQuick
import QtQuick.Controls
import SchnelleUmlaute

// Drop-in ComboBox replacement that styles every visible surface from
// the app's Theme instead of relying on Qt Quick Controls' system
// palette fallback. The plain ComboBox theming we used inline in
// Settings.qml only covered the collapsed text — the popup, delegates
// and dropdown indicator stayed system-coloured, which made the open
// list look out of place on every theme.
//
// Supports both plain-string models and object models via textRole:
// the delegate falls back to modelData when textRole is empty.
ComboBox {
    id: combo
    implicitHeight: 34
    font.family: Theme.fontFamily
    font.pixelSize: 13

    contentItem: Text {
        text: combo.displayText
        color: Theme.text
        font: combo.font
        leftPadding: Theme.spacingMd
        rightPadding: combo.indicator.width + Theme.spacingSm
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: Theme.background
        border.color: combo.activeFocus ? Theme.borderFocus : Theme.border
        border.width: 1
        Behavior on border.color { ColorAnimation { duration: Theme.animShort } }
    }

    indicator: Canvas {
        x: combo.width - width - Theme.spacingMd
        y: (combo.height - height) / 2
        width: 10
        height: 6
        contextType: "2d"
        // Repaint when the theme palette changes — Theme.textMuted is
        // a binding, but Canvas only re-renders on explicit requestPaint.
        Connections {
            target: Theme
            function onCurrentChanged() { combo.indicator.requestPaint() }
        }
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.fillStyle = Theme.textMuted;
            ctx.beginPath();
            ctx.moveTo(0, 0);
            ctx.lineTo(width, 0);
            ctx.lineTo(width / 2, height);
            ctx.closePath();
            ctx.fill();
        }
    }

    delegate: ItemDelegate {
        id: item
        required property int index
        required property var modelData
        width: combo.width
        implicitHeight: 32
        readonly property bool current: combo.currentIndex === index
        readonly property string itemLabel:
            combo.textRole && modelData && typeof modelData === "object"
                ? modelData[combo.textRole]
                : modelData
        contentItem: Text {
            text: item.itemLabel
            // Use Theme.accent (varies per theme) instead of Theme.brand
            // (constant green across themes) so the "active item" stamp
            // reads as part of the current theme rather than as the
            // schnelle-umlaute brand bleeding into every palette.
            color: item.current ? Theme.accent : Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: 13
            font.weight: item.current ? Font.Medium : Font.Normal
            leftPadding: Theme.spacingMd
            rightPadding: Theme.spacingMd
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: item.hovered ? Theme.surfaceHover : Theme.surface
            Behavior on color { ColorAnimation { duration: Theme.animShort } }
        }
    }

    popup: Popup {
        y: combo.height + 2
        width: combo.width
        // Cap the dropdown so very large models still fit on small
        // screens — 6 rows × 32 px + 2 × 4 px padding.
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 6 * 32 + 8)
        padding: 4

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: combo.delegateModel
            currentIndex: combo.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            color: Theme.surface
            radius: Theme.radiusSm
            border.color: Theme.border
            border.width: 1
        }
    }
}
