import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import SchnelleUmlauteOverlay

Window {
    id: win
    flags: Qt.FramelessWindowHint
    color: "transparent"
    width: frame.implicitWidth
    height: frame.implicitHeight
    // Start hidden so main() can configure the layer-shell surface
    // (layer/anchors/screen) before the first commit. main() then calls
    // show() once the surface role is fully set up.
    visible: false

    // Count Unicode codepoints, not UTF-16 code units. Without this,
    // surrogate-pair emojis (😊 et al.) report length 2 and fall into
    // the multi-char size bucket even though they render as one glyph.
    // Array.from + .length is not reliable in QML's V4 JS engine — it
    // counts code units for astral characters, so we skip low
    // surrogates explicitly.
    function codepointCount(s) {
        if (!s) return 0
        let n = 0
        for (let i = 0; i < s.length; i++) {
            const c = s.charCodeAt(i)
            if (c >= 0xDC00 && c <= 0xDFFF) continue
            n++
        }
        return n
    }

    // Emoji-range check on the first codepoint. Color-emoji fonts
    // occupy a smaller fraction of the em-box than JetBrains Mono at
    // the same pixelSize, so we bump pixelSize for emoji variants to
    // keep them visually on par with single-letter variants.
    function isEmoji(s) {
        if (!s) return false
        const cp = s.codePointAt(0)
        return cp >= 0x1F000
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        color: "#ee12101d"
        radius: 16
        border.color: "#2a2640"
        border.width: 1
        implicitWidth: row.implicitWidth + 32
        implicitHeight: 64

        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: 8

            Repeater {
                model: OverlayController.variants
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    readonly property bool active: index === OverlayController.currentIndex
                    width: 44
                    height: 44
                    radius: 10
                    color: active ? "#4ade80" : "#1a1728"
                    border.color: active ? "#4ade80" : "#2a2640"
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: 120 } }

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: active ? "#08060f" : "#f0fdf4"
                        font.family: "JetBrains Mono"
                        font.pixelSize: {
                            if (win.codepointCount(modelData) > 1) return 16
                            return win.isEmoji(modelData) ? 24 : 20
                        }
                        font.weight: Font.Medium
                    }
                }
            }
        }
    }
}
