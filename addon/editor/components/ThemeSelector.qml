import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute
import SchnelleUmlautePalette

// Theme picker: a compact dropdown. Each row (and the collapsed header) shows
// the theme name plus a small preview "pill" that holds the theme's four accent
// swatch circles on the theme's own background colour, a quick visual preview.
// Mirrors ThemedComboBox's header + Popup + keyboard model (see that file for
// why a custom dropdown, not a ComboBox).
//
// Holds no state of its own: the caller passes the selected id in and gets the
// pick back as a signal. That is what lets the same picker serve the main
// choice and the two halves of the automatic light/dark pair, instead of three
// near-copies wired to three different properties.
Item {
    id: sel
    implicitHeight: Theme.controlHeight
    implicitWidth: 200

    // The synthetic first entry: a mode, not a palette. Named once here so the
    // call sites, the label and the preview cannot drift apart.
    readonly property string autoId: "auto"

    property string selectedId: "schnelle-umlaute"
    // Only the main picker offers the automatic entry; offering it inside the
    // pair would let the mode point at itself.
    property bool includeAuto: false
    // The automatic entry has no palette of its own, so its pill borrows the
    // half that is currently rendered.
    property string autoPreviewId: "schnelle-umlaute"
    signal picked(string id)

    readonly property var ids:
        sel.includeAuto ? [sel.autoId].concat(Palettes.ids) : Palettes.ids
    readonly property int currentIndex: Math.max(0,
        sel.ids.indexOf(sel.selectedId))

    function labelFor(id) {
        return id === sel.autoId ? qsTr("Automatic (follow system)")
                                 : (Palettes.labels[id] || id);
    }
    function pillIdFor(id) {
        return id === sel.autoId ? sel.autoPreviewId : id;
    }

    function selectId(id) {
        sel.picked(id);
        popup.close();
    }

    // The compact preview pill: the theme's background fill holding its four
    // accent circles. Sized to the circles, not the row width.
    component ColorPill: Rectangle {
        property string themeId: "schnelle-umlaute"
        readonly property var pal: Palettes.get(themeId)
        implicitWidth: circles.width + 2 * Theme.spacingSm
        implicitHeight: 24
        radius: height / 2
        color: pal.background
        border.color: pal.border
        border.width: 1

        Row {
            id: circles
            anchors.centerIn: parent
            spacing: Theme.spacingXs
            Repeater {
                model: pal.swatches
                delegate: Rectangle {
                    required property string modelData
                    width: 14
                    height: 14
                    radius: width / 2
                    color: modelData
                    // Subtle neutral ring so a swatch near the pill fill stays
                    // visible on light and dark themes alike.
                    border.color: "#33808080"
                    border.width: 1
                }
            }
        }
    }

    // Collapsed header: the active theme's name + preview pill + chevron.
    Rectangle {
        id: header
        anchors.fill: parent
        radius: Theme.radiusSm
        color: Theme.comboBoxSurface
        border.color: (header.activeFocus || popup.visible)
                      ? Theme.borderFocus : Theme.border
        border.width: 1
        Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

        activeFocusOnTab: true
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return
                || event.key === Qt.Key_Enter || event.key === Qt.Key_Down) {
                if (!popup.visible) {
                    popup.keyboardSession = true;
                    popup.open();
                }
                event.accepted = true;
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.spacingMd
            anchors.rightMargin: Theme.spacingMd
            spacing: Theme.spacingMd

            Text {
                Layout.fillWidth: true
                text: sel.labelFor(sel.selectedId)
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
                elide: Text.ElideRight
            }
            ColorPill {
                Layout.alignment: Qt.AlignVCenter
                themeId: sel.pillIdFor(sel.selectedId)
            }
            DropdownIndicator {
                Layout.alignment: Qt.AlignVCenter
                pointingUp: popup.visible
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (popup.visible) {
                    popup.close();
                } else {
                    popup.keyboardSession = false;
                    popup.open();
                }
            }
        }
    }

    Popup {
        id: popup
        y: header.height + 2
        width: header.width
        padding: Theme.spacingXs
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        // Cap at 6 rows plus padding so the full list still fits on small
        // screens; the rest scrolls.
        implicitHeight: Math.min(contentItem.implicitHeight + 2 * Theme.spacingXs,
                                 6 * Theme.controlHeight + 2 * Theme.spacingXs)

        property bool keyboardSession: false
        onOpened: {
            list.currentIndex = sel.currentIndex;
            list.keyboardActive = popup.keyboardSession;
            list.forceActiveFocus();
        }
        onClosed: {
            if (popup.keyboardSession)
                header.forceActiveFocus();
            popup.keyboardSession = false;
        }

        contentItem: ListView {
            id: list
            clip: true
            implicitHeight: contentHeight
            model: sel.ids
            keyNavigationEnabled: true
            ScrollIndicator.vertical: ScrollIndicator {}
            property bool keyboardActive: false

            Keys.onPressed: (event) => {
                switch (event.key) {
                case Qt.Key_Up:
                case Qt.Key_Down:
                case Qt.Key_PageUp:
                case Qt.Key_PageDown:
                case Qt.Key_Home:
                case Qt.Key_End:
                    list.keyboardActive = true;
                    event.accepted = false;
                    break;
                case Qt.Key_Return:
                case Qt.Key_Enter:
                case Qt.Key_Space:
                    sel.selectId(sel.ids[list.currentIndex]);
                    event.accepted = true;
                    break;
                }
            }

            delegate: Rectangle {
                id: row
                required property int index
                required property string modelData
                width: ListView.view.width
                height: Theme.controlHeight
                radius: Theme.radiusSm
                readonly property bool selected:
                    row.modelData === sel.selectedId
                readonly property bool highlighted: list.keyboardActive
                    ? (row.ListView.isCurrentItem && list.activeFocus)
                    : rowHover.hovered
                color: highlighted ? Theme.surfaceHover : "transparent"

                HoverHandler {
                    id: rowHover
                    onHoveredChanged: if (hovered) list.keyboardActive = false
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spacingMd
                    anchors.rightMargin: Theme.spacingMd
                    spacing: Theme.spacingMd

                    Text {
                        Layout.fillWidth: true
                        text: sel.labelFor(row.modelData)
                        // Theme.accent (per-theme) marks the active entry so it
                        // reads as part of the current theme.
                        color: row.selected ? Theme.accent : Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        font.weight: row.selected ? Font.Medium : Font.Normal
                        elide: Text.ElideRight
                    }
                    ColorPill {
                        Layout.alignment: Qt.AlignVCenter
                        themeId: sel.pillIdFor(row.modelData)
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: sel.selectId(row.modelData)
                }
            }
        }

        background: Rectangle {
            color: Theme.dropdownSurface
            radius: Theme.radiusSm
            border.color: Theme.dropdownBorder
            border.width: 1
        }
    }
}
