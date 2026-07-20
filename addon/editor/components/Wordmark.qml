import QtQuick
import QtQuick.Layouts
import SchnelleUmlaute

// The two-colour "Schnelle Umlaute" wordmark, shared by the header and the
// About dialog so the name (its text, colours and word gap) is defined once.
RowLayout {
    spacing: 6 // gap between the two words of the wordmark
    Text {
        text: "Schnelle"
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontStrong
        font.weight: Font.Medium
    }
    Text {
        text: "Umlaute"
        color: Theme.brand
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontStrong
        font.weight: Font.Medium
    }
}
