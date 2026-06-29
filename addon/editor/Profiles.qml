import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Item {
    id: root

    property var profilesModel: null
    signal requestSnackbar(string message, color c)

    function doCreate() {
        if (!root.profilesModel)
            return;
        if (root.profilesModel.createProfile(newName.text)) {
            newName.text = "";
            root.requestSnackbar(qsTr("Profile created"), Theme.success);
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // New-profile bar.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

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
                onAccepted: root.doCreate()
            }

            Rectangle {
                id: addBtn
                readonly property bool ready:
                    root.profilesModel && newName.text.length > 0
                    && root.profilesModel.nameErrorFor(newName.text, -1) === ""
                implicitHeight: 34
                implicitWidth: addLabel.implicitWidth + 2 * Theme.spacingMd
                radius: Theme.radiusSm
                opacity: ready ? 1.0 : 0.4
                color: addMouse.containsMouse && ready ? Theme.accentHover
                                                       : Theme.accent
                Behavior on color { ColorAnimation { duration: Theme.animShort } }
                Text {
                    id: addLabel
                    anchors.centerIn: parent
                    text: qsTr("Add profile")
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
                    onClicked: if (addBtn.ready) root.doCreate()
                }
            }
        }

        // Inline name-error hint, mirrors the mapping add-card.
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

        // Profile list.
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface
            radius: Theme.radiusLg
            border.color: Theme.border
            border.width: 1

            ListView {
                id: listView
                anchors.fill: parent
                anchors.margins: Theme.spacingSm
                clip: true
                spacing: 2
                model: root.profilesModel
                boundsBehavior: Flickable.StopAtBounds

                delegate: Rectangle {
                    id: row
                    required property int index
                    required property string name
                    required property bool isActive
                    required property bool isProtected
                    width: ListView.view.width
                    height: 48
                    radius: Theme.radiusSm
                    color: rowHover.hovered ? Theme.surfaceHover : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.animShort } }

                    HoverHandler { id: rowHover }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacingMd
                        anchors.rightMargin: Theme.spacingSm
                        spacing: Theme.spacingMd

                        // Active marker dot.
                        Rectangle {
                            implicitWidth: 8
                            implicitHeight: 8
                            radius: 4
                            color: row.isActive ? Theme.accent : "transparent"
                        }

                        ThemedTextField {
                            id: nameField
                            Layout.fillWidth: true
                            text: row.name
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            background: Rectangle {
                                radius: Theme.radiusSm
                                color: nameField.activeFocus ? Theme.background
                                                             : "transparent"
                                border.color: nameField.activeFocus
                                              ? Theme.borderFocus : "transparent"
                                border.width: 1
                            }
                            onEditingFinished: {
                                if (!root.profilesModel || text === row.name)
                                    return;
                                if (!root.profilesModel.renameProfile(row.index,
                                                                      text))
                                    text = row.name; // revert on failure
                            }
                        }

                        // Active badge / set-active action.
                        Text {
                            visible: row.isActive
                            text: qsTr("Active")
                            color: Theme.accent
                            font.family: Theme.fontFamily
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }
                        Rectangle {
                            visible: !row.isActive
                            implicitHeight: 28
                            implicitWidth: setActiveLabel.implicitWidth
                                           + 2 * Theme.spacingSm
                            radius: Theme.radiusSm
                            color: setActiveMouse.containsMouse
                                   ? Theme.surfaceHover : Theme.background
                            border.color: Theme.border
                            border.width: 1
                            Behavior on color { ColorAnimation { duration: Theme.animShort } }
                            Text {
                                id: setActiveLabel
                                anchors.centerIn: parent
                                text: qsTr("Set active")
                                color: Theme.text
                                font.family: Theme.fontFamily
                                font.pixelSize: 12
                            }
                            MouseArea {
                                id: setActiveMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.profilesModel.setActiveRow(row.index))
                                        root.requestSnackbar(
                                            qsTr("Switched to “%1”").arg(row.name),
                                            Theme.accent);
                                }
                            }
                        }

                        // Delete (hidden for protected profiles: Standard / last).
                        ToolButton {
                            visible: !row.isProtected
                            text: "🗑"
                            contentItem: Text {
                                text: parent.text
                                color: parent.hovered ? Theme.error
                                                      : Theme.textMuted
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
                                confirmDelete.messageText = qsTr(
                                    "Delete the profile “%1”? Its mappings file is removed."
                                ).arg(row.name);
                                confirmDelete.onConfirmed = () => {
                                    if (root.profilesModel.removeProfile(row.index))
                                        root.requestSnackbar(
                                            qsTr("Profile deleted"),
                                            Theme.textMuted);
                                };
                                confirmDelete.open();
                            }
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }
        }
    }

    ConfirmDialog {
        id: confirmDelete
        titleText: qsTr("Delete profile")
        confirmText: qsTr("Delete")
    }
}
