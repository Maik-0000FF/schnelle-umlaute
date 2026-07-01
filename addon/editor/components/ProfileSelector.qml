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
        border.color: popup.visible ? Theme.borderFocus : Theme.border
        border.width: 1
        Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

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
            onClicked: popup.visible ? popup.close() : popup.open()
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
        // Cap the whole popup so a long profile list still fits small editor
        // windows; the inner list gets its own (smaller) cap below so the
        // add-row and separator always stay visible above it.
        implicitHeight: Math.min(popupCol.implicitHeight + 2 * Theme.spacingSm,
                                 360)
        // Darker than the surface cards behind it, plus a focus-coloured border,
        // so the open menu reads as a distinct floating layer instead of blending
        // into the page (the surrounding cards are Theme.surface).
        background: Rectangle {
            color: Theme.background
            radius: Theme.radiusSm
            border.color: Theme.borderFocus
            border.width: 1
        }

        ColumnLayout {
            id: popupCol
            width: parent.width
            spacing: Theme.spacingSm

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
                }

                Rectangle {
                    id: addBtn
                    readonly property bool ready:
                        root.profilesModel && newName.text.length > 0
                        && root.profilesModel.nameErrorFor(newName.text, -1) === ""
                    implicitHeight: Theme.controlHeightSm
                    implicitWidth: addLabel.implicitWidth + 2 * Theme.spacingMd
                    radius: Theme.radiusSm
                    opacity: ready ? 1.0 : 0.4
                    color: addMouse.containsMouse && ready ? Theme.accentHover
                                                           : Theme.accent
                    Behavior on color { ColorAnimation { duration: Theme.animShort } }
                    Text {
                        id: addLabel
                        anchors.centerIn: parent
                        text: qsTr("Add")
                        color: Theme.onAccent
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
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
                font.pixelSize: Theme.fontCaption
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
                spacing: 2
                model: root.profilesModel
                boundsBehavior: Flickable.StopAtBounds

                delegate: Rectangle {
                    id: prow
                    required property int index
                    required property string name
                    required property bool isActive
                    required property bool isProtected
                    required property bool favorite
                    required property string selectKey
                    width: ListView.view.width
                    height: Theme.rowHeight
                    radius: Theme.radiusSm
                    color: prowHover.hovered ? Theme.surfaceHover : "transparent"
                    property bool renaming: false

                    HoverHandler { id: prowHover }

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
                                visible: activeMouse.containsMouse
                                text: prow.isActive ? qsTr("Active profile")
                                                    : qsTr("Set as active")
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
                                visible: favMouse.containsMouse
                                text: prow.favorite
                                      ? qsTr("Favorite (in cycle)")
                                      : qsTr("Add to cycle favorites")
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

                        // Name (tap selects edit target) or inline rename field.
                        Text {
                            visible: !prow.renaming
                            Layout.fillWidth: true
                            text: prow.name
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
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
                                prow.renaming = false;
                            }
                            Keys.onEscapePressed: {
                                text = prow.name;
                                prow.renaming = false;
                            }
                            onActiveFocusChanged: {
                                if (!activeFocus && prow.renaming) {
                                    text = prow.name; // cancel on focus loss
                                    prow.renaming = false;
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
                                visible: parent.hovered
                                text: qsTr("Rename profile")
                            }
                            onClicked: prow.renaming = true
                        }

                        // Delete (trash). For protected profiles (Standard /
                        // last) it is disabled but keeps its slot, so the
                        // action columns stay aligned across all rows.
                        ToolButton {
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
                                visible: parent.hovered
                                text: qsTr("Delete profile")
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
