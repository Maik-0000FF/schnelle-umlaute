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

    ProfileListModel {
        id: profiles
        onErrorOccurred: (msg) => snackbar.show(msg, Theme.error)
    }

    Component.onCompleted: {
        Theme.setCurrent(settings.theme);
        // States for the IM environment (checked only when the variables
        // are not already active):
        //   1) env.d not imported          → TTY-launched compositor
        //                                    (e.g. Hyprland): relogin never
        //                                    reads environment.d, so the
        //                                    env.d first-run/logout advice is
        //                                    useless here. Split like the
        //                                    env.d path: if the compositor
        //                                    config already has the lines,
        //                                    only a session restart is
        //                                    pending (informational); else
        //                                    offer to add them.
        //   2) env.d imported, no file     → first-run; offer setup
        //   3) env.d imported, file valid  → logout pending; relogin
        //                                    activates the variables
        if (!envSetup.isConfigured()) {
            if (!envSetup.honorsEnvironmentD()) {
                if (envSetup.compositorConfigPath().length > 0
                        && envSetup.hasValidCompositorConfig()) {
                    compositorRestartDialog.open();
                } else {
                    compositorDialog.open();
                }
            } else if (!envSetup.hasValidConfigFile()) {
                envDialog.open();
            } else {
                logoutDialog.open();
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
        // Non-empty only for compositors with a known config file and
        // syntax (Hyprland). When set, the dialog can write the lines
        // itself; otherwise it falls back to showing them for manual use.
        readonly property string compPath: envSetup.compositorConfigPath()
        titleText: qsTr("Activation pending — %1").arg(envSetup.sessionName())
        messageText: {
            const intro = qsTr(
                "Input-method variables are not active and your session " +
                "does not import environment.d files — logging out will " +
                "not activate them, so they belong in your compositor " +
                "configuration instead.");
            const where = compPath.length > 0
                ? qsTr("These lines belong in %1. Add them below, then " +
                       "restart your session:").arg(compPath)
                : qsTr("Export these variables before your compositor " +
                       "starts (or launch it via uwsm) and restart your " +
                       "session:");
            const tail = qsTr(
                "If you start your session through a display manager or " +
                "uwsm, logging out and back in is enough instead.");
            return intro + "\n\n" + where + "\n\n"
                + envSetup.compositorEnvSnippet() + "\n\n" + tail;
        }
        confirmText: compPath.length > 0 ? qsTr("Add to config") : qsTr("OK")
        cancelText: qsTr("Cancel")
        confirmStyle: "primary"
        // No config to write for sway/river/etc. → informational OK only.
        singleButton: compPath.length === 0
        onConfirmed: () => {
            if (compPath.length === 0)
                return;
            if (envSetup.writeCompositorConfig()) {
                snackbar.show(
                    qsTr("Added to %1 — restart your session to activate.")
                        .arg(compPath),
                    Theme.success);
            } else {
                snackbar.show(
                    qsTr("Could not write %1 — please add the lines manually.")
                        .arg(compPath),
                    Theme.error);
            }
        }
    }

    // Compositor counterpart to logoutDialog: the lines are already in the
    // config file (hasValidCompositorConfig()), so the only thing left is a
    // session restart. Informational, no write button — that avoids
    // re-running the now no-op write and falsely reporting "Added".
    ConfirmDialog {
        id: compositorRestartDialog
        titleText: qsTr("Restart pending — %1").arg(envSetup.sessionName())
        messageText: qsTr(
            "The input-method variables are set in %1 but are not active " +
            "in this session yet — your compositor exports them only at " +
            "startup.\n\n" +
            "Restart your session (log out and back into the compositor, " +
            "not just reload the config) to activate them. After that, " +
            "this dialog will stop appearing."
        ).arg(envSetup.compositorConfigPath())
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
                    model: [qsTr("Settings"), qsTr("Mappings"), qsTr("Profiles")]
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
                profilesModel: profiles
                onRequestSnackbar: (msg, c) => snackbar.show(msg, c)
                onRequestUndoSnackbar: (msg, cb) => snackbar.showUndo(msg, cb)
            }

            Profiles {
                id: profilesPanel
                profilesModel: profiles
                onRequestSnackbar: (msg, c) => snackbar.show(msg, c)
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
        sequence: "Ctrl+3"
        onActivated: tabRow.currentIndex = 2
    }
    Shortcut {
        sequence: "Esc"
        onActivated: root.close()
    }
}
