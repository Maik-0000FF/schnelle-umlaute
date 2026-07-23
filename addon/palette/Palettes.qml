pragma Singleton
import QtQuick

// Single source of truth for every theme palette, shared by the editor
// (Theme.qml) and the overlay (Overlay.qml). Both live in their own QML module
// and process; before this module each kept its own copy of the palettes, which
// had to be edited in two places for every theme. Centralising them here means
// a theme is defined once.
//
// Per-theme schema:
//   - The editor's semantic tokens (background, surface, accent, brand, text,
//     warning, ...): consumed by Theme.qml.
//   - active / lead (+ their Hover variants): the theme's signature "active"
//     colour and its secondary "lead" colour, used by the delay range slider
//     and mirrored by the overlay progress bar. Most themes make the accent the
//     active colour and the brand the lead; a theme whose signature is the brand
//     (e.g. schnelle-umlaute's green) swaps them. Stored per theme so no
//     hard-coded theme-name check is needed.
//   - swatches: four representative accent colours, shown as circles in the
//     theme picker pill.
//   - mergeSources: the provenance colours for the composed merge view (chip
//     backgrounds + position badges), one hue per merged profile. Per theme so
//     the provenance cues match the theme; each set stays internally distinct
//     and avoids the theme's warning/error hue so a chip never reads as a
//     warning.
//   - overlay: only the three overlay render tokens that do NOT derive cleanly
//     from the shared tokens (frame, the inactive-cell fill and text). Everything
//     else the overlay draws is derived in overlayOf() from active/lead/border/
//     highlightText, so the overlay cells and progress bar can never drift from
//     the editor slider (both read the same active/lead).

QtObject {
    id: palettes

    // The provenance hues shared by the built-in themes (functional
    // identifiers, readable on dark and light alike, no amber/orange/yellow so
    // they never clash with the warning colour). New themes may define their
    // own set instead.
    readonly property var mergeSourcesDefault: [
        "#a855f7", "#38bdf8", "#4ade80", "#818cf8",
        "#f472b6", "#2dd4bf", "#a3e635", "#22d3ee"
    ]

    readonly property var all: ({
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
            errorHover:   "#ef4444",
            accentText:   "#ffffff",
            highlight:    "#4ade80",
            highlightText: "#08060f",
            switchThumb:  "#f0fdf4",
            scrim:        "#99000000",
            // Green is the signature/active colour here, so it takes the window
            // and the accent purple the lead (inverse of the other themes).
            active:       "#4ade80",
            activeHover:  "#86efac",
            lead:         "#a855f7",
            leadHover:    "#c084fc",
            swatches:     ["#a855f7", "#4ade80", "#38bdf8", "#f472b6"],
            mergeSources: palettes.mergeSourcesDefault,
            overlay: { frame: "#12101d", cellInactive: "#241f38", textInactive: "#4ade80" }
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
            errorHover:   "#ef4444",
            accentText:   "#ffffff",
            highlight:    "#60a5fa",
            highlightText: "#0f1115",
            switchThumb:  "#e5e7eb",
            scrim:        "#99000000",
            active:       "#60a5fa",
            activeHover:  "#93c5fd",
            lead:         "#4ade80",
            leadHover:    "#86efac",
            swatches:     ["#60a5fa", "#4ade80", "#fbbf24", "#f87171"],
            mergeSources: palettes.mergeSourcesDefault,
            overlay: { frame: "#181b22", cellInactive: "#232832", textInactive: "#e5e7eb" }
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
            errorHover:   "#b91c1c",
            accentText:   "#ffffff",
            highlight:    "#2563eb",
            highlightText: "#ffffff",
            switchThumb:  "#ffffff",
            scrim:        "#66000000",
            active:       "#2563eb",
            activeHover:  "#1d4ed8",
            lead:         "#16a34a",
            leadHover:    "#15803d",
            swatches:     ["#2563eb", "#16a34a", "#d97706", "#dc2626"],
            mergeSources: palettes.mergeSourcesDefault,
            overlay: { frame: "#ffffff", cellInactive: "#f4f4f5", textInactive: "#0f172a" }
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
            errorHover:   "#ffeb3b",
            accentText:   "#000000",
            highlight:    "#ffd60a",
            highlightText: "#000000",
            switchThumb:  "#000000",
            scrim:        "#cc000000",
            active:       "#ffd60a",
            activeHover:  "#ffeb3b",
            lead:         "#ffffff",
            leadHover:    "#ffffff",
            swatches:     ["#ffd60a", "#ffffff", "#ffeb3b", "#e5e5e5"],
            mergeSources: palettes.mergeSourcesDefault,
            overlay: { frame: "#000000", cellInactive: "#0a0a0a", textInactive: "#ffffff" }
        }
    })

    // Ordered list of theme ids, drives the picker order and the default.
    readonly property var ids: [
        "schnelle-umlaute", "dark", "light", "contrast"
    ]

    // Display names for the picker, keyed by id.
    readonly property var labels: ({
        "schnelle-umlaute": "Schnelle Umlaute",
        "dark": "Dark",
        "light": "Light",
        "contrast": "Contrast"
    })

    function has(id) { return all[id] !== undefined }
    // The full palette for an id, falling back to the default theme.
    function get(id) { return all[id] || all["schnelle-umlaute"] }
    // The overlay render sub-palette for an id, DERIVED from the shared tokens
    // so it can never drift from the editor. The active cell / progress-bar
    // window carry `active` and the bar lead-in `lead` (the same colours the
    // editor delay slider uses), the active-cell text is `highlightText`, and
    // the borders are the theme border. Only the three tokens that do not
    // derive cleanly (frame, the inactive-cell fill, the inactive-cell text)
    // are stored explicitly in each theme's `overlay` sub-object.
    function overlayOf(id) {
        var p = get(id)
        var o = p.overlay
        return {
            frame: o.frame, border: p.border,
            cellInactive: o.cellInactive, cellInactiveBorder: p.border,
            cellActive: p.active, cellActiveBorder: p.active,
            textInactive: o.textInactive, textActive: p.highlightText,
            barLead: p.lead, barWindow: p.active
        }
    }
}
