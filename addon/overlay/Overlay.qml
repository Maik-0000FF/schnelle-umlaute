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

    // Single source of truth for the panel background opacity. All four
    // themes share this value so a future tweak is one line, not four
    // embedded alpha bytes in the palette hex strings. Cell colors,
    // borders and text stay fully opaque — only the frame fades.
    readonly property real frameOpacity: 0.80

    // Palettes mirror addon/editor/Theme.qml. Inlined because the overlay
    // lives in its own QML module and process — sharing a singleton would
    // cost more build plumbing than the 4 small dicts are worth.
    // `frame` stores RGB only; the panel applies frameOpacity at render
    // time via Qt.alpha().
    readonly property var palettes: ({
        "schnelle-umlaute": {
            frame: "#12101d", border: "#2a2640",
            cellInactive: "#1a1728", cellInactiveBorder: "#2a2640",
            cellActive: "#4ade80", cellActiveBorder: "#4ade80",
            textInactive: "#f0fdf4", textActive: "#08060f"
        },
        "dark": {
            frame: "#181b22", border: "#2a2f3a",
            cellInactive: "#232832", cellInactiveBorder: "#2a2f3a",
            cellActive: "#60a5fa", cellActiveBorder: "#60a5fa",
            textInactive: "#e5e7eb", textActive: "#0f1115"
        },
        "light": {
            frame: "#ffffff", border: "#d4d4d8",
            cellInactive: "#f4f4f5", cellInactiveBorder: "#d4d4d8",
            cellActive: "#2563eb", cellActiveBorder: "#2563eb",
            textInactive: "#0f172a", textActive: "#ffffff"
        },
        "contrast": {
            frame: "#000000", border: "#ffffff",
            cellInactive: "#0a0a0a", cellInactiveBorder: "#ffffff",
            cellActive: "#ffd60a", cellActiveBorder: "#ffd60a",
            textInactive: "#ffffff", textActive: "#000000"
        }
    })
    readonly property var p: palettes[OverlayController.theme]
                             || palettes["schnelle-umlaute"]

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

    // Trim long variants so they stay inside the 44×44 cell. Three
    // codepoints fit in JetBrains Mono at pixelSize 16 (≈9.6 px each);
    // anything longer becomes "xy…" — two leading codepoints plus U+2026
    // HORIZONTAL ELLIPSIS. The ellipsis is a single narrow glyph, so
    // "xy…" still fits the same budget as "xyz". Surrogate-pair aware so
    // a two-codepoint prefix ending on an emoji copies both halves.
    function truncateDisplay(s) {
        if (!s || codepointCount(s) <= 3) return s
        let out = ""
        let taken = 0
        for (let i = 0; i < s.length && taken < 2; i++) {
            const c = s.charCodeAt(i)
            out += s.charAt(i)
            if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.length) {
                // high surrogate — pull its low surrogate in too, still
                // counts as one codepoint.
                out += s.charAt(++i)
            }
            taken++
        }
        return out + "…"
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
        color: Qt.alpha(win.p.frame, win.frameOpacity)
        radius: 16
        border.color: win.p.border
        border.width: 1
        implicitWidth: row.implicitWidth + 32
        implicitHeight: 64

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }

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
                    color: active ? win.p.cellActive : win.p.cellInactive
                    border.color: active ? win.p.cellActiveBorder : win.p.cellInactiveBorder
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: 120 } }
                    Behavior on border.color { ColorAnimation { duration: 120 } }

                    Text {
                        anchors.centerIn: parent
                        text: win.truncateDisplay(modelData)
                        color: active ? win.p.textActive : win.p.textInactive
                        font.family: "JetBrains Mono"
                        font.pixelSize: {
                            if (win.codepointCount(modelData) > 1) return 16
                            return win.isEmoji(modelData) ? 24 : 20
                        }
                        font.weight: Font.Medium

                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                }
            }
        }
    }
}
