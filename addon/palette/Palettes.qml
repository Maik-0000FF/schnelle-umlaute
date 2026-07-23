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
        },
        "catppuccin-mocha": {
            background:   "#1e1e2e",
            surface:      "#313244",
            surfaceHover: "#45475a",
            border:       "#45475a",
            borderFocus:  "#cba6f7",
            accent:       "#cba6f7",
            accentHover:  "#d9c1fa",
            accentSoft:   "#cba6f733",
            brand:        "#a6e3a1",
            brandHover:   "#b9ecb4",
            brandSoft:    "#a6e3a133",
            text:         "#cdd6f4",
            textMuted:    "#a6adc8",
            success:      "#a6e3a1",
            warning:      "#f9e2af",
            error:        "#f38ba8",
            errorHover:   "#f6a3b8",
            accentText:   "#1e1e2e",
            highlight:    "#cba6f7",
            highlightText: "#1e1e2e",
            switchThumb:  "#cdd6f4",
            scrim:        "#99000000",
            active:       "#cba6f7",
            activeHover:  "#d9c1fa",
            lead:         "#89b4fa",
            leadHover:    "#a6c8fb",
            swatches:     ["#cba6f7", "#89b4fa", "#a6e3a1", "#f5c2e7"],
            mergeSources: ["#cba6f7", "#89b4fa", "#94e2d5", "#f5c2e7",
                           "#a6e3a1", "#74c7ec", "#b4befe", "#89dceb"],
            overlay: { frame: "#313244", cellInactive: "#45475a", textInactive: "#cdd6f4" }
        },
        "nord": {
            background:   "#2e3440",
            surface:      "#3b4252",
            surfaceHover: "#434c5e",
            border:       "#434c5e",
            borderFocus:  "#88c0d0",
            accent:       "#88c0d0",
            accentHover:  "#9fd0dd",
            accentSoft:   "#88c0d033",
            brand:        "#a3be8c",
            brandHover:   "#b5cca2",
            brandSoft:    "#a3be8c33",
            text:         "#eceff4",
            textMuted:    "#7b88a1",
            success:      "#a3be8c",
            warning:      "#ebcb8b",
            error:        "#bf616a",
            errorHover:   "#cf727b",
            accentText:   "#2e3440",
            highlight:    "#88c0d0",
            highlightText: "#2e3440",
            switchThumb:  "#eceff4",
            scrim:        "#99000000",
            active:       "#88c0d0",
            activeHover:  "#9fd0dd",
            lead:         "#81a1c1",
            leadHover:    "#97b2ce",
            swatches:     ["#88c0d0", "#81a1c1", "#a3be8c", "#b48ead"],
            mergeSources: ["#88c0d0", "#81a1c1", "#8fbcbb", "#5e81ac",
                           "#a3be8c", "#b48ead", "#d08770", "#d8dee9"],
            overlay: { frame: "#3b4252", cellInactive: "#434c5e", textInactive: "#eceff4" }
        },
        "gruvbox-dark": {
            background:   "#282828",
            surface:      "#3c3836",
            surfaceHover: "#504945",
            border:       "#504945",
            borderFocus:  "#fabd2f",
            accent:       "#fabd2f",
            accentHover:  "#ffd75f",
            accentSoft:   "#fabd2f33",
            brand:        "#b8bb26",
            brandHover:   "#ccce4d",
            brandSoft:    "#b8bb2633",
            text:         "#ebdbb2",
            textMuted:    "#a89984",
            success:      "#b8bb26",
            warning:      "#fe8019",
            error:        "#fb4934",
            errorHover:   "#ff6249",
            accentText:   "#282828",
            highlight:    "#fabd2f",
            highlightText: "#282828",
            switchThumb:  "#ebdbb2",
            scrim:        "#99000000",
            active:       "#fabd2f",
            activeHover:  "#ffd75f",
            lead:         "#83a598",
            leadHover:    "#9ab8ab",
            swatches:     ["#fabd2f", "#83a598", "#b8bb26", "#d3869b"],
            mergeSources: ["#83a598", "#8ec07c", "#b8bb26", "#d3869b",
                           "#fabd2f", "#458588", "#689d6a", "#b16286"],
            overlay: { frame: "#3c3836", cellInactive: "#504945", textInactive: "#ebdbb2" }
        },
        "dracula": {
            background:   "#282a36",
            surface:      "#343746",
            surfaceHover: "#44475a",
            border:       "#44475a",
            borderFocus:  "#bd93f9",
            accent:       "#bd93f9",
            accentHover:  "#cba6fb",
            accentSoft:   "#bd93f933",
            brand:        "#50fa7b",
            brandHover:   "#74fb97",
            brandSoft:    "#50fa7b33",
            text:         "#f8f8f2",
            textMuted:    "#6272a4",
            success:      "#50fa7b",
            warning:      "#f1fa8c",
            error:        "#ff5555",
            errorHover:   "#ff7070",
            accentText:   "#282a36",
            highlight:    "#bd93f9",
            highlightText: "#282a36",
            switchThumb:  "#f8f8f2",
            scrim:        "#99000000",
            active:       "#bd93f9",
            activeHover:  "#cba6fb",
            lead:         "#ff79c6",
            leadHover:    "#ff92d0",
            swatches:     ["#bd93f9", "#ff79c6", "#8be9fd", "#50fa7b"],
            mergeSources: ["#bd93f9", "#ff79c6", "#8be9fd", "#50fa7b",
                           "#ffb86c", "#6272a4"],
            overlay: { frame: "#343746", cellInactive: "#44475a", textInactive: "#f8f8f2" }
        },
        "tokyo-night": {
            background:   "#1a1b26",
            surface:      "#24283b",
            surfaceHover: "#292e42",
            border:       "#292e42",
            borderFocus:  "#7aa2f7",
            accent:       "#7aa2f7",
            accentHover:  "#a5bef9",
            accentSoft:   "#7aa2f733",
            brand:        "#9ece6a",
            brandHover:   "#b5db8b",
            brandSoft:    "#9ece6a33",
            text:         "#c0caf5",
            textMuted:    "#565f89",
            success:      "#9ece6a",
            warning:      "#e0af68",
            error:        "#f7768e",
            errorHover:   "#f98fa3",
            accentText:   "#1a1b26",
            highlight:    "#7aa2f7",
            highlightText: "#1a1b26",
            switchThumb:  "#c0caf5",
            scrim:        "#99000000",
            active:       "#7aa2f7",
            activeHover:  "#a5bef9",
            lead:         "#bb9af7",
            leadHover:    "#ccb3f9",
            swatches:     ["#7aa2f7", "#bb9af7", "#7dcfff", "#9ece6a"],
            mergeSources: ["#7aa2f7", "#bb9af7", "#7dcfff", "#9ece6a",
                           "#ff9e64", "#1abc9c", "#565f89"],
            overlay: { frame: "#24283b", cellInactive: "#292e42", textInactive: "#c0caf5" }
        },
        "rose-pine": {
            background:   "#191724",
            surface:      "#1f1d2e",
            surfaceHover: "#26233a",
            border:       "#403d52",
            borderFocus:  "#c4a7e7",
            accent:       "#c4a7e7",
            accentHover:  "#d4bef0",
            accentSoft:   "#c4a7e733",
            brand:        "#9ccfd8",
            brandHover:   "#b4dce3",
            brandSoft:    "#9ccfd833",
            text:         "#e0def4",
            textMuted:    "#908caa",
            success:      "#9ccfd8",
            warning:      "#f6c177",
            error:        "#eb6f92",
            errorHover:   "#ef86a3",
            accentText:   "#191724",
            highlight:    "#c4a7e7",
            highlightText: "#191724",
            switchThumb:  "#e0def4",
            scrim:        "#99000000",
            active:       "#c4a7e7",
            activeHover:  "#d4bef0",
            lead:         "#ebbcba",
            leadHover:    "#f0cbca",
            swatches:     ["#c4a7e7", "#ebbcba", "#9ccfd8", "#31748f"],
            mergeSources: ["#c4a7e7", "#ebbcba", "#9ccfd8", "#31748f",
                           "#eb6f92", "#908caa"],
            overlay: { frame: "#1f1d2e", cellInactive: "#26233a", textInactive: "#e0def4" }
        },
        "catppuccin-latte": {
            background:   "#eff1f5",
            surface:      "#ffffff",
            surfaceHover: "#e6e9ef",
            border:       "#ccd0da",
            borderFocus:  "#8839ef",
            accent:       "#8839ef",
            accentHover:  "#9d5cf2",
            accentSoft:   "#8839ef1a",
            brand:        "#40a02b",
            brandHover:   "#4fb838",
            brandSoft:    "#40a02b1a",
            text:         "#4c4f69",
            textMuted:    "#6c6f85",
            success:      "#40a02b",
            warning:      "#df8e1d",
            error:        "#d20f39",
            errorHover:   "#e01e46",
            accentText:   "#ffffff",
            highlight:    "#8839ef",
            highlightText: "#ffffff",
            switchThumb:  "#ffffff",
            scrim:        "#66000000",
            active:       "#8839ef",
            activeHover:  "#9d5cf2",
            lead:         "#1e66f5",
            leadHover:    "#3f7ff7",
            swatches:     ["#8839ef", "#1e66f5", "#40a02b", "#ea76cb"],
            mergeSources: ["#8839ef", "#1e66f5", "#179299", "#ea76cb",
                           "#40a02b", "#04a5e5", "#7287fd", "#fe640b"],
            overlay: { frame: "#ffffff", cellInactive: "#e6e9ef", textInactive: "#4c4f69" }
        },
        "solarized-light": {
            background:   "#eee8d5",
            surface:      "#fdf6e3",
            surfaceHover: "#e4ddc8",
            border:       "#d8d0ba",
            borderFocus:  "#268bd2",
            accent:       "#268bd2",
            accentHover:  "#3a9bde",
            accentSoft:   "#268bd21a",
            brand:        "#859900",
            brandHover:   "#9bb000",
            brandSoft:    "#8599001a",
            text:         "#073642",
            textMuted:    "#657b83",
            success:      "#859900",
            warning:      "#b58900",
            error:        "#dc322f",
            errorHover:   "#e8443f",
            accentText:   "#fdf6e3",
            highlight:    "#268bd2",
            highlightText: "#fdf6e3",
            switchThumb:  "#fdf6e3",
            scrim:        "#66000000",
            active:       "#268bd2",
            activeHover:  "#3a9bde",
            lead:         "#2aa198",
            leadHover:    "#34b3a9",
            swatches:     ["#268bd2", "#2aa198", "#859900", "#6c71c4"],
            mergeSources: ["#268bd2", "#2aa198", "#6c71c4", "#d33682",
                           "#859900", "#cb4b16"],
            overlay: { frame: "#fdf6e3", cellInactive: "#e4ddc8", textInactive: "#073642" }
        },
        "eldritch": {
            background:   "#212337",
            surface:      "#323449",
            surfaceHover: "#3d3f5a",
            border:       "#323449",
            borderFocus:  "#a48cf2",
            accent:       "#a48cf2",
            accentHover:  "#b9a6f5",
            accentSoft:   "#a48cf233",
            brand:        "#37f499",
            brandHover:   "#5ff6ab",
            brandSoft:    "#37f49933",
            text:         "#ebfafa",
            textMuted:    "#7081d0",
            success:      "#37f499",
            warning:      "#f7c67f",
            error:        "#f16c75",
            errorHover:   "#f4868e",
            accentText:   "#212337",
            highlight:    "#a48cf2",
            highlightText: "#212337",
            switchThumb:  "#ebfafa",
            scrim:        "#99000000",
            active:       "#a48cf2",
            activeHover:  "#b9a6f5",
            lead:         "#04d1f9",
            leadHover:    "#4bdcfb",
            swatches:     ["#a48cf2", "#04d1f9", "#f265b5", "#37f499"],
            mergeSources: ["#a48cf2", "#04d1f9", "#f265b5", "#37f499",
                           "#7081d0", "#82e2ff"],
            overlay: { frame: "#323449", cellInactive: "#3d3f5a", textInactive: "#ebfafa" }
        }
    })

    // Ordered list of theme ids, drives the picker order and the default.
    readonly property var ids: [
        "schnelle-umlaute", "dark", "light", "contrast",
        "catppuccin-mocha", "catppuccin-latte", "nord", "gruvbox-dark",
        "dracula", "tokyo-night", "rose-pine", "solarized-light", "eldritch"
    ]

    // Display names for the picker, keyed by id.
    readonly property var labels: ({
        "schnelle-umlaute": "Schnelle Umlaute",
        "dark": "Dark",
        "light": "Light",
        "contrast": "Contrast",
        "catppuccin-mocha": "Catppuccin Mocha",
        "catppuccin-latte": "Catppuccin Latte",
        "nord": "Nord",
        "gruvbox-dark": "Gruvbox Dark",
        "dracula": "Dracula",
        "tokyo-night": "Tokyo Night",
        "rose-pine": "Rosé Pine",
        "solarized-light": "Solarized Light",
        "eldritch": "Eldritch"
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
