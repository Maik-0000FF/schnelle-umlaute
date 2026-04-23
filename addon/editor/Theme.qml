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
            brandSoft:    "#4ade8033",
            text:         "#f0fdf4",
            textMuted:    "#6b7280",
            success:      "#4ade80",
            warning:      "#fbbf24",
            error:        "#f87171",
            onAccent:     "#ffffff",
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
            brandSoft:    "#4ade8033",
            text:         "#e5e7eb",
            textMuted:    "#9ca3af",
            success:      "#4ade80",
            warning:      "#fbbf24",
            error:        "#f87171",
            onAccent:     "#ffffff",
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
            brandSoft:    "#16a34a1a",
            text:         "#0f172a",
            textMuted:    "#52525b",
            success:      "#16a34a",
            warning:      "#d97706",
            error:        "#dc2626",
            onAccent:     "#ffffff",
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
            brandSoft:    "#ffffff33",
            text:         "#ffffff",
            textMuted:    "#e5e5e5",
            success:      "#ffffff",
            warning:      "#ffd60a",
            error:        "#ffd60a",
            onAccent:     "#000000",
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
    readonly property color brandSoft:    p.brandSoft
    readonly property color text:         p.text
    readonly property color textMuted:    p.textMuted
    readonly property color success:      p.success
    readonly property color warning:      p.warning
    readonly property color error:        p.error
    readonly property color onAccent:     p.onAccent
    readonly property color switchThumb:  p.switchThumb
    readonly property color scrim:        p.scrim

    readonly property string fontFamily:     "Inter"
    readonly property string fontFamilyMono: "JetBrains Mono"

    readonly property int radiusSm: 6
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14

    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 16
    readonly property int spacingXl: 24

    readonly property int animShort: 150
    readonly property int animMed:   220

    function setCurrent(name) {
        if (palettes[name] !== undefined && current !== name) {
            current = name
        }
    }
}
