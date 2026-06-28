// Unit tests for the pure caret-theme helpers (addon/editor/caret_theme.h):
// colour normalisation, the flat classicui.conf parse/patch round-trip, and
// the generated theme.conf colour-role mapping. No file I/O, no DBus, no
// editor runtime — pure QString logic, so only Qt6::Core is needed.

#include "caret_theme.h"

#include "test_expect.h"

#include <QMap>
#include <QString>
#include <QStringList>

using schnelle_umlaute::applyIniKeys;
using schnelle_umlaute::generateCaretThemeConf;
using schnelle_umlaute::normColor;
using schnelle_umlaute::parseFlatIni;

int main() {
    // ── normColor ────────────────────────────────────────────────────────
    {
        // #rrggbb -> opaque alpha-last.
        EXPECT(normColor("#08060f") == "#08060fff");
        EXPECT(normColor("#FFD60A") == "#FFD60Aff"); // case preserved
        EXPECT(normColor("  #08060f  ") == "#08060fff"); // trimmed first
        // Qt's alpha-first #aarrggbb -> alpha-last #rrggbbaa.
        EXPECT(normColor("#cc08060f") == "#08060fcc");
        // Non-colour strings pass through unchanged.
        EXPECT(normColor("default") == "default");
        EXPECT(normColor("") == "");
    }

    // ── parseFlatIni ─────────────────────────────────────────────────────
    {
        const QString content = "# a comment\n"
                                "Theme=default\n"
                                "[SomeSection]\n" // headers are ignored (flat)
                                "UseAccentColor=True\n"
                                "\n"
                                "  Font=Sans 10  \n"  // key/value trimmed
                                "Theme=override\n";   // last value wins
        const auto m = parseFlatIni(content);
        EXPECT(m.size() == 3);
        EXPECT(m.value("Theme") == "override");
        EXPECT(m.value("UseAccentColor") == "True");
        EXPECT(m.value("Font") == "Sans 10");
        EXPECT(!m.contains("# a comment"));
        EXPECT(!m.contains("[SomeSection]"));
    }

    // ── applyIniKeys ─────────────────────────────────────────────────────
    {
        const QStringList lines = {"# user comment", "Font=Sans 11",
                                   "Theme=mytheme",
                                   "Vertical Candidate List=True"};
        const QMap<QString, QString> kv = {
            {"Theme", "schnelle-umlaute"}, {"UseAccentColor", "False"}};
        const auto out = applyIniKeys(lines, kv);

        EXPECT(out.contains("# user comment"));    // comments preserved
        EXPECT(out.contains("Font=Sans 11"));      // unrelated key preserved
        EXPECT(out.contains("Vertical Candidate List=True")); // spaced key kept
        EXPECT(out.contains("Theme=schnelle-umlaute")); // replaced in place
        EXPECT(!out.contains("Theme=mytheme"));         // old value gone
        EXPECT(out.contains("UseAccentColor=False"));   // missing key appended

        // Empty input -> every key appended, nothing else.
        const auto fresh = applyIniKeys(QStringList{}, kv);
        EXPECT(fresh.size() == 2);
        EXPECT(fresh.contains("Theme=schnelle-umlaute"));
        EXPECT(fresh.contains("UseAccentColor=False"));
    }

    // ── generateCaretThemeConf (colour-role mapping) ─────────────────────
    {
        // Distinct colours so each role can be checked independently.
        const QString conf = generateCaretThemeConf(
            "#111111",  // background -> panel
            "#222222",  // text       -> NormalColor
            "#333333",  // highlight  -> active-candidate background
            "#444444",  // onHighlight-> active-candidate text
            "#555555"); // border
        EXPECT(conf.contains("NormalColor=#222222ff"));
        EXPECT(conf.contains("HighlightCandidateColor=#444444ff"));
        EXPECT(conf.contains("HighlightBackgroundColor=#333333ff"));
        EXPECT(conf.contains("Color=#111111ff"));  // [InputPanel/Background]
        EXPECT(conf.contains("BorderColor=#555555ff"));
        EXPECT(conf.contains("Color=#333333ff"));  // [InputPanel/Highlight]
        EXPECT(conf.contains("[Metadata]"));
        EXPECT(conf.contains("[InputPanel/Highlight]"));
    }

    return 0;
}
