import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Item {
    id: root

    property var mappingsModel: null
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

                delegate: MappingRow {
                    required property int index
                    required property string input
                    required property string output
                    width: listView.width
                    rowIndex: index
                    inputText: input
                    outputText: output
                    modelRef: root.mappingsModel
                    onRemoveRequested: {
                        const removedInput = inputText;
                        const removedOutput = outputText;
                        root.mappingsModel.removeMapping(rowIndex);
                        root.requestUndoSnackbar(
                            qsTr("“%1” removed").arg(removedInput),
                            () => root.mappingsModel.addMapping(removedInput, removedOutput)
                        );
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }
        }
    }

    function focusAdd() { addCard.focusInput(); }
}
