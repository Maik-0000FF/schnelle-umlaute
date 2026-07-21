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

    // Navigating the open popup by keyboard switches its highlight into
    // keyboard mode (see the delegate); accepted stays false so the ComboBox
    // still performs the move. A row hover switches it back to mouse mode.
    Keys.onPressed: (event) => {
        switch (event.key) {
        case Qt.Key_Up:
        case Qt.Key_Down:
        case Qt.Key_PageUp:
        case Qt.Key_PageDown:
        case Qt.Key_Home:
        case Qt.Key_End:
            popupList.keyboardActive = true;
        }
        event.accepted = false;
    }

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
        color: Theme.comboBoxSurface
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
        // Match the list's own width (not combo.width) so the rounded row
        // highlight sits inside the popup padding, exactly like the
        // Profile/Library rows.
        width: item.ListView.view.width
        // Custom contentItem supplies its own padding; clear the style's so
        // the row height is fixed at Theme.controlHeight regardless of style /
        // Qt version.
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

        // Mouse-hover state comes from a HoverHandler, exactly like the
        // Profile/Library dropdown rows, so hover looks and feels identical
        // across every dropdown. Hovering a row also switches the popup back to
        // mouse mode so a stale keyboard highlight never lingers.
        HoverHandler {
            id: itemHover
            onHoveredChanged: if (hovered) popupList.keyboardActive = false
        }
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
            radius: Theme.radiusSm
            // Exactly one highlight at a time, following the popup's active
            // input mode (mirrors the Profile/Library lists): the
            // keyboard-current row while navigating by keys, otherwise the
            // mouse-hovered row. Both use surfaceHover, and there is no colour
            // Behavior, so the highlight snaps instantly instead of smearing
            // into a flicker as the rows or the pointer move.
            color: (popupList.keyboardActive
                    ? item.ListView.isCurrentItem
                    : itemHover.hovered)
                   ? Theme.surfaceHover : "transparent"
        }
    }

    popup: Popup {
        y: combo.height + 2
        width: combo.width
        // Cap the dropdown so very large models still fit on small screens:
        // 6 rows plus the popup's top and bottom padding.
        implicitHeight: Math.min(contentItem.implicitHeight + 2 * Theme.spacingXs,
                                 6 * Theme.controlHeight + 2 * Theme.spacingXs)
        padding: Theme.spacingXs

        contentItem: ListView {
            id: popupList
            clip: true
            implicitHeight: contentHeight
            model: combo.delegateModel
            currentIndex: combo.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
            // Active input mode for the single-highlight rule (mirrors the
            // Profile/Library lists): a navigation key means keyboard, a row
            // hover means mouse. Set true by the combo's key handler, back to
            // false by a row's HoverHandler.
            property bool keyboardActive: false
        }

        background: Rectangle {
            // Shared dropdown look (Theme.dropdownSurface/Border): darker than
            // the surface cards behind it, so the open list reads as a distinct
            // floating layer like the Profile/Library dropdowns.
            color: Theme.dropdownSurface
            radius: Theme.radiusSm
            border.color: Theme.dropdownBorder
            border.width: 1
        }
    }
}
