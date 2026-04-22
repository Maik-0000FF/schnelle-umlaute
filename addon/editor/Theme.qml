pragma Singleton
import QtQuick

QtObject {
    readonly property color background:   "#08060f"
    readonly property color surface:      "#12101d"
    readonly property color surfaceHover: "#1a1728"
    readonly property color border:       "#2a2640"
    readonly property color borderFocus:  "#4a3f70"
    readonly property color accent:       "#a855f7"
    readonly property color accentHover:  "#c084fc"
    readonly property color accentSoft:   "#a855f733"
    readonly property color brand:        "#4ade80"
    readonly property color brandSoft:    "#4ade8033"
    readonly property color text:         "#f0fdf4"
    readonly property color textMuted:    "#6b7280"
    readonly property color success:      "#4ade80"
    readonly property color warning:      "#fbbf24"
    readonly property color error:        "#f87171"

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
}
