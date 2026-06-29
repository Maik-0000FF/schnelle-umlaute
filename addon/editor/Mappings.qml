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

        // Edit-target selector: which profile's mappings are shown/edited here.
        // Independent of the active profile: editing the active one applies
        // live on save; editing another only writes that profile's file.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd
            visible: root.profilesModel && root.profilesModel.count > 1

            Text {
                text: qsTr("Editing profile")
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 13
                Layout.preferredWidth: 120
            }

            ThemedComboBox {
                id: editTargetBox
                Layout.fillWidth: true
                model: {
                    if (!root.profilesModel) return [];
                    root.profilesModel.revision; // re-eval on changes
                    return root.profilesModel.profileNames();
                }
                onActivated: {
                    if (root.profilesModel && root.mappingsModel)
                        root.mappingsModel.profileFile =
                            root.profilesModel.fileForRow(currentIndex);
                }
            }

            // ComboBox assigns currentIndex imperatively on activation, which
            // would break a plain declarative binding; a Binding element keeps
            // it tracking the real edit target across rename/delete/switch.
            Binding {
                target: editTargetBox
                property: "currentIndex"
                value: {
                    if (!root.profilesModel || !root.mappingsModel) return 0;
                    root.profilesModel.revision;
                    for (var i = 0; i < root.profilesModel.count; ++i) {
                        if (root.profilesModel.fileForRow(i)
                                === root.mappingsModel.profileFile)
                            return i;
                    }
                    return 0;
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.profilesModel && root.profilesModel.count > 1
            wrapMode: Text.WordWrap
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: 12
            text: {
                if (!root.profilesModel || !root.mappingsModel) return "";
                root.profilesModel.revision; // refresh on active/profile change
                return root.profilesModel.fileForRow(
                           root.profilesModel.activeRow())
                           === root.mappingsModel.profileFile
                    ? qsTr("This is the active profile: changes apply while typing as soon as you save.")
                    : qsTr("This is not the active profile: changes are saved but only take effect once you switch to it.");
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

    function focusAdd() { addCard.focusInput(); }
}
