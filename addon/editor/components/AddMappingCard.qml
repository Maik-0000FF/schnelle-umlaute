import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Rectangle {
    id: root
    radius: Theme.radiusLg
    color: Theme.surface
    border.color: Theme.border
    border.width: 1
    implicitHeight: layout.implicitHeight + Theme.spacingLg * 2

    property var modelRef: null
    property var settingsModel: null
    signal mappingAdded(string input, string output)

    function focusInput() { inputField.forceActiveFocus(); }

    // isActiveLeaderKey is a method call — bump a tick when leaders change
    // and reference it in the binding to force re-evaluation.
    property int leadersTick: 0
    Connections {
        target: root.settingsModel
        function onLeadersChanged() { root.leadersTick++; }
    }

    readonly property string inputError:
        modelRef ? modelRef.inputErrorFor(inputField.text) : ""
    readonly property bool leaderConflict: {
        leadersTick; // establish dependency
        return inputField.text.length > 0 && settingsModel &&
            settingsModel.isActiveLeaderKey(inputField.text);
    }
    readonly property bool outputValid:
        modelRef && outputField.text.length > 0 &&
        modelRef.validateOutput(outputField.text)
    readonly property bool canAdd:
        inputField.text.length > 0 && inputError === "" && outputValid

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingSm

        Text {
            text: qsTr("New mapping")
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontCaption
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingMd

            ThemedTextField {
                id: inputField
                Layout.preferredWidth: 80
                placeholderText: qsTr("Key")
                maximumLength: 4
                font.family: Theme.fontFamilyMono
                font.pixelSize: Theme.fontStrong
                horizontalAlignment: TextInput.AlignHCenter
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: Theme.background
                    border.color: root.inputError !== ""
                        ? Theme.error
                        : (root.leaderConflict
                            ? Theme.warning
                            : (inputField.activeFocus ? Theme.accent : Theme.border))
                    border.width: 1
                    Behavior on border.color { ColorAnimation { duration: Theme.animShort } }
                }
                onAccepted: if (root.canAdd) commit()
            }

            Text {
                text: "→"
                color: Theme.textMuted
                font.pixelSize: Theme.fontDisplay
            }

            ThemedTextField {
                id: outputField
                Layout.fillWidth: true
                placeholderText: qsTr("Output (e.g. ä or é,è,ê,ë)")
                font.family: Theme.fontFamilyMono
                font.pixelSize: Theme.fontStrong
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: Theme.background
                    border.color: outputField.activeFocus ? Theme.accent : Theme.border
                    border.width: 1
                    Behavior on border.color { ColorAnimation { duration: Theme.animShort } }
                }
                onAccepted: if (root.canAdd) commit()
            }

            Button {
                id: addBtn
                text: Theme.iconAdd
                enabled: root.canAdd
                implicitWidth: 44
                implicitHeight: Theme.controlHeightLg
                contentItem: Text {
                    text: addBtn.text
                    color: addBtn.enabled ? "#ffffff" : Theme.textMuted
                    font.pixelSize: Theme.fontDisplay
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: Theme.radiusSm
                    color: addBtn.enabled
                        ? (addBtn.hovered ? Theme.accentHover : Theme.accent)
                        : Theme.surfaceHover
                    Behavior on color { ColorAnimation { duration: Theme.animShort } }
                }
                onClicked: commit()
            }
        }

        Text {
            Layout.fillWidth: true
            text: (root.inputError !== "" && inputField.text.length > 0)
                ? root.inputError
                : (root.leaderConflict
                    ? qsTr("This key is configured as a Leader — mapping will not work")
                    : qsTr("Key: a single character. Output: text or comma-separated variants for cycling."))
            color: (root.inputError !== "" && inputField.text.length > 0)
                ? Theme.error
                : (root.leaderConflict ? Theme.warning : Theme.textMuted)
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontCaption
            wrapMode: Text.WordWrap
            Behavior on color { ColorAnimation { duration: Theme.animShort } }
        }
    }

    function commit() {
        if (!canAdd) return;
        root.mappingAdded(inputField.text, outputField.text);
        inputField.clear();
        outputField.clear();
        inputField.forceActiveFocus();
    }
}
