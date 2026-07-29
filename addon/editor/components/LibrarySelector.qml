import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

// Standalone "Library" dropdown next to the profile selector. A compact header
// button opens a popup listing the bundled presets, grouped by category with a
// search box. Adding one copies it into the user's profiles and registers it
// (model.addProfileFromPreset), so it appears in the profile list and goes live
// immediately. Kept separate from the profile dropdown so neither popup
// overflows and the (potentially large) library has room of its own.
Item {
    id: root

    property var profilesModel: null
    signal requestSnackbar(string message, color c)

    implicitWidth: header.implicitWidth
    implicitHeight: header.implicitHeight

    // Stable category keys (a preset's "# Category:" header) in the order their
    // sections should appear, each mapped to a localized title. An unknown or
    // missing category folds into "other".
    readonly property var categoryOrder: ["language", "symbols", "emoji", "other"]
    function categoryTitle(key) {
        switch (key) {
        case "language": return qsTr("Languages");
        case "symbols":  return qsTr("Symbols & Math");
        case "emoji":    return qsTr("Emoji");
        default:         return qsTr("Other");
        }
    }
    function normCategory(key) {
        return root.categoryOrder.indexOf(key) >= 0 ? key : "other";
    }

    // Loaded fresh each time the popup opens; the search text filters it live.
    property var allPresets: []
    property string query: ""

    // Presets in one category matching the current search, sorted by name. Used
    // per-section so grouping needs no model-role plumbing (the model is a plain
    // JS array). name/description are matched case-insensitively.
    function presetsFor(catKey) {
        var q = root.query.trim().toLowerCase();
        var rows = [];
        for (var i = 0; i < root.allPresets.length; ++i) {
            var p = root.allPresets[i];
            if (root.normCategory(p.category) !== catKey)
                continue;
            if (q.length > 0
                && p.name.toLowerCase().indexOf(q) < 0
                && p.description.toLowerCase().indexOf(q) < 0)
                continue;
            rows.push(p);
        }
        rows.sort(function (a, b) { return a.name.localeCompare(b.name); });
        return rows;
    }
    readonly property int matchCount: {
        var n = 0;
        for (var i = 0; i < root.categoryOrder.length; ++i)
            n += root.presetsFor(root.categoryOrder[i]).length;
        return n;
    }

    function openPopup() {
        if (root.profilesModel)
            root.allPresets = root.profilesModel.availablePresets();
        // Clear the source field, not just the derived query: the TextField
        // survives the popup closing, so resetting only root.query would leave
        // stale text in the box out of sync with the (empty) filter. Assigning
        // text drives query via onTextChanged.
        searchField.text = "";
        popup.open();
    }

    Rectangle {
        id: header
        implicitWidth: libLabel.implicitWidth + chevron.width + 3 * Theme.spacingMd
        implicitHeight: Theme.controlHeight
        radius: Theme.radiusSm
        // Matches the profile dropdown header: darker than the surface card it
        // sits on, so the control reads as distinct from the card background.
        color: Theme.background
        border.color: (header.activeFocus || popup.visible) ? Theme.borderFocus
                                                            : Theme.border
        border.width: 1
        Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

        // Reachable by Tab; Space/Enter/Down open the library dropdown.
        activeFocusOnTab: true
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return
                || event.key === Qt.Key_Enter || event.key === Qt.Key_Down) {
                if (!popup.visible) {
                    popup.keyboardSession = true;
                    root.openPopup();
                }
                event.accepted = true;
            }
        }

        Text {
            id: libLabel
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacingMd
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Library")
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
        }
        DropdownIndicator {
            id: chevron
            anchors.right: parent.right
            anchors.rightMargin: Theme.spacingMd
            anchors.verticalCenter: parent.verticalCenter
            pointingUp: popup.visible
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (popup.visible) {
                    popup.close();
                } else {
                    popup.keyboardSession = false;
                    root.openPopup();
                }
            }
        }
    }

    Popup {
        id: popup
        // Right-align the wider popup with the compact button so it opens
        // leftward and the categorized list has room without widening the button.
        width: 360
        x: header.width - width
        y: header.implicitHeight + 2
        padding: Theme.spacingSm
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        implicitHeight: Math.min(popupCol.implicitHeight + 2 * Theme.spacingSm, 420)
        // Shared dropdown look (Theme.dropdownSurface/Border): darker than the
        // surface cards behind it, plus a focus-coloured border, so the open menu
        // reads as a distinct floating layer instead of blending into the page.
        background: Rectangle {
            color: Theme.dropdownSurface
            radius: Theme.radiusSm
            border.color: Theme.dropdownBorder
            border.width: 1
        }

        // Focus returns to the header only for keyboard sessions, so a mouse
        // open/close never leaves a keyboard focus ring behind.
        property bool keyboardSession: false
        onOpened: searchField.forceActiveFocus()
        onClosed: {
            if (keyboardSession)
                header.forceActiveFocus();
            keyboardSession = false;
        }

        ColumnLayout {
            id: popupCol
            width: parent.width
            spacing: Theme.spacingSm

            ThemedTextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: qsTr("Search presets…")
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
                onTextChanged: root.query = text
                // Down moves into the preset list (the next focusable).
                Keys.onDownPressed: searchField.nextItemInFocusChain(true)
                                              .forceActiveFocus(Qt.TabFocusReason)
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: Theme.surface
                    border.color: searchField.activeFocus ? Theme.borderFocus
                                                          : Theme.border
                    border.width: 1
                }
            }

            Text {
                visible: root.matchCount === 0
                Layout.fillWidth: true
                Layout.leftMargin: Theme.spacingSm
                text: root.allPresets.length === 0
                      ? qsTr("No presets available")
                      : qsTr("No presets match your search")
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
            }

            ScrollView {
                id: scroller
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(grid.implicitHeight, 360)
                visible: root.matchCount > 0
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    id: grid
                    width: scroller.availableWidth
                    spacing: Theme.spacingSm

                    Repeater {
                        model: root.categoryOrder
                        delegate: ColumnLayout {
                            required property string modelData
                            readonly property var rows: root.presetsFor(modelData)
                            Layout.fillWidth: true
                            spacing: Theme.spacingXs
                            visible: rows.length > 0

                            // Section header.
                            Text {
                                Layout.fillWidth: true
                                Layout.leftMargin: Theme.spacingXs
                                text: root.categoryTitle(modelData)
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontBody
                                font.weight: Font.Medium
                                // Preset metadata is data, not markup.
                                textFormat: Text.PlainText
                            }

                            Repeater {
                                model: parent.rows
                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    implicitHeight: Theme.controlHeightLg
                                    radius: Theme.radiusSm
                                    color: presetHover.hovered ? Theme.surfaceHover
                                                               : "transparent"
                                    HoverHandler { id: presetHover }
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: Theme.spacingSm
                                        // Reserve the scrollbar's width while the
                                        // list is scrollable, so the overlay
                                        // scrollbar never sits on top of the add
                                        // (+) button at the row's right edge.
                                        anchors.rightMargin: Theme.spacingXs
                                            + (scroller.ScrollBar.vertical.size < 1
                                               ? scroller.ScrollBar.vertical.width : 0)
                                        spacing: Theme.spacingSm

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 0
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.name
                                                color: Theme.text
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontBody
                                                textFormat: Text.PlainText
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                visible: modelData.description.length > 0
                                                text: modelData.description
                                                color: Theme.textMuted
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontBody
                                                textFormat: Text.PlainText
                                                elide: Text.ElideRight
                                            }
                                        }

                                        Text {
                                            text: modelData.count + ""
                                            color: Theme.textMuted
                                            font.family: Theme.fontFamilyMono
                                            font.pixelSize: Theme.fontBody
                                            Layout.rightMargin: Theme.spacingXs
                                        }

                                        Rectangle {
                                            id: addPresetBtn
                                            // Reachable by Tab; Space/Enter add
                                            // this preset, Up/Down step through
                                            // the list (the focusables in order).
                                            activeFocusOnTab: true
                                            implicitHeight: Theme.controlHeightSm
                                            implicitWidth: implicitHeight
                                            radius: Theme.radiusSm
                                            color: (addMouse.containsMouse
                                                    || addPresetBtn.activeFocus)
                                                   ? Theme.accentHover : Theme.accent
                                            Behavior on color {
                                                ColorAnimation { duration: Theme.animShort }
                                            }

                                            // Only confirm on success; a failed
                                            // add emits errorOccurred (shown as an
                                            // error snackbar) and returns false, so
                                            // a green "Added" toast would contradict
                                            // it.
                                            function addPreset() {
                                                if (root.profilesModel
                                                    && root.profilesModel.addProfileFromPreset(
                                                        modelData.file))
                                                    root.requestSnackbar(
                                                        qsTr("Added “%1”").arg(modelData.name),
                                                        Theme.success);
                                                popup.close();
                                            }

                                            Keys.onPressed: (event) => {
                                                if (event.key === Qt.Key_Space
                                                    || event.key === Qt.Key_Return
                                                    || event.key === Qt.Key_Enter) {
                                                    addPresetBtn.addPreset();
                                                    event.accepted = true;
                                                } else if (event.key === Qt.Key_Down) {
                                                    addPresetBtn.nextItemInFocusChain(true)
                                                        .forceActiveFocus(Qt.TabFocusReason);
                                                    event.accepted = true;
                                                } else if (event.key === Qt.Key_Up) {
                                                    addPresetBtn.nextItemInFocusChain(false)
                                                        .forceActiveFocus(Qt.BacktabFocusReason);
                                                    event.accepted = true;
                                                }
                                            }

                                            Text {
                                                id: addLabel
                                                anchors.centerIn: parent
                                                text: Theme.iconAdd
                                                color: Theme.accentText
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontStrong
                                                font.weight: Font.Medium
                                            }
                                            MouseArea {
                                                id: addMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: addPresetBtn.addPreset()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
