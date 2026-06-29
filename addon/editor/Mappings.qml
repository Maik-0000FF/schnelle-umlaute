import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Item {
    id: root

    property var mappingsModel: null
    property var settingsModel: null
    property var profilesModel: null
    signal requestSnackbar(string message, color c)
    signal requestUndoSnackbar(string message, var callback)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // Profile section: one dropdown to pick the edit target, create new
        // (empty) profiles, set the active one (star), rename and delete.
        SettingsCard {
            titleText: qsTr("Profile")

            ProfileSelector {
                Layout.fillWidth: true
                profilesModel: root.profilesModel
                mappingsModel: root.mappingsModel
                onRequestSnackbar: (msg, c) => root.requestSnackbar(msg, c)
                onRequestDelete: (index, name) => {
                    profileConfirm.messageText = qsTr(
                        "Delete the profile “%1”? Its mappings file is removed."
                    ).arg(name);
                    profileConfirm.onConfirmed = () => {
                        if (root.profilesModel.removeProfile(index))
                            root.requestSnackbar(qsTr("Profile deleted"),
                                                 Theme.textMuted);
                    };
                    profileConfirm.open();
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: 12
                text: {
                    if (!root.profilesModel || !root.mappingsModel) return "";
                    root.profilesModel.revision; // refresh on active/edit change
                    return root.profilesModel.fileForRow(
                               root.profilesModel.activeRow())
                               === root.mappingsModel.profileFile
                        ? qsTr("Editing the active profile: changes apply while typing as soon as you save.")
                        : qsTr("Editing a profile that is not active: changes are saved but take effect once you make it active (checkmark).");
                }
            }
        }

        AddMappingCard {
            id: addCard
            Layout.fillWidth: true
            modelRef: root.mappingsModel
            settingsModel: root.settingsModel
            onMappingAdded: (input, output) => {
                if (root.mappingsModel.addMapping(input, output)) {
                    root.requestSnackbar(qsTr("Mapping added"), Theme.success);
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface
            radius: Theme.radiusLg
            border.color: Theme.border
            border.width: 1

            EmptyState {
                anchors.centerIn: parent
                visible: root.mappingsModel && root.mappingsModel.count === 0
            }

            ListView {
                id: listView
                anchors.fill: parent
                anchors.margins: Theme.spacingSm
                clip: true
                spacing: 2
                visible: root.mappingsModel && root.mappingsModel.count > 0
                model: root.mappingsModel
                boundsBehavior: Flickable.StopAtBounds

                moveDisplaced: Transition {
                    NumberAnimation { properties: "y"; duration: 180; easing.type: Easing.OutCubic }
                }
                move: Transition {
                    NumberAnimation { properties: "y"; duration: 180; easing.type: Easing.OutCubic }
                }

                property int editingIndex: -1

                delegate: MappingRow {
                    required property int index
                    required property string input
                    required property string output
                    width: listView.width
                    rowIndex: index
                    inputText: input
                    outputText: output
                    modelRef: root.mappingsModel
                    settingsModel: root.settingsModel
                    editing: listView.editingIndex === index
                    onEditStartRequested: listView.editingIndex = index
                    onEditEndRequested: listView.editingIndex = -1
                    onRemoveRequested: {
                        confirmDialog.messageText = qsTr(
                            "Delete the mapping “%1” → “%2”?"
                        ).arg(inputText).arg(outputText);
                        confirmDialog.onConfirmed = () => {
                            root.mappingsModel.removeMapping(rowIndex);
                            root.requestSnackbar(
                                qsTr("Mapping deleted"), Theme.textMuted);
                        };
                        confirmDialog.open();
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }
        }
    }

    ConfirmDialog {
        id: confirmDialog
        titleText: qsTr("Delete mapping")
        confirmText: qsTr("Delete")
    }

    ConfirmDialog {
        id: profileConfirm
        titleText: qsTr("Delete profile")
        confirmText: qsTr("Delete")
    }

    function focusAdd() { addCard.focusInput(); }
}
