import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Item {
    id: root

    property var mappingsModel: null
    property var settingsModel: null
    signal requestSnackbar(string message, color c)
    signal requestUndoSnackbar(string message, var callback)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

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
