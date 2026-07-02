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
        // If the deleted profile was the Mappings edit target, fall back to the
        // active profile so the tab never keeps editing an orphaned file.
        onProfileRemoved: (file) => {
            if (mappings.profileFile === file)
                mappings.profileFile = profiles.fileForRow(profiles.activeRow());
        }
    }

    Component.onCompleted: {
        Theme.setCurrent(settings.theme);
        // Default the Mappings edit target to the active profile (the two are
        // otherwise independent: you can switch the edit target without
        // changing which profile is active at runtime).
        mappings.profileFile = profiles.fileForRow(profiles.activeRow());
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
                    id: tabRepeater
                    model: [qsTr("Settings"), qsTr("Mappings")]
                    delegate: Item {
                        id: tabItem
                        required property int index
                        required property string modelData
                        Layout.preferredWidth: tabLabel.implicitWidth + Theme.spacingLg * 2
                        Layout.preferredHeight: 36
                        readonly property bool active: tabRow.currentIndex === index

                        // Reachable by Tab; Left/Right move between tabs and
                        // select, Space/Enter select the focused tab.
                        activeFocusOnTab: true
                        Keys.onPressed: (event) => {
                            if (event.key === Qt.Key_Left
                                || event.key === Qt.Key_Right) {
                                const dir = event.key === Qt.Key_Right ? 1 : -1;
                                const n = (index + dir + tabRepeater.count)
                                          % tabRepeater.count;
                                tabRow.currentIndex = n;
                                tabRepeater.itemAt(n).forceActiveFocus(
                                    Qt.TabFocusReason);
                                event.accepted = true;
                            } else if (event.key === Qt.Key_Space
                                       || event.key === Qt.Key_Return
                                       || event.key === Qt.Key_Enter) {
                                tabRow.currentIndex = index;
                                event.accepted = true;
                            }
                        }

                        FocusRing { visible: tabItem.activeFocus }

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
                            font.pixelSize: Theme.fontBody
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

            // Move keyboard focus into the newly shown panel on every tab
            // switch (mouse, Space/Enter, Left/Right, or a Ctrl shortcut all
            // funnel through currentIndex). Without this, a control on the
            // now-hidden page keeps active focus and still eats keys — e.g. the
            // theme combo would turn arrow presses into theme changes after you
            // switch to Mappings. Keyboard tab-nav re-grabs focus onto the tab
            // itself right after (Keys.onPressed above), so Left/Right cycling
            // is unaffected.
            onCurrentIndexChanged: currentIndex === 0
                ? settingsPanel.focusPanel()
                : mappingsPanel.focusPanel()

            Settings {
                id: settingsPanel
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
                font.pixelSize: Theme.fontBody
            }

            Button {
                id: undoButton
                // Keyboard-reachable via Tab, but must not grab focus on click.
                focusPolicy: Qt.TabFocus
                text: qsTr("Undo")
                flat: true
                visible: false
                contentItem: Text {
                    text: undoButton.text
                    color: Theme.accent
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody
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
    // Step through the tabs like a typical multi-document editor.
    Shortcut {
        sequence: "Ctrl+Tab"
        onActivated: tabRow.currentIndex =
            (tabRow.currentIndex + 1) % tabRepeater.count
    }
    Shortcut {
        sequence: "Ctrl+Shift+Tab"
        onActivated: tabRow.currentIndex =
            (tabRow.currentIndex - 1 + tabRepeater.count) % tabRepeater.count
    }
    // Switch the Mappings edit-target profile with the conventional
    // next/previous-document keys, kept distinct from the global cycle keys.
    Shortcut {
        sequence: "Ctrl+PgDown"
        onActivated: root.cycleEditTarget(1)
    }
    Shortcut {
        sequence: "Ctrl+PgUp"
        onActivated: root.cycleEditTarget(-1)
    }
    Shortcut {
        sequence: "Esc"
        onActivated: root.close()
    }

    // Move the Mappings edit target to the next/previous profile (dir = +1/-1),
    // wrapping around, and reveal it on the Mappings tab.
    function cycleEditTarget(dir) {
        const n = profiles.count;
        if (n <= 1)
            return;
        let cur = 0;
        for (let i = 0; i < n; i++)
            if (profiles.fileForRow(i) === mappings.profileFile) {
                cur = i;
                break;
            }
        mappings.profileFile = profiles.fileForRow((cur + dir + n) % n);
        tabRow.currentIndex = 1;
    }
}
