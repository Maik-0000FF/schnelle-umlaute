import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Item {
    id: root

    property var mappingsModel: null
    property var settingsModel: null
    property var profilesModel: null
    property var mergeModel: null
    signal requestSnackbar(string message, color c)
    signal requestUndoSnackbar(string message, var callback)

    // Shared label column width for the cycle-shortcut rows.
    readonly property int cycleLabelWidth: 100

    // Surface model-side validation errors (e.g. a duplicate shortcut) as a
    // snackbar. A null target is a harmless no-op until profilesModel is set.
    Connections {
        target: root.profilesModel
        function onErrorOccurred(message) {
            root.requestSnackbar(message, Theme.error);
        }
    }

    // Same for the mappings model, e.g. a cross-row chip move refused because
    // the target mapping already has that variant.
    Connections {
        target: root.mappingsModel
        function onErrorOccurred(message) {
            root.requestSnackbar(message, Theme.error);
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        // Profile section: one dropdown to pick the edit target, create new
        // (empty) profiles, set the active one (star), rename and delete.
        SettingsCard {
            titleText: qsTr("Profile")

            // Profile dropdown fills the row; the library is a compact button to
            // its right, opening its own (wider) standalone menu so neither popup
            // crowds the other.
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingMd

                ProfileSelector {
                    Layout.fillWidth: true
                    profilesModel: root.profilesModel
                    mappingsModel: root.mappingsModel
                    mergeModel: root.mergeModel
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

                LibrarySelector {
                    Layout.alignment: Qt.AlignTop
                    profilesModel: root.profilesModel
                    onRequestSnackbar: (msg, c) => root.requestSnackbar(msg, c)
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontBody
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

            // Global cycle shortcuts: step through the favorites (★), or all
            // profiles if none are favorited.
            CycleShortcutRow {
                Layout.topMargin: Theme.spacingXs
                labelText: qsTr("Cycle next")
                labelWidth: root.cycleLabelWidth
                shortcut: root.profilesModel ? root.profilesModel.cycleNext : ""
                description: qsTr("to the next favorite profile (or any, if none)")
                onCaptured: (combo) => {
                    if (root.profilesModel)
                        root.profilesModel.cycleNext = combo;
                }
            }
            CycleShortcutRow {
                labelText: qsTr("Cycle previous")
                labelWidth: root.cycleLabelWidth
                shortcut: root.profilesModel ? root.profilesModel.cyclePrev : ""
                description: qsTr("to the previous favorite profile")
                onCaptured: (combo) => {
                    if (root.profilesModel)
                        root.profilesModel.cyclePrev = combo;
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

            // Switch to mouse mode only on genuine pointer movement. This
            // handler sits on the non-scrolling card, so its point.position is
            // stable while the list scrolls under a still cursor (keyboard
            // paging or Alt-reorder); only real mouse movement changes it, so
            // keyboard navigation keeps its highlight instead of jumping to
            // whatever row slid under the pointer.
            HoverHandler {
                property point lastPos: Qt.point(-1, -1)
                onPointChanged: {
                    // Ignore the first point acquisition (sentinel -> real
                    // position): a keyboard-opened list with the cursor already
                    // resting over it must not be knocked into mouse mode. Only
                    // genuine follow-up movement flips.
                    if (lastPos.x >= 0
                        && (point.position.x !== lastPos.x
                            || point.position.y !== lastPos.y))
                        listView.keyboardActive = false;
                    lastPos = point.position;
                }
            }

            EmptyState {
                anchors.centerIn: parent
                visible: root.mappingsModel && root.mappingsModel.count === 0
            }

            ListView {
                id: listView
                // Set by a chip while it is being dragged, so every mapping row
                // can clear its drop-target highlight the moment the drag ends
                // (see MappingRow.dropTarget).
                property bool chipDragging: false
                anchors.fill: parent
                anchors.margins: Theme.spacingSm
                clip: true
                spacing: Theme.spacingXxs
                visible: root.mappingsModel && root.mappingsModel.count > 0
                model: root.mappingsModel
                boundsBehavior: Flickable.StopAtBounds

                // Reachable by Tab; Up/Down move the current row (shown with a
                // ring), Alt+Up/Down reorder it, Enter/F2 edit it, Delete
                // removes it. While a row is being edited the fields handle keys.
                activeFocusOnTab: true
                keyNavigationEnabled: true
                // Which input last drove the selection, so rows show exactly one
                // highlight (see MappingRow): true after any key press here,
                // flipped back to false by a row hover/click. Arrow keys are not
                // accepted below, so navigation still works while this flips.
                property bool keyboardActive: false
                Keys.onPressed: (event) => {
                    listView.keyboardActive = true;
                    if (listView.editingIndex !== -1 || !root.mappingsModel)
                        return;
                    const i = listView.currentIndex;
                    if (i < 0)
                        return;
                    if ((event.modifiers & Qt.AltModifier)
                        && event.key === Qt.Key_Up) {
                        if (i > 0) {
                            root.mappingsModel.moveMapping(i, i - 1);
                            listView.currentIndex = i - 1;
                        }
                        event.accepted = true;
                    } else if ((event.modifiers & Qt.AltModifier)
                               && event.key === Qt.Key_Down) {
                        if (i < root.mappingsModel.count - 1) {
                            root.mappingsModel.moveMapping(i, i + 1);
                            listView.currentIndex = i + 1;
                        }
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Return
                               || event.key === Qt.Key_Enter
                               || event.key === Qt.Key_F2) {
                        listView.editingIndex = i;
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Delete) {
                        if (listView.currentItem)
                            listView.currentItem.removeRequested();
                        event.accepted = true;
                    }
                }

                moveDisplaced: Transition {
                    NumberAnimation { properties: "y"; duration: 180; easing.type: Easing.OutCubic }
                }
                move: Transition {
                    NumberAnimation { properties: "y"; duration: 180; easing.type: Easing.OutCubic }
                }

                property int editingIndex: -1

                // Switching the edit target reloads the model in place, but the
                // row-index-keyed editingIndex would otherwise survive and reopen
                // an unconfirmed edit at the same position in the new profile.
                // Discard any open edit whenever the profile changes.
                Connections {
                    target: root.mappingsModel
                    function onProfileFileChanged() { listView.editingIndex = -1; }
                }

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

    // Click anywhere empty to drop keyboard focus, disarming an armed cycle or
    // per-profile select-key capture field. Topmost among the page content so
    // it sees every press first, but passes them through so the list drag and
    // controls keep working. (The dialogs below are modal popups on their own
    // layer, unaffected by this.)
    FocusSink {}

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
