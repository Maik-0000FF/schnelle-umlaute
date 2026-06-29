import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Rectangle {
    id: root
    radius: Theme.radiusMd
    color: hoverHandler.hovered ? Theme.surfaceHover : "transparent"
    border.color: editing ? Theme.borderFocus : "transparent"
    border.width: 1
    height: col.implicitHeight + 8

    // HoverHandler is a passive grabber: unlike a MouseArea with hoverEnabled,
    // it stays "hovered" while the cursor is over child pointer handlers
    // (drag handle, buttons) — which otherwise stole hover and made the row
    // background flicker.
    HoverHandler { id: hoverHandler }

    Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

    property int rowIndex: -1
    property string inputText: ""
    property string outputText: ""
    property var modelRef: null
    property var settingsModel: null
    property bool editing: false

    signal removeRequested()
    signal editStartRequested()
    signal editEndRequested()

    onEditingChanged: {
        if (editing) {
            inputEdit.text = inputText;
            outputEdit.text = outputText;
            outputEdit.forceActiveFocus();
        }
    }

    // isActiveLeaderKey / inputErrorFor are method calls — QML can't track the
    // state behind them, so bump a tick when leaders change and reference it
    // in the conflict binding to force re-evaluation.
    property int leadersTick: 0
    Connections {
        target: root.settingsModel
        function onLeadersChanged() { root.leadersTick++; }
    }

    readonly property string editInputError:
        modelRef && editing
            ? modelRef.inputErrorFor(inputEdit.text, rowIndex)
            : ""
    readonly property bool editLeaderConflict: {
        leadersTick; // establish dependency
        return editing && inputEdit.text.length > 0 && settingsModel &&
            settingsModel.isActiveLeaderKey(inputEdit.text);
    }
    readonly property bool editOutputInvalid:
        editing && (outputEdit.text.length === 0 ||
                    !modelRef.validateOutput(outputEdit.text))
    readonly property bool editValid:
        editing && inputEdit.text.length > 0 && editInputError === "" &&
        !editOutputInvalid

    // Read-only view uses inputText directly so the row still flags dead
    // mappings when it isn't being edited.
    readonly property bool staticLeaderConflict: {
        leadersTick; // establish dependency
        return !editing && inputText.length > 0 && settingsModel &&
            settingsModel.isActiveLeaderKey(inputText);
    }

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingSm
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

        Item {
            width: 16
            height: 32
            visible: !root.editing

            Text {
                anchors.centerIn: parent
                text: "⠿"
                color: dragArea.containsMouse || dragArea.pressed
                    ? Theme.text : Theme.textMuted
                font.pixelSize: 14
                Behavior on color { ColorAnimation { duration: Theme.animShort } }
            }

            MouseArea {
                id: dragArea
                anchors.fill: parent
                anchors.margins: -4
                hoverEnabled: true
                cursorShape: Qt.SizeVerCursor

                property real pressY: 0
                property int originalIndex: -1

                onPressed: (mouse) => {
                    const view = root.ListView.view;
                    pressY = mapToItem(view, mouse.x, mouse.y).y;
                    originalIndex = root.rowIndex;
                }

                onPositionChanged: (mouse) => {
                    if (!pressed || originalIndex < 0 || !root.modelRef) return;
                    const view = root.ListView.view;
                    const currentY = mapToItem(view, mouse.x, mouse.y).y;
                    const rowPitch = root.height + (view.spacing || 0);
                    const delta = Math.round((currentY - pressY) / rowPitch);
                    const targetIndex = Math.max(0, Math.min(
                        root.modelRef.count - 1,
                        originalIndex + delta
                    ));
                    if (targetIndex !== root.rowIndex) {
                        root.modelRef.moveMapping(root.rowIndex, targetIndex);
                    }
                }

                onReleased: {
                    originalIndex = -1;
                }
            }
        }

        Rectangle {
            width: 44
            height: 32
            radius: Theme.radiusSm
            color: Theme.background
            border.color: root.staticLeaderConflict ? Theme.warning : Theme.border
            border.width: 1
            visible: !root.editing
            Behavior on border.color { ColorAnimation { duration: Theme.animShort } }

            Text {
                anchors.centerIn: parent
                text: root.inputText
                color: Theme.text
                font.family: Theme.fontFamilyMono
                font.pixelSize: 15
            }
        }

        ThemedTextField {
            id: inputEdit
            visible: root.editing
            Layout.preferredWidth: 80
            text: root.inputText
            maximumLength: 4
            font.family: Theme.fontFamilyMono
            font.pixelSize: 15
            horizontalAlignment: TextInput.AlignHCenter
            background: Rectangle {
                radius: Theme.radiusSm
                color: Theme.background
                border.color: root.editInputError !== ""
                    ? Theme.error
                    : (root.editLeaderConflict ? Theme.warning : Theme.accent)
                border.width: 1
                Behavior on border.color { ColorAnimation { duration: Theme.animShort } }
            }
        }

        Text {
            text: "→"
            color: Theme.textMuted
            font.pixelSize: 14
        }

        Text {
            Layout.fillWidth: true
            visible: !root.editing
            text: root.outputText
            color: Theme.text
            font.family: Theme.fontFamilyMono
            font.pixelSize: 15
            elide: Text.ElideRight
        }

        ThemedTextField {
            id: outputEdit
            visible: root.editing
            Layout.fillWidth: true
            text: root.outputText
            font.family: Theme.fontFamilyMono
            font.pixelSize: 15
            background: Rectangle {
                radius: Theme.radiusSm
                color: Theme.background
                border.color: root.editOutputInvalid && text.length > 0
                    ? Theme.error : Theme.accent
                border.width: 1
            }
            onAccepted: if (root.editValid) confirmEdit()
            Keys.onEscapePressed: cancelEdit()
        }

        ToolButton {
            id: applyBtn
            text: root.editing ? Theme.iconCheck : Theme.iconEdit
            enabled: !root.editing || root.editValid
            ThemedToolTip {
                visible: applyBtn.hovered
                text: root.editing ? qsTr("Apply") : qsTr("Edit")
            }
            contentItem: Text {
                text: applyBtn.text
                color: !applyBtn.enabled
                    ? Theme.border
                    : (applyBtn.hovered ? Theme.brand : Theme.textMuted)
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle { color: "transparent" }
            onClicked: {
                if (root.editing) confirmEdit();
                else startEdit();
            }
        }

        ToolButton {
            id: deleteBtn
            text: root.editing ? Theme.iconCancel : Theme.iconTrash
            ThemedToolTip {
                visible: deleteBtn.hovered
                text: root.editing ? qsTr("Cancel") : qsTr("Delete")
            }
            contentItem: Text {
                text: parent.text
                color: parent.hovered ? Theme.error : Theme.textMuted
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle { color: "transparent" }
            onClicked: {
                if (root.editing) cancelEdit();
                else root.removeRequested();
            }
        }
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 44 + Theme.spacingMd
            visible: root.editing && root.editInputError !== "" &&
                     inputEdit.text.length > 0
            text: root.editInputError
            color: Theme.error
            font.family: Theme.fontFamily
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 44 + Theme.spacingMd
            visible: root.staticLeaderConflict ||
                     (root.editing && root.editLeaderConflict &&
                      root.editInputError === "")
            text: qsTr("This key is configured as a Leader — mapping will not work")
            color: Theme.warning
            font.family: Theme.fontFamily
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Delete && !root.editing) {
            root.removeRequested();
            event.accepted = true;
        }
    }

    function startEdit() { root.editStartRequested(); }

    function confirmEdit() {
        if (root.modelRef && root.modelRef.updateMapping(
                root.rowIndex, inputEdit.text, outputEdit.text)) {
            root.editEndRequested();
        }
    }

    function cancelEdit() { root.editEndRequested(); }
}
