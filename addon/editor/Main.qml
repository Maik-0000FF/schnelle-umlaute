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
        // States for the IM environment:
        //   1) isConfigured()              → env-vars active, do nothing
        //   2) no valid file               → first-run; offer setup
        //   3) file valid, env.d honored   → logout pending; relogin
        //                                    activates the variables
        //   4) file valid, env.d ignored   → TTY-launched compositor
        //                                    (e.g. Hyprland): relogin
        //                                    never reads environment.d,
        //                                    so point at the compositor
        //                                    config instead of repeating
        //                                    a logout that won't help
        if (!envSetup.isConfigured()) {
            if (!envSetup.hasValidConfigFile()) {
                envDialog.open();
            } else if (envSetup.honorsEnvironmentD()) {
                logoutDialog.open();
            } else {
                compositorDialog.open();
            }
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

    ConfirmDialog {
        id: logoutDialog
        titleText: qsTr("Logout pending")
        messageText: qsTr(
            "Setup is complete, but the environment variables are not " +
            "active in this session yet — environment.d files are read " +
            "once at login.\n\n" +
            "Log out and back in to activate. After that, this dialog " +
            "will stop appearing."
        )
        confirmText: qsTr("OK")
        confirmStyle: "primary"
        singleButton: true
    }

    ConfirmDialog {
        id: compositorDialog
        titleText: qsTr("Activation pending — %1").arg(envSetup.sessionName())
        messageText: {
            const intro = qsTr(
                "The variables were written to %1, but your session does " +
                "not import environment.d files — logging out will not " +
                "activate them.").arg(envSetup.configPath());
            const path = envSetup.compositorConfigPath();
            const where = path.length > 0
                ? qsTr("Add these lines to %1 and restart your session:")
                    .arg(path)
                : qsTr("Export these variables before your compositor " +
                       "starts (or launch it via uwsm) and restart your " +
                       "session:");
            const tail = qsTr(
                "If you start your session through a display manager or " +
                "uwsm, logging out and back in is enough instead.");
            return intro + "\n\n" + where + "\n\n"
                + envSetup.compositorEnvSnippet() + "\n\n" + tail;
        }
        confirmText: qsTr("OK")
        confirmStyle: "primary"
        singleButton: true
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
