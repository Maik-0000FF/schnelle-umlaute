import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

// Profile management dropdown for the Mappings page. Collapsed it shows the
// current edit target; expanded it offers an add-new field at the top and one
// row per profile with set-active (star), rename (pencil, inline) and delete
// (trash). Tapping a row's name selects it as the edit target. Action icons
// keep the popup open; selecting an edit target or adding closes it.
Item {
    id: root

    property var profilesModel: null
    property var mappingsModel: null
    // Sole owner of the merge manifest (merge.conf); may be null in contexts
    // that don't wire it, which hides the per-row merge control.
    property var mergeModel: null
    signal requestSnackbar(string message, color c)
    // Delete is confirmed by the parent (its ConfirmDialog), so a modal does
    // not have to stack over this popup.
    signal requestDelete(int index, string name)

    implicitHeight: header.implicitHeight

    // Display name of the current edit target (for the collapsed header).
    readonly property string editTargetName: {
        if (!profilesModel || !mappingsModel)
            return "";
        profilesModel.revision; // re-eval on add/rename/delete
        var names = profilesModel.profileNames();
        for (var i = 0; i < profilesModel.count; ++i) {
            if (profilesModel.fileForRow(i) === mappingsModel.profileFile)
                return names[i];
        }
        return names.length ? names[0] : "";
    }

    function addProfile() {
        if (!profilesModel)
            return;
        if (profilesModel.createProfile(newName.text)) {
            var f = profilesModel.fileForRow(profilesModel.count - 1);
            newName.text = "";
            if (mappingsModel)
                mappingsModel.profileFile = f; // land on the new empty profile
            root.requestSnackbar(qsTr("Profile created"), Theme.success);
            popup.close();
        }
    }

    Rectangle {
        id: header
        width: parent.width
        implicitHeight: Theme.controlHeight
        radius: Theme.radiusSm
        color: Theme.background
        border.color: (header.activeFocus || popup.visible) ? Theme.borderFocus
                                                            : Theme.border
        border.width: 1
        Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

        // Reachable by Tab; Space/Enter/Down open the dropdown.
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

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.spacingMd
            anchors.right: chevron.left
            anchors.rightMargin: Theme.spacingSm
            anchors.verticalCenter: parent.verticalCenter
            text: root.editTargetName
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontBody
            // A profile name is user input, shown literally.
            textFormat: Text.PlainText
            elide: Text.ElideRight
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
                    popup.open();
                }
            }
        }
    }

    Popup {
        id: popup
        y: header.implicitHeight + 2
        width: header.width
        padding: Theme.spacingSm
        // Close on a press outside the header (the popup's parent), not on the
        // header itself, so re-clicking the header toggles cleanly instead of
        // closing-then-reopening on the same click. A click anywhere else still
        // dismisses it.
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        // True only when the popup was opened via the keyboard, so the focus
        // ring appears for keyboard use but not when driving with the mouse.
        property bool keyboardSession: false
        // On open, land the cursor on the current edit target and (for keyboard
        // sessions) move focus into the list; on close, hand focus back to the
        // header only if it was a keyboard session.
        onOpened: {
            if (root.profilesModel && root.mappingsModel) {
                let target = 0;
                for (let i = 0; i < root.profilesModel.count; i++)
                    if (root.profilesModel.fileForRow(i)
                        === root.mappingsModel.profileFile) {
                        target = i;
                        break;
                    }
                list.currentIndex = target;
            }
            if (keyboardSession)
                list.forceActiveFocus();
            // Start in the mode the popup was opened with, so the single row
            // highlight follows keyboard vs mouse from the first frame.
            list.keyboardActive = keyboardSession;
        }
        onClosed: {
            if (keyboardSession)
                header.forceActiveFocus();
            keyboardSession = false;
            // Clear any open rename so reopening the popup starts clean (the
            // state now lives on the persistent list, not a recycled delegate).
            list.renamingIndex = -1;
            // Re-arm "ignore first acquisition": lastPos lives on the persistent
            // popup content, so reset it here, otherwise a later keyboard reopen
            // with the cursor now at a new spot would flip to mouse mode.
            modeHover.lastPos = Qt.point(-1, -1);
        }
        // Cap the whole popup so a long profile list still fits small editor
        // windows; the inner list gets its own (smaller) cap below so the
        // add-row and separator always stay visible above it.
        implicitHeight: Math.min(popupCol.implicitHeight + 2 * Theme.spacingSm,
                                 360)
        // Shared dropdown look (Theme.dropdownSurface/Border): darker than the
        // surface cards behind it, plus a focus-coloured border, so the open menu
        // reads as a distinct floating layer instead of blending into the page.
        background: Rectangle {
            color: Theme.dropdownSurface
            radius: Theme.radiusSm
            border.color: Theme.dropdownBorder
            border.width: 1
        }

        ColumnLayout {
            id: popupCol
            width: parent.width
            spacing: Theme.spacingSm

            // Switch to mouse mode only on genuine pointer movement. Sits on the
            // non-scrolling popup content, so its point.position stays stable
            // while the profile list scrolls under a still cursor; only real
            // mouse movement flips keyboardActive.
            HoverHandler {
                id: modeHover
                property point lastPos: Qt.point(-1, -1)
                onPointChanged: {
                    // Ignore the first point acquisition (sentinel -> real
                    // position) so opening the popup by keyboard with the cursor
                    // already over it stays in keyboard mode. Only genuine
                    // follow-up movement flips.
                    if (lastPos.x >= 0
                        && (point.position.x !== lastPos.x
                            || point.position.y !== lastPos.y))
                        list.keyboardActive = false;
                    lastPos = point.position;
                }
            }

            // Add-new row.
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingSm

                ThemedTextField {
                    id: newName
                    Layout.fillWidth: true
                    placeholderText: qsTr("New profile name")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
                    background: Rectangle {
                        radius: Theme.radiusSm
                        color: Theme.background
                        border.color: newName.activeFocus ? Theme.borderFocus
                                                          : Theme.border
                        border.width: 1
                    }
                    onAccepted: root.addProfile()
                    // Escape closes the dropdown (matching its CloseOnEscape),
                    // instead of the window-level Esc shortcut closing the whole
                    // editor.
                    Keys.onEscapePressed: popup.close()
                    Keys.onShortcutOverride: (event) => {
                        if (event.key === Qt.Key_Escape)
                            event.accepted = true;
                    }
                }

                Rectangle {
                    id: addBtn
                    readonly property bool ready:
                        root.profilesModel && newName.text.length > 0
                        && root.profilesModel.nameErrorFor(newName.text, -1) === ""
                    implicitHeight: Theme.controlHeightSm
                    implicitWidth: implicitHeight
                    radius: Theme.radiusSm
                    color: ready
                        ? (addMouse.containsMouse ? Theme.accentHover : Theme.accent)
                        : Theme.surfaceHover
                    Behavior on color { ColorAnimation { duration: Theme.animShort } }
                    Text {
                        id: addLabel
                        anchors.centerIn: parent
                        text: Theme.iconAdd
                        color: addBtn.ready ? Theme.accentText : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontStrong
                        font.weight: Font.Medium
                    }
                    MouseArea {
                        id: addMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: addBtn.ready ? Qt.PointingHandCursor
                                                  : Qt.ArrowCursor
                        onClicked: if (addBtn.ready) root.addProfile()
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: root.profilesModel && newName.text.length > 0
                         && text.length > 0
                text: root.profilesModel
                      ? root.profilesModel.nameErrorFor(newName.text, -1) : ""
                color: Theme.error
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
                // The message quotes the typed name back.
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.border
            }

            ListView {
                id: list
                Layout.fillWidth: true
                // ~6 rows (36 px each) before it scrolls, kept under the
                // popup cap so the add-row above never gets pushed off.
                Layout.preferredHeight: Math.min(contentHeight, 240)
                clip: true
                spacing: Theme.spacingXxs
                model: root.profilesModel
                boundsBehavior: Flickable.StopAtBounds

                // Single list-wide "which row is being renamed" (-1 = none), so
                // one place owns the rename state: a row's inline field shows
                // when it matches, and a background click anywhere is suppressed
                // while any rename is open (not just this row's).
                property int renamingIndex: -1

                // Removing a row can shift indices out from under an open inline
                // rename, leaving renamingIndex pointing at the wrong row, so
                // drop the rename on a removal (Mappings does the same for its
                // edit via onProfileFileChanged). Insertions append at the end
                // and close the popup, so they need no handler.
                Connections {
                    target: root.profilesModel
                    function onRowsRemoved() { list.renamingIndex = -1; }
                }

                // Up/Down move the current row (keyNavigationEnabled); the keys
                // below act on it: Enter selects the edit target, F2 renames,
                // Delete removes, A sets active, F toggles favorite.
                keyNavigationEnabled: true
                // Active input mode for the single-highlight rule (mirrors the
                // mapping list): a key press means keyboard, a row hover/click
                // means mouse.
                property bool keyboardActive: false
                Keys.onPressed: (event) => {
                    list.keyboardActive = true;
                    const it = list.currentItem;
                    if (!it)
                        return;
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        if (root.profilesModel && root.mappingsModel) {
                            root.mappingsModel.profileFile =
                                root.profilesModel.fileForRow(it.index);
                            popup.close();
                        }
                        event.accepted = true;
                    } else if (event.key === Qt.Key_F2) {
                        list.renamingIndex = it.index;
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Delete) {
                        if (!it.isProtected) {
                            popup.close();
                            root.requestDelete(it.index, it.name);
                        }
                        event.accepted = true;
                    } else if (event.text.toLowerCase() === "a") {
                        if (root.profilesModel && !it.isActive
                            && root.profilesModel.setActiveRow(it.index))
                            root.requestSnackbar(
                                qsTr("Switched to “%1”").arg(it.name), Theme.accent);
                        event.accepted = true;
                    } else if (event.text.toLowerCase() === "f") {
                        if (root.profilesModel)
                            root.profilesModel.setFavorite(it.index, !it.favorite);
                        event.accepted = true;
                    }
                }

                delegate: Rectangle {
                    id: prow
                    required property int index
                    required property string name
                    required property bool isActive
                    required property bool isProtected
                    required property bool favorite
                    required property string selectKey
                    required property string file
                    // Position of this profile in the merge (1 = base, 2..N =
                    // appended, -1 = not merged). The mergeBase comparison is not
                    // a no-op: its VALUE feeds the condition, which keeps the
                    // dependency on manifestChanged from being optimized away by
                    // compiled QML (a bare read would be), so the badge
                    // re-evaluates and renumbers whenever the merge changes.
                    readonly property int mergeOrderIdx:
                        (root.mergeModel
                         && root.mergeModel.mergeBase !== undefined)
                            ? root.mergeModel.orderIndex(prow.file) : -1
                    width: ListView.view.width
                    height: Theme.rowHeight
                    radius: Theme.radiusSm
                    // Exactly one highlight at a time, following the list's
                    // active input mode: the keyboard-current row while
                    // navigating by keyboard, otherwise the mouse-hovered row.
                    // Both use surfaceHover so switching input never shows two.
                    color: (list.keyboardActive
                            ? (prow.ListView.isCurrentItem && list.activeFocus)
                            : prowHover.hovered)
                           ? Theme.surfaceHover : "transparent"
                    readonly property bool renaming: list.renamingIndex === prow.index

                    // Only reports which row the pointer is over; the mouse-mode
                    // switch is driven by genuine movement on the non-scrolling
                    // popup content (see popupCol), not by hover changes that
                    // also fire when rows scroll under a still cursor.
                    HoverHandler { id: prowHover }

                    // Clicking the row (outside the action icons) makes it the
                    // current row and focuses the list, so keyboard actions
                    // (Enter/F2/Delete/A/F) continue from where the mouse landed.
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        onClicked: {
                            // Don't steal focus from an open inline rename (this
                            // row or another) on a click on bare row padding: it
                            // cancels on focus loss, so this would silently abort
                            // the rename.
                            if (list.renamingIndex !== -1)
                                return;
                            // A click is mouse input: show the hover look here,
                            // not the keyboard highlight.
                            list.keyboardActive = false;
                            list.currentIndex = prow.index;
                            list.forceActiveFocus();
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingSm
                        // Reserve the scrollbar's width on the right while the
                        // list is scrollable, so the overlay scrollbar never
                        // sits on top of the trash (delete) button at the row's
                        // right edge.
                        anchors.rightMargin: Theme.spacingXs
                            + (list.contentHeight > list.height
                               ? listScrollBar.width : 0)
                        spacing: Theme.spacingXs

                        // Active marker / toggle (checkmark). The star next to
                        // it is the separate "favorite for cycling" toggle, so
                        // active and favorite stay visually distinct.
                        Text {
                            text: Theme.iconCheck
                            // Inactive uses the shared muted base of every
                            // unselected action icon (star, pencil, trash), not
                            // the darker border, so the icon row reads uniform;
                            // brightens to full text on hover, accent when active.
                            color: prow.isActive
                                   ? Theme.accent
                                   : (activeMouse.containsMouse ? Theme.text
                                                                : Theme.textMuted)
                            font.pixelSize: Theme.fontIcon
                            Layout.preferredWidth: 20
                            horizontalAlignment: Text.AlignHCenter
                            ThemedToolTip {
                                hovered: activeMouse.containsMouse
                                text: prow.isActive ? qsTr("Active profile")
                                                    : qsTr("Set as active (A)")
                            }
                            MouseArea {
                                id: activeMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.profilesModel && !prow.isActive
                                        && root.profilesModel.setActiveRow(prow.index))
                                        root.requestSnackbar(
                                            qsTr("Switched to “%1”").arg(prow.name),
                                            Theme.accent);
                                }
                            }
                        }

                        // Favorite toggle (★). When any profile is a favorite,
                        // the cycle shortcut steps through favorites only.
                        Text {
                            // Always the filled glyph: the hollow outline read
                            // as a barely-there speck in the inactive state.
                            // Inactive instead reads as a muted (lighter) fill,
                            // brightening to full text on hover; active is accent.
                            text: Theme.iconStar
                            color: prow.favorite
                                   ? Theme.accent
                                   : (favMouse.containsMouse ? Theme.text
                                                             : Theme.textMuted)
                            font.pixelSize: Theme.fontIcon
                            Layout.preferredWidth: 20
                            horizontalAlignment: Text.AlignHCenter
                            ThemedToolTip {
                                hovered: favMouse.containsMouse
                                text: prow.favorite
                                      ? qsTr("Favorite (in cycle)")
                                      : qsTr("Add to cycle favorites (F)")
                            }
                            MouseArea {
                                id: favMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: if (root.profilesModel)
                                    root.profilesModel.setFavorite(
                                        prow.index, !prow.favorite);
                            }
                        }

                        // Merge control (⧉): click to add a profile to the merge
                        // (appended in click order); the profile at position 1
                        // is the base. Clicking a merged profile removes it, and
                        // removing position 1 promotes the next. The numbered
                        // badge shows the position in the same provenance colour
                        // the composed view uses.
                        Text {
                            id: mergeIcon
                            visible: root.mergeModel !== null
                            text: Theme.iconMerge
                            color: prow.mergeOrderIdx > 0
                                   ? Theme.mergeSourceColor(prow.mergeOrderIdx)
                                   : (mergeMouse.containsMouse ? Theme.text
                                                               : Theme.textMuted)
                            font.pixelSize: Theme.fontIcon
                            Layout.preferredWidth: 20
                            horizontalAlignment: Text.AlignHCenter
                            ThemedToolTip {
                                hovered: mergeMouse.containsMouse
                                text: prow.mergeOrderIdx > 0
                                    ? qsTr("Merged (position %1), click to remove")
                                      .arg(prow.mergeOrderIdx)
                                    : qsTr("Add to the merge")
                            }
                            Rectangle {
                                visible: prow.mergeOrderIdx > 0
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.rightMargin: -Theme.badgeOffset
                                anchors.topMargin: -Theme.badgeOffset
                                width: Math.max(Theme.badgeSize,
                                                badgeText.implicitWidth + Theme.spacingXs)
                                height: Theme.badgeSize
                                radius: height / 2
                                color: Theme.mergeSourceColor(prow.mergeOrderIdx)
                                Text {
                                    id: badgeText
                                    anchors.centerIn: parent
                                    text: prow.mergeOrderIdx
                                    color: Theme.accentText
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontBadge
                                    font.weight: Font.Medium
                                }
                            }
                            MouseArea {
                                id: mergeMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: if (root.mergeModel)
                                    root.mergeModel.toggleMerge(prow.file);
                            }
                        }

                        // Name (tap selects edit target) or inline rename field.
                        Text {
                            visible: !prow.renaming
                            Layout.fillWidth: true
                            // Fill the row height so the select click (below)
                            // covers the whole hover bar, not just the text line;
                            // the text itself stays vertically centred.
                            Layout.fillHeight: true
                            text: prow.name
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.profilesModel && root.mappingsModel) {
                                        root.mappingsModel.profileFile =
                                            root.profilesModel.fileForRow(prow.index);
                                        popup.close();
                                    }
                                }
                            }
                        }
                        ThemedTextField {
                            id: renameField
                            visible: prow.renaming
                            Layout.fillWidth: true
                            text: prow.name
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            background: Rectangle {
                                radius: Theme.radiusSm
                                color: Theme.background
                                border.color: Theme.borderFocus
                                border.width: 1
                            }
                            onVisibleChanged: if (visible) { forceActiveFocus(); selectAll(); }
                            // Commit only on explicit Enter. Escape and losing
                            // focus (click away, another row, popup dismiss)
                            // cancel, so a half-typed rename is never applied
                            // against the user's intent.
                            onAccepted: {
                                if (text !== prow.name && root.profilesModel
                                    && !root.profilesModel.renameProfile(prow.index, text))
                                    text = prow.name; // revert on invalid name
                                list.renamingIndex = -1;
                            }
                            Keys.onEscapePressed: {
                                text = prow.name;
                                list.renamingIndex = -1;
                            }
                            // Handle Escape locally (cancel the rename) instead
                            // of the window-level Esc shortcut closing the editor.
                            Keys.onShortcutOverride: (event) => {
                                if (event.key === Qt.Key_Escape)
                                    event.accepted = true;
                            }
                            onActiveFocusChanged: {
                                if (!activeFocus && prow.renaming) {
                                    text = prow.name; // cancel on focus loss
                                    list.renamingIndex = -1;
                                }
                            }
                        }

                        // Per-profile select hotkey (compact capture field).
                        KeyCaptureField {
                            Layout.preferredWidth: Theme.shortcutFieldWidth
                            visible: !prow.renaming
                            value: prow.selectKey
                            onCaptured: (combo) => {
                                if (root.profilesModel)
                                    root.profilesModel.setSelectKey(prow.index,
                                                                    combo);
                            }
                        }

                        // Rename (pencil). Brand green on hover, matching the
                        // edit pencil in MappingRow (Theme.brand is the constant
                        // green; Theme.accent varies per theme).
                        ToolButton {
                            // Mouse affordance; keyboard renames via F2 on the
                            // roving list, so it must not grab focus on click.
                            focusPolicy: Qt.NoFocus
                            text: Theme.iconEdit
                            implicitWidth: 28
                            contentItem: Text {
                                text: parent.text
                                color: parent.hovered ? Theme.brand : Theme.textMuted
                                font.pixelSize: Theme.fontIcon
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { color: "transparent" }
                            ThemedToolTip {
                                hovered: parent.hovered
                                text: qsTr("Rename profile (F2)")
                            }
                            onClicked: list.renamingIndex = prow.index
                        }

                        // Delete (trash). For protected profiles (Standard /
                        // last) it is disabled but keeps its slot, so the
                        // action columns stay aligned across all rows.
                        ToolButton {
                            // Mouse affordance; keyboard deletes via Delete on
                            // the roving list, so it must not grab focus on click.
                            focusPolicy: Qt.NoFocus
                            enabled: !prow.isProtected
                            opacity: prow.isProtected ? 0 : 1
                            text: Theme.iconTrash
                            implicitWidth: 28
                            contentItem: Text {
                                text: parent.text
                                color: parent.hovered ? Theme.error : Theme.textMuted
                                font.pixelSize: Theme.fontIcon
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle { color: "transparent" }
                            ThemedToolTip {
                                hovered: parent.hovered
                                text: qsTr("Delete profile (Del)")
                            }
                            onClicked: {
                                popup.close();
                                root.requestDelete(prow.index, prow.name);
                            }
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar { id: listScrollBar }
            }
        }
    }
}
