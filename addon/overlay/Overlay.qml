import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import SchnelleUmlauteOverlay

Window {
    id: win
    // WindowTransparentForInput sets an empty wl_surface input region, so the
    // overlay never captures the pointer: clicks fall through to the window
    // below and, crucially in cursor mode where the panel sits right under the
    // pointer, hiding it doesn't leave the cursor unpainted until the next
    // motion event. The overlay is a passive indicator (keyboard already off
    // via KeyboardInteractivityNone), so it needs no input at all.
    flags: Qt.FramelessWindowHint | Qt.WindowTransparentForInput
    color: "transparent"
    // Window grows up, and to the right only when the bar is longer than the
    // panel, to hold the optional progress bar. The bar is left-aligned with the
    // panel's top edge; the panel stays bottom-left at its own size so it never
    // reflows. With no bar the window is exactly the frame.
    width: Math.max(frame.implicitWidth,
                    OverlayController.progressActive ? win.progressBarWidth : 0)
    height: frame.implicitHeight + (OverlayController.progressActive
                                    ? win.progressBarHeight + win.progressBarGap
                                    : 0)
    // Panel width, read by the daemon to centre the panel (not the wider
    // panel+bar surface) on a grid column.
    readonly property int frameWidth: frame.implicitWidth
    // Start hidden so main() can configure the layer-shell surface
    // (layer/anchors/screen) before the first commit. main() then calls
    // show() once the surface role is fully set up.
    visible: false

    // Single source of truth for the panel background opacity. All four
    // themes share this value so a future tweak is one line, not four
    // embedded alpha bytes in the palette hex strings. Cell colors,
    // borders and text stay fully opaque — only the frame fades.
    readonly property real frameOpacity: 0.75

    // Animation constants — keep every color / border transition (frame,
    // cells, text) at the same duration so the active-cell handover and
    // theme switches feel like one motion rather than three offset ones.
    readonly property int animationDuration: 120

    // Layout constants — `cellSize` is the 44 px referenced in the
    // truncateDisplay comment below ("Three codepoints fit in JetBrains
    // Mono at pixelSize 16 (≈9.6 px each)" against a 44 px cell).
    // `framePadding` is per-side: the panel rectangle's implicitWidth
    // adds 2 × framePadding to the row width. `cellTextInset` is the
    // total horizontal slack the glyph Text leaves inside the cell (half
    // per side); it bounds the Text width that Text.HorizontalFit fits to.
    readonly property int cellSize: 44
    readonly property int cellRadius: 10
    readonly property int framePadding: 16
    readonly property int cellTextInset: 8
    // Horizontal padding (per side) inside the profile-name pill, so the
    // full-width name token has breathing room like a glyph cell does.
    readonly property int labelPadding: 14

    // Progress bar (opt-in via [Overlay]/ProgressBar). It starts at the panel's
    // top-right corner and runs to the right. Its pixel length and the
    // lead/window split come from progress_overlay_geometry.h via
    // OverlayController (one tested source, shared with the daemon's grid
    // placement); only the bar's visual height/gap/radius live here.
    readonly property int progressBarHeight: 6
    readonly property int progressBarRadius: 3
    readonly property int progressBarGap: 8

    readonly property int progressLead: OverlayController.progressLeadMs
    readonly property int progressWindow: OverlayController.progressWindowMs
    readonly property int progressTotal: progressLead + progressWindow
    readonly property int progressBarWidth:
        OverlayController.progressBarLength(progressTotal, Screen.width)
    readonly property real progressLeadWidth:
        OverlayController.progressLeadLength(progressBarWidth, progressLead,
                                             progressTotal)
    readonly property real progressWindowWidth:
        progressBarWidth - progressLeadWidth
    // Single time-based driver for the bar: the gesture's elapsed position in
    // milliseconds across the whole [0, total] timeline. The FrameAnimation
    // below re-samples it from the engine's monotonic clock every frame (see
    // progressElapsedNowMs) so it can't drift from the real window; the
    // lead/window fills derive their widths from it.
    property real progressNow: 0
    // True once the lead-in has elapsed and the leader window is open; drives
    // the panel reveal (the cells appear only when the window opens, not during
    // the lead-in). Derived from progressNow so a latency-skipped lead reveals
    // the panel immediately instead of replaying the lead.
    readonly property bool progressWindowPhase: progressNow >= progressLead

    // Font sizes per variant glyph type. Color-emoji fonts occupy a
    // smaller fraction of the em-box than JetBrains Mono at the same
    // pixelSize, so emoji bumps to 24; single ASCII letters stay at 20;
    // multi-codepoint truncations shrink to 16 to fit "xy…" inside the
    // 44 px cell.
    readonly property int pixelSizeSingle: 20
    readonly property int pixelSizeMulti: 16
    readonly property int pixelSizeEmoji: 24

    // Resolve the mono family to the first installed candidate. JetBrains Mono
    // is preferred (the metrics the cell sizing and truncateDisplay budget are
    // tuned to), but it does not ship by default, so fall back to the common
    // system monos and finally the generic alias fontconfig always resolves. A
    // wider fallback face can't overflow the fixed cell because the cell Text
    // uses Text.HorizontalFit (pixelSize becomes a max, the glyph shrinks to
    // fit). font.family takes a single string, so we resolve to one name here.
    //
    // pickFamily mirrors addon/editor/Theme.qml's resolver; the overlay is a
    // separate QML module and process (the same reason the palettes below are
    // inlined), so the logic is duplicated rather than shared. Keep the
    // candidate list in sync with Theme.qml's fontFamilyMono, both are tuned
    // to JetBrains Mono metrics.
    function pickFamily(candidates) {
        const avail = Qt.fontFamilies()
        for (let i = 0; i < candidates.length; i++)
            if (avail.indexOf(candidates[i]) >= 0)
                return candidates[i]
        return candidates[candidates.length - 1]
    }
    readonly property string fontFamilyMono: pickFamily(
        ["JetBrains Mono", "Noto Sans Mono", "DejaVu Sans Mono", "Liberation Mono", "monospace"])

    // Palettes mirror addon/editor/Theme.qml. Inlined because the overlay
    // lives in its own QML module and process — sharing a singleton would
    // cost more build plumbing than the 4 small dicts are worth.
    // `frame` stores RGB only; the panel applies frameOpacity at render
    // time via Qt.alpha().
    readonly property var palettes: ({
        "schnelle-umlaute": {
            frame: "#12101d", border: "#2a2640",
            cellInactive: "#241f38", cellInactiveBorder: "#2a2640",
            cellActive: "#4ade80", cellActiveBorder: "#4ade80",
            // Inactive cell text carries the theme's signature green; the active
            // cell keeps dark text on the green fill.
            textInactive: "#4ade80", textActive: "#08060f",
            // Green is this theme's signature/active colour (cellActive above),
            // so the active leader window (barWindow) carries the green and the
            // dead-time lead-in (barLead) the accent purple, the inverse of the
            // other themes. This mirrors Theme.qml's sliderWindow/sliderLead swap
            // so the editor slider and the overlay bar agree on which segment is
            // which colour.
            barLead: "#a855f7", barWindow: "#4ade80"
        },
        "dark": {
            frame: "#181b22", border: "#2a2f3a",
            cellInactive: "#232832", cellInactiveBorder: "#2a2f3a",
            cellActive: "#60a5fa", cellActiveBorder: "#60a5fa",
            textInactive: "#e5e7eb", textActive: "#0f1115",
            barLead: "#4ade80", barWindow: "#60a5fa"
        },
        "light": {
            frame: "#ffffff", border: "#d4d4d8",
            cellInactive: "#f4f4f5", cellInactiveBorder: "#d4d4d8",
            cellActive: "#2563eb", cellActiveBorder: "#2563eb",
            textInactive: "#0f172a", textActive: "#ffffff",
            barLead: "#16a34a", barWindow: "#2563eb"
        },
        "contrast": {
            frame: "#000000", border: "#ffffff",
            cellInactive: "#0a0a0a", cellInactiveBorder: "#ffffff",
            cellActive: "#ffd60a", cellActiveBorder: "#ffd60a",
            textInactive: "#ffffff", textActive: "#000000",
            barLead: "#ffffff", barWindow: "#ffd60a"
        }
    })
    readonly property var p: palettes[OverlayController.theme]
                             || palettes["schnelle-umlaute"]

    // Cell-text colours read through accessors with a fallback to an always-
    // defined palette key, so a palette that omits textInactive/textActive
    // degrades to a visible on-theme colour instead of silently rendering
    // black (undefined coerces to #000000). Inactive text falls back to the
    // accent (visible on the dark inactive cell), active text to the frame
    // (dark, visible on the bright active cell).
    readonly property color textActiveColor: p.textActive || p.frame
    readonly property color textInactiveColor: p.textInactive || p.cellActive

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

    // Progress bar starting at the panel's top-left corner, running right
    // (progress mode only). Phase 1: the lead segment (green) grows from the
    // corner to the right over the min-hold. Phase 2: the window segment
    // (accent) appears full past the lead, then its right end recedes left as
    // [min, max] counts down. The panel is hidden during phase 1 and revealed
    // when the window opens.
    Item {
        id: progressSlot
        visible: OverlayController.progressActive
        anchors.top: parent.top
        anchors.left: parent.left
        width: win.progressBarWidth
        height: win.progressBarHeight

        Rectangle {
            id: leadFill
            x: 0
            height: parent.height
            radius: win.progressBarRadius
            color: win.p.barLead
            // Grows from the corner over the lead-in, then stays full. A binding
            // on the shared timeline, so it tracks progressNow exactly.
            width: win.progressLeadWidth
                   * Math.max(0, Math.min(1, win.progressNow
                                             / Math.max(1, win.progressLead)))
        }
        Rectangle {
            id: windowFill
            x: win.progressLeadWidth
            height: parent.height
            radius: win.progressBarRadius
            color: win.p.barWindow
            // Zero until the lead is over, then full and receding to 0 as the
            // window counts down across [lead, total].
            width: win.progressNow < win.progressLead
                   ? 0
                   : win.progressWindowWidth
                     * Math.max(0, Math.min(1,
                         (win.progressTotal - win.progressNow)
                         / Math.max(1, win.progressWindow)))
        }

        // Sample the engine's real elapsed time every frame rather than letting
        // one NumberAnimation interpolate the whole timeline. That animation
        // began a render frame or two after SetProgress arrived yet ran the full
        // remaining duration, so it finished late and left the window-fill open
        // by a sliver right at the closing edge, the mismatch visible at the
        // boundary. FrameAnimation ties the update to the render loop and reads
        // progressElapsedNowMs() (the shared monotonic clock) each frame, so the
        // bar tracks the real timeline with only sub-frame, non-cumulative
        // error. Stopped while frozen so a caught window holds the bar in place.
        FrameAnimation {
            running: OverlayController.progressActive
                     && !OverlayController.progressFrozen
            onTriggered: win.progressNow =
                OverlayController.progressElapsedNowMs()
        }

        // Snap to the arrival-time elapsed the instant a gesture starts, so the
        // bar never shows one stale frame from the previous gesture before the
        // FrameAnimation's first tick. Only on a fresh, unfrozen start; a freeze
        // must leave progressNow where it is.
        Connections {
            target: OverlayController
            function onProgressChanged() {
                if (OverlayController.progressActive
                    && !OverlayController.progressFrozen)
                    win.progressNow = OverlayController.progressElapsedMs
            }
        }
    }

    Rectangle {
        id: frame
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        // In progress mode the panel is hidden during the lead-in and fades in
        // when the window opens; it keeps its layout slot (opacity, not visible)
        // so the bar can anchor to its top-right corner. Always shown otherwise.
        opacity: OverlayController.progressActive
                 ? (win.progressWindowPhase ? 1 : 0)
                 : 1
        Behavior on opacity { NumberAnimation { duration: win.animationDuration } }
        color: Qt.alpha(win.p.frame, win.frameOpacity)
        radius: 16
        border.color: win.p.border
        border.width: 1
        implicitWidth: (OverlayController.label ? labelPill.implicitWidth
                                                : row.implicitWidth)
                       + 2 * win.framePadding
        implicitHeight: 64

        Behavior on color { ColorAnimation { duration: win.animationDuration } }
        Behavior on border.color { ColorAnimation { duration: win.animationDuration } }

        // Profile-switch name: a single full-width token rendered in the same
        // cell styling as the active mapped glyph (dark text on the bright
        // active-cell colour), so it reads as one of the overlay's tokens
        // rather than dark text floating on the panel background.
        Rectangle {
            id: labelPill
            visible: OverlayController.label
            anchors.centerIn: parent
            radius: win.cellRadius
            color: win.p.cellActive
            border.color: win.p.cellActiveBorder
            border.width: 1
            implicitWidth: labelText.width + 2 * win.labelPadding
            implicitHeight: win.cellSize

            Behavior on color { ColorAnimation { duration: win.animationDuration } }
            Behavior on border.color { ColorAnimation { duration: win.animationDuration } }

            Text {
                id: labelText
                anchors.centerIn: parent
                text: OverlayController.variants.length
                      ? OverlayController.variants[0] : ""
                color: win.textActiveColor
                font.family: win.fontFamilyMono
                font.pixelSize: win.pixelSizeSingle
                font.weight: Font.Medium
                // Full name; only a very long one elides so the pill (plus its
                // padding and the frame padding) still fits the screen.
                width: Math.min(implicitWidth,
                                Screen.width - 4 * win.framePadding
                                - 2 * win.labelPadding)
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                Behavior on color { ColorAnimation { duration: win.animationDuration } }
            }
        }

        RowLayout {
            id: row
            visible: !OverlayController.label
            anchors.centerIn: parent
            spacing: 8

            Repeater {
                model: OverlayController.variants
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    readonly property bool active: index === OverlayController.currentIndex
                    width: win.cellSize
                    height: win.cellSize
                    radius: win.cellRadius
                    color: active ? win.p.cellActive : win.p.cellInactive
                    border.color: active ? win.p.cellActiveBorder : win.p.cellInactiveBorder
                    border.width: 1

                    Behavior on color { ColorAnimation { duration: win.animationDuration } }
                    Behavior on border.color { ColorAnimation { duration: win.animationDuration } }

                    Text {
                        anchors.centerIn: parent
                        // Bound the text to the cell (minus a small inset) and
                        // let it shrink to fit. With JetBrains Mono present the
                        // glyph already fits, so pixelSize stays as set and the
                        // look is unchanged; with a wider fallback mono the fit
                        // mode scales it down instead of spilling out of the cell.
                        width: win.cellSize - win.cellTextInset
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        fontSizeMode: Text.HorizontalFit
                        text: win.truncateDisplay(modelData)
                        color: active ? win.textActiveColor : win.textInactiveColor
                        font.family: win.fontFamilyMono
                        font.pixelSize: {
                            if (win.codepointCount(modelData) > 1) return win.pixelSizeMulti
                            return win.isEmoji(modelData) ? win.pixelSizeEmoji : win.pixelSizeSingle
                        }
                        font.weight: Font.Medium

                        Behavior on color { ColorAnimation { duration: win.animationDuration } }
                    }
                }
            }
        }
    }
}
