import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import SchnelleUmlaute
import "components"

ApplicationWindow {
    id: root
    visible: true
    width: 680
    height: 640
    minimumWidth: 520
    minimumHeight: 440
    title: qsTr("Schnelle Umlaute")
    color: Theme.background

    MappingListModel {
        id: mappings
        onErrorOccurred: (msg) => snackbar.show(msg, Theme.error)
    }

    SettingsModel {
        id: settings
        onThemeChanged: Theme.setCurrent(theme)
    }

    Component.onCompleted: {
        Theme.setCurrent(settings.theme);
        // Without the input-method environment variables set in the
        // user's session, the addon does not hook into any application
        // and every setting edited here would silently have no effect.
        // Offer to write ~/.config/environment.d/fcitx5.conf so the
        // user can complete the install (which the AUR package alone
        // cannot do — environment.d is per-user, not per-package).
        if (!envSetup.isConfigured()) {
            envDialog.open();
        }
    }

    ConfirmDialog {
        id: envDialog
        titleText: qsTr("Setup required")
        messageText: qsTr(
            "Input-method environment variables are not set. Without " +
            "them, Schnelle Umlaute has no effect in any application.\n\n" +
            "Create %1 and log out / in to activate."
        ).arg(envSetup.configPath())
        confirmText: qsTr("Set up now")
        cancelText: qsTr("Cancel")
        confirmStyle: "primary"
        onConfirmed: () => {
            if (envSetup.writeConfig()) {
                snackbar.show(
                    qsTr("Set up — log out and back in for the variables to take effect."),
                    Theme.success);
            } else {
                snackbar.show(
                    qsTr("Setup failed — please create %1 manually.")
                        .arg(envSetup.configPath()),
                    Theme.error);
            }
        }
    }

    ColumnLayout {
        id: rootLayout
        anchors.fill: parent
        spacing: 0

        Header {
            Layout.fillWidth: true
            mappingCount: mappings.count
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: 42
            z: 1

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.border
                z: 0
            }

            RowLayout {
                id: tabRow
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingLg
                spacing: Theme.spacingXs
                z: 1

                property int currentIndex: 0

                Repeater {
                    model: [qsTr("Settings"), qsTr("Mappings")]
                    delegate: Item {
                        required property int index
                        required property string modelData
                        Layout.preferredWidth: tabLabel.implicitWidth + Theme.spacingLg * 2
                        Layout.preferredHeight: 36
                        readonly property bool active: tabRow.currentIndex === index

                        // Underline strip — sits over the row separator's
                        // 1 px border at the bottom of the tab strip and
                        // breaks through it for the active tab. 2 px tall
                        // so it remains visible on HiDPI without blooming.
                        // Theme.accent (varies per theme: violet / blue /
                        // blue / yellow) instead of Theme.brand (constant
                        // green) so the marker reads as part of the theme.
                        Rectangle {
                            visible: parent.active
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 2
                            color: Theme.accent
                        }

                        Text {
                            id: tabLabel
                            anchors.centerIn: parent
                            text: modelData
                            color: parent.active
                                ? Theme.accent
                                : (tabMouse.containsMouse ? Theme.text
                                                          : Theme.textMuted)
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            font.weight: parent.active ? Font.Medium : Font.Normal
                            Behavior on color { ColorAnimation { duration: Theme.animShort } }
                        }

                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: tabRow.currentIndex = index
                        }
                    }
                }
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabRow.currentIndex

            Settings {
                settingsModel: settings
                mappingsModel: mappings
            }

            Mappings {
                id: mappingsPanel
                mappingsModel: mappings
                settingsModel: settings
                onRequestSnackbar: (msg, c) => snackbar.show(msg, c)
                onRequestUndoSnackbar: (msg, cb) => snackbar.showUndo(msg, cb)
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
            tabRow.currentIndex = 1;
            mappingsPanel.focusAdd();
        }
    }
    Shortcut {
        sequence: "Ctrl+1"
        onActivated: tabRow.currentIndex = 0
    }
    Shortcut {
        sequence: "Ctrl+2"
        onActivated: tabRow.currentIndex = 1
    }
    Shortcut {
        sequence: "Esc"
        onActivated: root.close()
    }
}
