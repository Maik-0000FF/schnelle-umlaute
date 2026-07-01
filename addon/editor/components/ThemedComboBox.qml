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
    implicitHeight: Theme.controlHeight
    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontBody

    // The custom contentItem/indicator handle their own insets, so drop the
    // control's style-supplied padding/spacing. Without this the box height
    // and content offset vary with the active Quick Controls style / Qt
    // version, which shifts the surrounding row layout.
    padding: 0
    spacing: 0

    // Single source for the "unavailable" predicate, shared by the collapsed
    // box (currentUnavailable) and the open dropdown delegate (itemUnavailable)
    // so the dimming rule lives in one place. Object models may carry an
    // `unavailable: true` field; plain string models leave it undefined, so
    // they render unchanged.
    function isUnavailable(d) {
        return d && typeof d === "object" && d.unavailable === true;
    }

    // Whether the currently selected entry is flagged unavailable, so the
    // collapsed box dims to match the open dropdown's delegate.
    readonly property bool currentUnavailable:
        combo.isUnavailable(combo.model && combo.currentIndex >= 0
            ? combo.model[combo.currentIndex] : null)

    contentItem: Text {
        text: combo.displayText
        color: combo.currentUnavailable ? Theme.textMuted : Theme.text
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

    indicator: DropdownIndicator {
        x: combo.width - width - Theme.spacingMd
        y: (combo.height - height) / 2
    }

    delegate: ItemDelegate {
        id: item
        required property int index
        required property var modelData
        width: combo.width
        // Custom contentItem supplies its own padding; clear the style's so
        // row height is fixed at 32 regardless of style / Qt version.
        padding: 0
        implicitHeight: Theme.controlHeight
        readonly property bool current: combo.currentIndex === index
        readonly property string itemLabel:
            combo.textRole && modelData && typeof modelData === "object"
                ? modelData[combo.textRole]
                : modelData
        // Per-item dimming for a choice the environment can't honour (e.g. a
        // placement that needs a missing protocol). Uses the shared predicate
        // on the combo root; string models render unchanged.
        readonly property bool itemUnavailable: combo.isUnavailable(modelData)
        contentItem: Text {
            text: item.itemLabel
            // Use Theme.accent (varies per theme) instead of Theme.brand
            // (constant green across themes) so the "active item" stamp
            // reads as part of the current theme rather than as the
            // schnelle-umlaute brand bleeding into every palette.
            color: item.itemUnavailable ? Theme.textMuted
                   : item.current ? Theme.accent : Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
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
