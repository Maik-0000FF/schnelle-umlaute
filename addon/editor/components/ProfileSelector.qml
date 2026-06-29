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
            font.pixelSize: 13
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
        // Cap the whole popup so a long profile list still fits small editor
        // windows; the inner list gets its own (smaller) cap below so the
        // add-row and separator always stay visible above it.
        implicitHeight: Math.min(popupCol.implicitHeight + 2 * Theme.spacingSm,
                                 360)
        background: Rectangle {
            color: Theme.surface
            radius: Theme.radiusSm
            border.color: Theme.border
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
                    font.pixelSize: 13
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
                    implicitHeight: 30
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
                        font.pixelSize: 13
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
                font.pixelSize: 12
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
                    height: 36
                    radius: Theme.radiusSm
                    color: prowHover.hovered ? Theme.surfaceHover : "transparent"
                    property bool renaming: false

                    HoverHandler { id: prowHover }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingSm
                        anchors.rightMargin: Theme.spacingXs
                        spacing: Theme.spacingXs

                        // Active marker / toggle (checkmark). The star glyph is
                        // intentionally NOT used here: it is reserved for a
                        // future "favorite for cycling" flag so the shortcut
                        // cycle can step through favorites only.
                        Text {
                            text: "✓"
                            color: prow.isActive
                                   ? Theme.accent
                                   : (activeMouse.containsMouse ? Theme.textMuted
                                                                : Theme.border)
                            font.pixelSize: 15
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
                            text: prow.favorite ? "★" : "☆"
                            color: prow.favorite
                                   ? Theme.accent
                                   : (favMouse.containsMouse ? Theme.textMuted
                                                             : Theme.border)
                            font.pixelSize: 15
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
                            font.pixelSize: 13
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
                            font.pixelSize: 13
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
                            Layout.preferredWidth: 96
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
                            text: "✎"
                            implicitWidth: 28
                            contentItem: Text {
                                text: parent.text
                                color: parent.hovered ? Theme.brand : Theme.textMuted
                                font.pixelSize: 14
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

                        // Delete (trash), hidden for protected (Standard/last).
                        ToolButton {
                            visible: !prow.isProtected
                            text: "🗑"
                            implicitWidth: 28
                            contentItem: Text {
                                text: parent.text
                                color: parent.hovered ? Theme.error : Theme.textMuted
                                font.pixelSize: 14
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

                ScrollBar.vertical: ScrollBar {}
            }
        }
    }
}
