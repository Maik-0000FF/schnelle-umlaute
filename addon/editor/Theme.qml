pragma Singleton
import QtQuick

QtObject {
    id: theme

    property string current: "schnelle-umlaute"

    readonly property var palettes: ({
        "schnelle-umlaute": {
            background:   "#08060f",
            surface:      "#12101d",
            surfaceHover: "#1a1728",
            border:       "#2a2640",
            borderFocus:  "#4a3f70",
            accent:       "#a855f7",
            accentHover:  "#c084fc",
            accentSoft:   "#a855f733",
            brand:        "#4ade80",
            brandHover:   "#86efac",
            brandSoft:    "#4ade8033",
            text:         "#f0fdf4",
            textMuted:    "#6b7280",
            success:      "#4ade80",
            warning:      "#fbbf24",
            error:        "#f87171",
            onAccent:     "#ffffff",
            highlight:    "#4ade80",
            onHighlight:  "#08060f",
            switchThumb:  "#f0fdf4",
            scrim:        "#99000000"
        },
        "dark": {
            background:   "#0f1115",
            surface:      "#181b22",
            surfaceHover: "#232832",
            border:       "#2a2f3a",
            borderFocus:  "#3f4654",
            accent:       "#60a5fa",
            accentHover:  "#93c5fd",
            accentSoft:   "#60a5fa33",
            brand:        "#4ade80",
            brandHover:   "#86efac",
            brandSoft:    "#4ade8033",
            text:         "#e5e7eb",
            textMuted:    "#9ca3af",
            success:      "#4ade80",
            warning:      "#fbbf24",
            error:        "#f87171",
            onAccent:     "#ffffff",
            highlight:    "#60a5fa",
            onHighlight:  "#0f1115",
            switchThumb:  "#e5e7eb",
            scrim:        "#99000000"
        },
        "light": {
            background:   "#ececef",
            surface:      "#ffffff",
            surfaceHover: "#dfdfe3",
            border:       "#d4d4d8",
            borderFocus:  "#a1a1aa",
            accent:       "#2563eb",
            accentHover:  "#1d4ed8",
            accentSoft:   "#2563eb1a",
            brand:        "#16a34a",
            brandHover:   "#15803d",
            brandSoft:    "#16a34a1a",
            text:         "#0f172a",
            textMuted:    "#52525b",
            success:      "#16a34a",
            warning:      "#d97706",
            error:        "#dc2626",
            onAccent:     "#ffffff",
            highlight:    "#2563eb",
            onHighlight:  "#ffffff",
            switchThumb:  "#ffffff",
            scrim:        "#66000000"
        },
        "contrast": {
            background:   "#000000",
            surface:      "#0a0a0a",
            surfaceHover: "#1a1a1a",
            border:       "#ffffff",
            borderFocus:  "#ffd60a",
            accent:       "#ffd60a",
            accentHover:  "#ffeb3b",
            accentSoft:   "#ffd60a33",
            brand:        "#ffffff",
            brandHover:   "#ffffff",
            brandSoft:    "#ffffff33",
            text:         "#ffffff",
            textMuted:    "#e5e5e5",
            success:      "#ffffff",
            warning:      "#ffd60a",
            error:        "#ffd60a",
            onAccent:     "#000000",
            highlight:    "#ffd60a",
            onHighlight:  "#000000",
            switchThumb:  "#000000",
            scrim:        "#cc000000"
        }
    })

    readonly property var p: palettes[current] || palettes["schnelle-umlaute"]

    readonly property color background:   p.background
    readonly property color surface:      p.surface
    readonly property color surfaceHover: p.surfaceHover
    readonly property color border:       p.border
    readonly property color borderFocus:  p.borderFocus
    readonly property color accent:       p.accent
    readonly property color accentHover:  p.accentHover
    readonly property color accentSoft:   p.accentSoft
    readonly property color brand:        p.brand
    readonly property color brandHover:   p.brandHover
    readonly property color brandSoft:    p.brandSoft
    readonly property color text:         p.text
    readonly property color textMuted:    p.textMuted
    readonly property color success:      p.success
    readonly property color warning:      p.warning
    readonly property color error:        p.error
    readonly property color onAccent:     p.onAccent
    // Active-selection colours, mirrored from the overlay's cellActive /
    // textActive (the signature theme highlights in green, not the accent).
    readonly property color highlight:    p.highlight
    readonly property color onHighlight:  p.onHighlight
    // Delay range slider role colours: the active window vs the dead-time
    // (lead). Normally the window carries the accent and the lead the brand
    // green. In schnelle-umlaute green IS the signature/active colour, so the
    // window takes the green and the lead the accent there, keeping the active
    // part on the theme's identity colour like every other theme does. The
    // overlay's progress bar (Overlay.qml barLead/barWindow) mirrors this same
    // per-theme swap, so the editor slider and the overlay bar always agree on
    // which segment is which colour.
    readonly property color sliderWindow:
        current === "schnelle-umlaute" ? brand : accent
    readonly property color sliderWindowHover:
        current === "schnelle-umlaute" ? brandHover : accentHover
    readonly property color sliderLead:
        current === "schnelle-umlaute" ? accent : brand
    readonly property color sliderLeadHover:
        current === "schnelle-umlaute" ? accentHover : brandHover
    readonly property color switchThumb:  p.switchThumb
    readonly property color scrim:        p.scrim

    // Resolve to the first installed family from a preference list rather than
    // hard-coding one. Inter and JetBrains Mono are preferred (the branded
    // look) but neither ships on a default install; when absent, fontconfig
    // would substitute an arbitrary face whose metrics break the layout (e.g.
    // on Linux Mint). Picking a known system UI/mono font ourselves keeps it
    // predictable. The trailing generic alias is always resolvable. (font.family
    // takes a single string; font.families plural is not assignable in Qt 6.4,
    // which the editor still targets, so we resolve to one name here.)
    function pickFamily(candidates) {
        const avail = Qt.fontFamilies()
        for (let i = 0; i < candidates.length; i++)
            if (avail.indexOf(candidates[i]) >= 0)
                return candidates[i]
        return candidates[candidates.length - 1]
    }
    readonly property string fontFamily: pickFamily(
        ["Inter", "Cantarell", "Noto Sans", "Ubuntu", "DejaVu Sans", "sans-serif"])
    // The mono candidate list is mirrored in addon/overlay/Overlay.qml's
    // pickFamily (separate module/process); keep both in sync.
    readonly property string fontFamilyMono: pickFamily(
        ["JetBrains Mono", "Noto Sans Mono", "DejaVu Sans Mono", "Liberation Mono", "monospace"])

    // Type scale: the single source for every text size in the editor. Roles,
    // not raw pixels, so a size change is one edit here. Glyphs get their own
    // token (fontIcon) so action icons can be rescaled independently of text.
    readonly property int fontCaption: 11  // hints, descriptions, status, small labels
    readonly property int fontBody:    12  // body text, control labels, input fields
    readonly property int fontIcon:    14  // action-glyph size (✎ ✗ 🗑 ✓ ★ ✕ ⠿ →)
    readonly property int fontStrong:  15  // mono mapping cells, dialog/app titles
    readonly property int fontDisplay: 18  // large add-card display elements
    readonly property int fontHero:    28  // empty-state hero glyph

    readonly property int radiusSm: 6
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14

    // Control-height ladder: one source so buttons, fields and rows line up.
    // controlHeight is the standard single-line control (combo box, dropdown
    // header, standard buttons, input cells); Sm for compact buttons, Lg for
    // the primary action button and tall two-line rows, rowHeight for the
    // selectable list rows (profile / app-list entries).
    readonly property int controlHeightSm: 30
    readonly property int controlHeight:   34
    readonly property int rowHeight:       36
    readonly property int controlHeightLg: 40

    // Width of a shortcut-capture field, wide enough to show a longer combo
    // (e.g. "Control+Alt+Super+J") without eliding, including the always-
    // reserved clear-button slot. Shared by the per-profile select-key field
    // and the cycle fields.
    readonly property int shortcutFieldWidth: 184

    readonly property int spacingXxs: 2
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 16
    readonly property int spacingXl: 24

    readonly property int animShort: 150
    readonly property int animMed:   220

    // Action icon glyphs, one source so the editor uses a consistent set.
    // iconCancel (✗, abort an edit) and iconClear (✕, empty a field) are
    // intentionally distinct glyphs for their distinct meanings.
    readonly property string iconCheck:       "✓"
    readonly property string iconEdit:        "✎"
    readonly property string iconTrash:       "🗑"
    readonly property string iconCancel:      "✗"
    readonly property string iconClear:       "✕"
    readonly property string iconStar:        "★"
    readonly property string iconAdd:         "+"

    function setCurrent(name) {
        if (palettes[name] !== undefined && current !== name) {
            current = name
        }
    }
}
