import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import SchnelleUmlaute

ApplicationWindow {
    id: root
    visible: true
    width: 680
    height: 640
    minimumWidth: 520
    minimumHeight: 440
    title: qsTr("Schnelle Umlaute — Mapping Editor")
    color: Theme.background

    MappingListModel {
        id: mappings
        onErrorOccurred: (msg) => snackbar.show(msg, Theme.error)
    }

    SettingsModel { id: settings }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Header {
            Layout.fillWidth: true
            mappingCount: mappings.count
            onReloadRequested: { mappings.reload(); }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        TabBar {
            id: tabBar
            Layout.fillWidth: true
            background: Rectangle { color: Theme.background }

            TabButton {
                text: qsTr("Mappings")
                contentItem: Text {
                    text: parent.text
                    color: tabBar.currentIndex === 0 ? Theme.brand : Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    Behavior on color { ColorAnimation { duration: Theme.animShort } }
                }
                background: Rectangle {
                    color: "transparent"
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 2
                        color: tabBar.currentIndex === 0 ? Theme.brand : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.animShort } }
                    }
                }
            }
            TabButton {
                text: qsTr("Settings")
                contentItem: Text {
                    text: parent.text
                    color: tabBar.currentIndex === 1 ? Theme.brand : Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    Behavior on color { ColorAnimation { duration: Theme.animShort } }
                }
                background: Rectangle {
                    color: "transparent"
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 2
                        color: tabBar.currentIndex === 1 ? Theme.brand : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.animShort } }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            Mappings {
                id: mappingsPanel
                mappingsModel: mappings
                onRequestSnackbar: (msg, c) => snackbar.show(msg, c)
                onRequestUndoSnackbar: (msg, cb) => snackbar.showUndo(msg, cb)
            }

            Settings {
                settingsModel: settings
                mappingsModel: mappings
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        Footer {
            Layout.fillWidth: true
            saveStatus: mappings.saveStatus
        }
    }

    Rectangle {
        id: snackbar
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: Theme.spacingXl + 40
        width: Math.min(rowLayout.implicitWidth + Theme.spacingLg * 2, root.width - 40)
        height: 44
        radius: Theme.radiusMd
        color: Theme.surface
        border.color: currentColor
        border.width: 1
        opacity: 0
        visible: opacity > 0

        property color currentColor: Theme.accent
        property var undoCallback: null

        function show(message, accent) {
            undoButton.visible = false;
            undoCallback = null;
            text.text = message;
            currentColor = accent;
            opacity = 1;
            hideTimer.restart();
        }

        function showUndo(message, callback) {
            undoButton.visible = true;
            undoCallback = callback;
            text.text = message;
            currentColor = Theme.warning;
            opacity = 1;
            hideTimer.restart();
        }

        Behavior on opacity {
            NumberAnimation { duration: Theme.animMed }
        }

        Timer {
            id: hideTimer
            interval: 4000
            onTriggered: snackbar.opacity = 0
        }

        RowLayout {
            id: rowLayout
            anchors.centerIn: parent
            spacing: Theme.spacingMd

            Text {
                id: text
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 13
            }

            Button {
                id: undoButton
                text: qsTr("Undo")
                flat: true
                visible: false
                contentItem: Text {
                    text: undoButton.text
                    color: Theme.accent
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }
                background: Rectangle { color: "transparent" }
                onClicked: {
                    if (snackbar.undoCallback) snackbar.undoCallback();
                    snackbar.opacity = 0;
                }
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+N"
        onActivated: {
            tabBar.currentIndex = 0;
            mappingsPanel.focusAdd();
        }
    }
    Shortcut {
        sequence: "Ctrl+1"
        onActivated: tabBar.currentIndex = 0
    }
    Shortcut {
        sequence: "Ctrl+2"
        onActivated: tabBar.currentIndex = 1
    }
    Shortcut {
        sequence: "F5"
        onActivated: mappings.reload()
    }
    Shortcut {
        sequence: "Esc"
        onActivated: root.close()
    }
}
