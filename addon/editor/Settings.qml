import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Item {
    id: root

    property var settingsModel: null
    property var mappingsModel: null

    // Push the active editor palette into the caret candidate-window theme.
    // One place to keep the colour-argument list in sync across the toggle,
    // the placement selector and the theme-change handler.
    function reapplyCaretTheme() {
        root.settingsModel.applyCaretTheme(
            Theme.background, Theme.text, Theme.highlight,
            Theme.highlightText, Theme.border);
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AsNeeded

        readonly property int minContentWidth: 484
        contentWidth: Math.max(root.width, scroll.minContentWidth)

        ColumnLayout {
            width: scroll.contentWidth
            spacing: Theme.spacingMd

            Item { implicitHeight: Theme.spacingLg }

            ColumnLayout {
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                Layout.fillWidth: true
                spacing: Theme.spacingMd

                SettingsCard {
                    titleText: qsTr("Theme")

                    ThemeSelector {
                        Layout.fillWidth: true
                        settingsModel: root.settingsModel
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.spacingXs
                        text: qsTr("Applies to the editor and the cycle overlay. \"Contrast\" meets WCAG AAA (7:1).")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                    }
                }

                SettingsCard {
                    titleText: qsTr("Delay")

                    LabeledRangeSlider {
                        labelText: qsTr("Lowercase")
                        from: 0
                        to: 2000
                        step: 10
                        // Mirrors kDelayMin: the engine floors the window's max at 50ms.
                        upperMin: 50
                        lowerValue: root.settingsModel ? root.settingsModel.delayLowercaseMin : 0
                        upperValue: root.settingsModel ? root.settingsModel.delayLowercase : 400
                        onLowerEdited: (v) => root.settingsModel.delayLowercaseMin = v
                        onUpperEdited: (v) => root.settingsModel.delayLowercase = v
                    }
                    LabeledRangeSlider {
                        labelText: qsTr("Uppercase")
                        from: 0
                        to: 2000
                        step: 10
                        // Mirrors kDelayMin: the engine floors the window's max at 50ms.
                        upperMin: 50
                        lowerValue: root.settingsModel ? root.settingsModel.delayUppercaseMin : 0
                        upperValue: root.settingsModel ? root.settingsModel.delayUppercase : 700
                        onLowerEdited: (v) => root.settingsModel.delayUppercaseMin = v
                        onUpperEdited: (v) => root.settingsModel.delayUppercase = v
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.spacingXs
                        text: qsTr("The accent fires only while the mapped key is held and the leader (e.g. Space) arrives inside the window. Raise the minimum to avoid accidental accents when typing fast; lower it to 0 for the classic timeout-only behavior.")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                    }
                }

                SettingsCard {
                    id: leaderCard
                    titleText: qsTr("Leader Keys")

                    // Reactive note for the "at least one leader" guard: the
                    // model refuses to turn off the last effective leader and
                    // the switch snaps back, so surface why. Cleared once a
                    // leader is added again (effective count rises above one).
                    property bool guardHint: false
                    Connections {
                        target: root.settingsModel
                        function onLeaderRemovalBlocked() { leaderCard.guardHint = true; }
                        function onLeadersChanged() {
                            if (root.settingsModel && root.settingsModel.effectiveLeaderCount > 1)
                                leaderCard.guardHint = false;
                        }
                    }

                    DirectionalLeaderRow {
                        labelText: qsTr("Space")
                        tooltipText: qsTr("Use Space to trigger and cycle accents.")
                        enabledValue: root.settingsModel ? root.settingsModel.leaderSpace : false
                        reverseValue: root.settingsModel ? root.settingsModel.leaderSpaceReverse : false
                        onEnabledToggled: (v) => root.settingsModel.leaderSpace = v
                        onReverseToggled: (v) => root.settingsModel.leaderSpaceReverse = v
                    }
                    // Arrows carry a direction: the toggle left of the enable
                    // switch flips the cycle direction, and the arrow marker
                    // (→ forward, ← reverse) shows it. Any arrow can go either
                    // way, independently.
                    DirectionalLeaderRow {
                        labelText: qsTr("Left Arrow")
                        tooltipText: qsTr("Use the Left arrow to trigger and cycle accents.")
                        enabledValue: root.settingsModel ? root.settingsModel.leaderLeft : false
                        reverseValue: root.settingsModel ? root.settingsModel.leaderLeftReverse : false
                        onEnabledToggled: (v) => root.settingsModel.leaderLeft = v
                        onReverseToggled: (v) => root.settingsModel.leaderLeftReverse = v
                    }
                    DirectionalLeaderRow {
                        labelText: qsTr("Right Arrow")
                        tooltipText: qsTr("Use the Right arrow to trigger and cycle accents.")
                        enabledValue: root.settingsModel ? root.settingsModel.leaderRight : false
                        reverseValue: root.settingsModel ? root.settingsModel.leaderRightReverse : false
                        onEnabledToggled: (v) => root.settingsModel.leaderRight = v
                        onReverseToggled: (v) => root.settingsModel.leaderRightReverse = v
                    }
                    DirectionalLeaderRow {
                        labelText: qsTr("Up Arrow")
                        tooltipText: qsTr("Use the Up arrow to trigger and cycle accents.")
                        enabledValue: root.settingsModel ? root.settingsModel.leaderUp : false
                        reverseValue: root.settingsModel ? root.settingsModel.leaderUpReverse : false
                        onEnabledToggled: (v) => root.settingsModel.leaderUp = v
                        onReverseToggled: (v) => root.settingsModel.leaderUpReverse = v
                    }
                    DirectionalLeaderRow {
                        labelText: qsTr("Down Arrow")
                        tooltipText: qsTr("Use the Down arrow to trigger and cycle accents.")
                        enabledValue: root.settingsModel ? root.settingsModel.leaderDown : false
                        reverseValue: root.settingsModel ? root.settingsModel.leaderDownReverse : false
                        onEnabledToggled: (v) => root.settingsModel.leaderDown = v
                        onReverseToggled: (v) => root.settingsModel.leaderDownReverse = v
                    }
                    DirectionalLeaderRow {
                        labelText: qsTr("Alt")
                        tooltipText: qsTr("Use the left Alt key to trigger and cycle accents.")
                        enabledValue: root.settingsModel ? root.settingsModel.leaderAlt : false
                        reverseValue: root.settingsModel ? root.settingsModel.leaderAltReverse : false
                        onEnabledToggled: (v) => root.settingsModel.leaderAlt = v
                        onReverseToggled: (v) => root.settingsModel.leaderAltReverse = v
                    }
                    DirectionalLeaderRow {
                        labelText: qsTr("AltGr")
                        tooltipText: qsTr("Use AltGr (the right Alt) to trigger and cycle accents.")
                        enabledValue: root.settingsModel ? root.settingsModel.leaderAltGr : false
                        reverseValue: root.settingsModel ? root.settingsModel.leaderAltGrReverse : false
                        onEnabledToggled: (v) => root.settingsModel.leaderAltGr = v
                        onReverseToggled: (v) => root.settingsModel.leaderAltGrReverse = v
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.border
                    }

                    CustomLeaderRow {
                        labelText: qsTr("Custom Leader 1")
                        tooltipText: qsTr("Use a custom physical key to trigger and cycle accents.")
                        enabledValue: root.settingsModel ? root.settingsModel.customKey1Enabled : false
                        reverseValue: root.settingsModel ? root.settingsModel.customKey1Reverse : false
                        keyValue: root.settingsModel ? root.settingsModel.customKey1 : ""
                        keyValueCode: root.settingsModel ? root.settingsModel.customKey1Code : -1
                        keyAssigned: root.settingsModel ? root.settingsModel.customKey1HasKey : false
                        mappingsModel: root.mappingsModel
                        settingsModel: root.settingsModel
                        onEnabledEdited: (v) => root.settingsModel.customKey1Enabled = v
                        onReverseEdited: (v) => root.settingsModel.customKey1Reverse = v
                        onKeyCaptured: (ch, code) => root.settingsModel.captureCustomKey1(ch, code)
                        onKeyCleared: () => root.settingsModel.clearCustomKey1()
                    }

                    CustomLeaderRow {
                        labelText: qsTr("Custom Leader 2 (hand-split)")
                        tooltipText: qsTr("Use a second custom key on the opposite hand to trigger and cycle accents.")
                        enabledValue: root.settingsModel ? root.settingsModel.customKey2Enabled : false
                        reverseValue: root.settingsModel ? root.settingsModel.customKey2Reverse : false
                        keyValue: root.settingsModel ? root.settingsModel.customKey2 : ""
                        keyValueCode: root.settingsModel ? root.settingsModel.customKey2Code : -1
                        keyAssigned: root.settingsModel ? root.settingsModel.customKey2HasKey : false
                        mappingsModel: root.mappingsModel
                        settingsModel: root.settingsModel
                        onEnabledEdited: (v) => root.settingsModel.customKey2Enabled = v
                        onReverseEdited: (v) => root.settingsModel.customKey2Reverse = v
                        onKeyCaptured: (ch, code) => root.settingsModel.captureCustomKey2(ch, code)
                        onKeyCleared: () => root.settingsModel.clearCustomKey2()
                    }

                    Text {
                        visible: leaderCard.guardHint
                        Layout.fillWidth: true
                        Layout.topMargin: Theme.spacingXs
                        text: qsTr("At least one leader must stay active, so this one can't be turned off.")
                        color: Theme.warning
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                    }
                }

                SettingsCard {
                    titleText: qsTr("Overlay")

                    LabeledSwitch {
                        // Governs every overlay feature (cycling picker, trigger
                        // preview, progress bar, caret window), so the label is
                        // the plain master switch, not "while cycling" which
                        // undersold its scope.
                        labelText: qsTr("Show overlay")
                        tooltipText: qsTr("Show the on-screen overlay during the accent gesture.")
                        // Always available: the "At text cursor" placement
                        // renders through fcitx5's input panel and needs no
                        // layer-shell. Only the Grid/MouseCursor placements and
                        // their sub-options below stay layer-shell-gated.
                        enabled: root.settingsModel
                        checked: root.settingsModel ? root.settingsModel.overlayEnabled : false
                        onToggled: (v) => root.settingsModel.overlayEnabled = v
                    }

                    // Placement is the structural choice, so it comes first,
                    // right under the master switch. The sub-options below adapt
                    // to whichever placement is selected.
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMd
                        visible: root.settingsModel
                            && root.settingsModel.overlayEnabled

                        Text {
                            text: qsTr("Placement")
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            Layout.preferredWidth: 120
                        }

                        ThemedComboBox {
                            id: placementBox
                            Layout.fillWidth: true
                            // "At text cursor" renders through fcitx5's input
                            // panel (no layer-shell); "Fixed position" and "At
                            // mouse cursor" drive the overlay daemon, which needs
                            // wlr-layer-shell. Where that is missing, those two
                            // are flagged "(needs layer-shell)" and dimmed inline
                            // so the constraint shows at the point of choice, not
                            // only in the note below. They stay selectable as an
                            // escape hatch if the capability check is wrong.
                            // No `enabled` gate needed: the parent row's
                            // `visible` already carries the same condition, so
                            // the combo only ever renders when it is usable.
                            textRole: "label"
                            valueRole: "key"
                            readonly property bool noLayerShell:
                                root.settingsModel
                                && !root.settingsModel.layerShellAvailable
                            model: [
                                { key: "Grid",
                                  label: placementBox.noLayerShell
                                      ? qsTr("Fixed position (needs layer-shell)")
                                      : qsTr("Fixed position"),
                                  unavailable: placementBox.noLayerShell },
                                { key: "MouseCursor",
                                  label: placementBox.noLayerShell
                                      ? qsTr("At mouse cursor (needs layer-shell)")
                                      : qsTr("At mouse cursor"),
                                  unavailable: placementBox.noLayerShell },
                                { key: "TextCaret",
                                  label: qsTr("At text cursor (caret)"),
                                  unavailable: false }
                            ]
                            currentIndex: {
                                if (!root.settingsModel) return 0;
                                for (var i = 0; i < model.length; ++i) {
                                    if (model[i].key === root.settingsModel.overlayPlacement) return i;
                                }
                                return 0;
                            }
                            onActivated: {
                                if (!root.settingsModel)
                                    return;
                                root.settingsModel.overlayPlacement = model[currentIndex].key;
                                // The caret theme override only makes sense in
                                // caret mode: apply it on entering, restore the
                                // user's theme on leaving (keeping the toggle).
                                if (root.settingsModel.overlayCaretTheme) {
                                    if (model[currentIndex].key === "TextCaret")
                                        root.reapplyCaretTheme();
                                    else
                                        root.settingsModel.clearCaretTheme();
                                }
                            }
                        }
                    }

                    Text {
                        // Sits right under the placement combo it explains:
                        // Grid/MouseCursor need wlr-layer-shell. "At text
                        // cursor" works on X11 but is unreliable on GNOME
                        // Wayland (Mutter lacks the input-method protocol), so
                        // the note splits the two no-layer-shell sessions.
                        visible: root.settingsModel
                            && root.settingsModel.overlayEnabled
                            && !root.settingsModel.layerShellAvailable
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        text: {
                            if (!root.settingsModel)
                                return "";
                            var s = root.settingsModel.layerShellSession;
                            var head = qsTr("\"Fixed position\" and \"At mouse cursor\" need wlr-layer-shell, unavailable on %1.").arg(s);
                            var caret = s.indexOf("(Wayland)") !== -1
                                ? qsTr(" \"At text cursor\" is also unreliable here: this compositor lacks the input-method protocol fcitx5 needs, so it only works in X11/XWayland apps.")
                                : qsTr(" Use \"At text cursor\" instead; it works on X11.");
                            return head + caret
                                + qsTr("\nLayer-shell is supported on KDE Plasma Wayland, sway, Hyprland, river, wayfire.");
                        }
                    }

                    LabeledSwitch {
                        labelText: qsTr("Show timing progress bar")
                        tooltipText: qsTr("Show a bar counting down the accent gesture timing.")
                        // Daemon-only visual (needs layer-shell, no effect in
                        // caret placement). Hide it there like the position
                        // picker instead of leaving a dead disabled switch.
                        visible: root.settingsModel
                            && root.settingsModel.layerShellAvailable
                            && root.settingsModel.overlayEnabled
                            && root.settingsModel.overlayPlacement !== "TextCaret"
                        checked: root.settingsModel ? root.settingsModel.overlayProgressBar : false
                        onToggled: (v) => root.settingsModel.overlayProgressBar = v
                    }

                    PositionPicker {
                        // The grid only matters for Grid/MouseCursor placement;
                        // in TextCaret mode the caret decides the position, so
                        // hide the picker entirely.
                        visible: root.settingsModel
                            && root.settingsModel.layerShellAvailable
                            && root.settingsModel.overlayEnabled
                            && root.settingsModel.overlayPlacement !== "TextCaret"
                        value: root.settingsModel ? root.settingsModel.overlayPosition : "TopCenter"
                        // In mouse-cursor mode the grid is only the fallback: it
                        // stays marked but dimmed, and a pointer marker shows the
                        // menu follows the mouse.
                        atCursorMode: root.settingsModel
                            ? root.settingsModel.overlayPlacement === "MouseCursor"
                            : false
                        onEdited: (v) => root.settingsModel.overlayPosition = v
                    }

                    // Caret mode only: style fcitx5's candidate window to match
                    // the active editor theme. Opt-in because it changes the
                    // global candidate window (see warning below).
                    LabeledSwitch {
                        labelText: qsTr("Match candidate window to this theme")
                        tooltipText: qsTr("Style fcitx5's candidate window to match this theme (At-text-cursor placement).")
                        visible: root.settingsModel
                            && root.settingsModel.overlayEnabled
                            && root.settingsModel.overlayPlacement === "TextCaret"
                        enabled: root.settingsModel
                            && root.settingsModel.overlayEnabled
                        checked: root.settingsModel ? root.settingsModel.overlayCaretTheme : false
                        onToggled: (v) => {
                            root.settingsModel.overlayCaretTheme = v;
                            if (v)
                                root.reapplyCaretTheme();
                        }
                    }

                    Text {
                        // Neutral (muted), like the other field descriptions:
                        // it explains a global side effect (it rewrites the
                        // shared classicui theme, affecting other input methods)
                        // but the warning colour read as alarming for an opt-in.
                        visible: root.settingsModel
                            && root.settingsModel.overlayEnabled
                            && root.settingsModel.overlayPlacement === "TextCaret"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        text: qsTr("Styles fcitx5's candidate window globally (it affects other input methods too) and overrides your classicui theme while on. Turning it off restores your previous theme.")
                    }

                    LabeledSwitch {
                        labelText: qsTr("Preview in the trigger window")
                        tooltipText: qsTr("Show the accent preview the moment the gesture fires.")
                        // Applies to every placement (the caret path shows the
                        // same preview), so it only depends on the overlay
                        // being enabled, not on layer-shell.
                        enabled: root.settingsModel
                            && root.settingsModel.overlayEnabled
                        checked: root.settingsModel ? root.settingsModel.overlayShowOnTrigger : false
                        onToggled: (v) => root.settingsModel.overlayShowOnTrigger = v
                    }

                    Text {
                        // One combined note: what the preview does plus the
                        // single-accent caveat (those keys never cycle, so the
                        // preview is the only way they ever get an overlay).
                        // Merged from two stacked paragraphs to thin the
                        // muted-text wall.
                        visible: root.settingsModel
                            && root.settingsModel.overlayEnabled
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        text: qsTr("Shows the available accents as soon as you hold a mapped key, before pressing a leader. It is also the only way single-accent keys, which never cycle, get an overlay.")
                    }

                    // Re-apply on every editor theme change while the toggle is
                    // on, so the caret window follows the selected theme.
                    Connections {
                        target: Theme
                        function onCurrentChanged() {
                            if (root.settingsModel
                                && root.settingsModel.overlayCaretTheme
                                && root.settingsModel.overlayPlacement === "TextCaret")
                                root.reapplyCaretTheme();
                        }
                    }
                }

                SettingsCard {
                    titleText: qsTr("App Filter")

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingMd

                        Text {
                            text: qsTr("Mode")
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontBody
                            Layout.preferredWidth: 120
                        }

                        ThemedComboBox {
                            id: modeBox
                            Layout.fillWidth: true
                            model: ["Disabled", "Blacklist", "Whitelist"]
                            currentIndex: root.settingsModel
                                ? model.indexOf(root.settingsModel.appFilterMode)
                                : 0
                            onActivated: {
                                if (root.settingsModel) {
                                    root.settingsModel.appFilterMode = model[currentIndex];
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.border
                        visible: modeBox.currentIndex !== 0
                    }

                    AppListEditor {
                        visible: modeBox.currentIndex === 1
                        labelText: qsTr("Blacklist")
                        items: root.settingsModel ? root.settingsModel.blacklist : []
                        onAddRequested: (e) => root.settingsModel.addBlacklistEntry(e)
                        onRemoveRequested: (i) => root.settingsModel.removeBlacklistEntry(i)
                    }

                    AppListEditor {
                        visible: modeBox.currentIndex === 2
                        labelText: qsTr("Whitelist")
                        items: root.settingsModel ? root.settingsModel.whitelist : []
                        onAddRequested: (e) => root.settingsModel.addWhitelistEntry(e)
                        onRemoveRequested: (i) => root.settingsModel.removeWhitelistEntry(i)
                    }

                    Text {
                        visible: modeBox.currentIndex !== 0
                        Layout.fillWidth: true
                        text: qsTr("Case-sensitive substring match against the program identifier fcitx5 reports. Some apps report their GUI library instead of their name (e.g. Kitty → GLFW_Application).")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                    }
                }

                Item { implicitHeight: Theme.spacingLg }
            }
        }
    }

    // Click anywhere empty to drop keyboard focus, disarming an armed
    // custom-leader capture field. Topmost so it sees every press first, but
    // passes them through so the ScrollView and controls keep working.
    FocusSink {}
}
