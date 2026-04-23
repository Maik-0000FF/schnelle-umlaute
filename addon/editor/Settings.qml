import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SchnelleUmlaute

Item {
    id: root

    property var settingsModel: null
    property var mappingsModel: null

    ScrollView {
        anchors.fill: parent
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: root.width
            spacing: Theme.spacingMd

            Item { implicitHeight: Theme.spacingLg }

            ColumnLayout {
                Layout.leftMargin: Theme.spacingLg
                Layout.rightMargin: Theme.spacingLg
                Layout.fillWidth: true
                spacing: Theme.spacingMd

                SettingsCard {
                    titleText: qsTr("Delay")

                    LabeledSlider {
                        labelText: qsTr("Lowercase")
                        minValue: 50
                        maxValue: 2000
                        stepSize: 25
                        value: root.settingsModel ? root.settingsModel.delayLowercase : 400
                        onValueEdited: (v) => root.settingsModel.delayLowercase = v
                    }
                    LabeledSlider {
                        labelText: qsTr("Uppercase")
                        minValue: 50
                        maxValue: 2000
                        stepSize: 25
                        value: root.settingsModel ? root.settingsModel.delayUppercase : 700
                        onValueEdited: (v) => root.settingsModel.delayUppercase = v
                    }
                }

                SettingsCard {
                    titleText: qsTr("Leader Keys")

                    LabeledSwitch {
                        labelText: qsTr("Space")
                        checked: root.settingsModel ? root.settingsModel.leaderSpace : false
                        onToggled: (v) => root.settingsModel.leaderSpace = v
                    }
                    LabeledSwitch {
                        labelText: qsTr("Left Arrow")
                        checked: root.settingsModel ? root.settingsModel.leaderLeft : false
                        onToggled: (v) => root.settingsModel.leaderLeft = v
                    }
                    LabeledSwitch {
                        labelText: qsTr("Right Arrow")
                        checked: root.settingsModel ? root.settingsModel.leaderRight : false
                        onToggled: (v) => root.settingsModel.leaderRight = v
                    }
                    LabeledSwitch {
                        labelText: qsTr("Up Arrow")
                        checked: root.settingsModel ? root.settingsModel.leaderUp : false
                        onToggled: (v) => root.settingsModel.leaderUp = v
                    }
                    LabeledSwitch {
                        labelText: qsTr("Down Arrow")
                        checked: root.settingsModel ? root.settingsModel.leaderDown : false
                        onToggled: (v) => root.settingsModel.leaderDown = v
                    }
                    LabeledSwitch {
                        labelText: qsTr("Alt / AltGr")
                        checked: root.settingsModel ? root.settingsModel.leaderAlt : false
                        onToggled: (v) => root.settingsModel.leaderAlt = v
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.border
                    }

                    CustomLeaderRow {
                        labelText: qsTr("Custom Leader 1")
                        placeholderHint: ""
                        enabledValue: root.settingsModel ? root.settingsModel.customKey1Enabled : false
                        keyValue: root.settingsModel ? root.settingsModel.customKey1 : ""
                        mappingsModel: root.mappingsModel
                        settingsModel: root.settingsModel
                        onEnabledEdited: (v) => root.settingsModel.customKey1Enabled = v
                        onKeyEdited: (v) => root.settingsModel.customKey1 = v
                    }

                    CustomLeaderRow {
                        labelText: qsTr("Custom Leader 2 (hand-split)")
                        placeholderHint: ""
                        enabledValue: root.settingsModel ? root.settingsModel.customKey2Enabled : false
                        keyValue: root.settingsModel ? root.settingsModel.customKey2 : ""
                        mappingsModel: root.mappingsModel
                        settingsModel: root.settingsModel
                        onEnabledEdited: (v) => root.settingsModel.customKey2Enabled = v
                        onKeyEdited: (v) => root.settingsModel.customKey2 = v
                    }
                }

                SettingsCard {
                    titleText: qsTr("Overlay")

                    LabeledSwitch {
                        labelText: qsTr("Show overlay while cycling")
                        enabled: root.settingsModel && root.settingsModel.layerShellAvailable
                        checked: root.settingsModel ? root.settingsModel.overlayEnabled : false
                        onToggled: (v) => root.settingsModel.overlayEnabled = v
                    }

                    Text {
                        visible: root.settingsModel && !root.settingsModel.layerShellAvailable
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        text: root.settingsModel
                            ? qsTr("Unavailable on %1. %2\nSupported: KDE Plasma Wayland, sway, Hyprland, river, wayfire.")
                                .arg(root.settingsModel.layerShellSession)
                                .arg(root.settingsModel.layerShellReason)
                            : ""
                    }

                    PositionPicker {
                        visible: root.settingsModel
                            && root.settingsModel.layerShellAvailable
                            && root.settingsModel.overlayEnabled
                        value: root.settingsModel ? root.settingsModel.overlayPosition : "TopCenter"
                        onEdited: (v) => root.settingsModel.overlayPosition = v
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
                            font.pixelSize: 13
                            Layout.preferredWidth: 120
                        }

                        ComboBox {
                            id: modeBox
                            Layout.fillWidth: true
                            model: ["Disabled", "Blacklist", "Whitelist"]
                            currentIndex: root.settingsModel
                                ? model.indexOf(root.settingsModel.appFilterMode)
                                : 0
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            onActivated: {
                                if (root.settingsModel) {
                                    root.settingsModel.appFilterMode = model[currentIndex];
                                }
                            }
                            contentItem: Text {
                                text: modeBox.displayText
                                color: Theme.text
                                font: modeBox.font
                                leftPadding: Theme.spacingMd
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: Theme.radiusSm
                                color: Theme.background
                                border.color: Theme.border
                                border.width: 1
                                implicitHeight: 34
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
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                Item { implicitHeight: Theme.spacingLg }
            }
        }
    }
}
