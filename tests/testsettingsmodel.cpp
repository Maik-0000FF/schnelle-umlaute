// Tests for SettingsModel: validators, save/load round-trip, legacy-position
// migration, and app-filter list dedup. Redirects XDG_CONFIG_HOME to a
// tempdir so save() writes there. Every setter calls save() implicitly,
// so "write value, read it back in a fresh model" is the main pattern.
//
// save() also calls reloadSchnelleUmlauteAddon() via DBus — the async
// session-bus send is a silent no-op when no bus is present, keeping the
// test hermetic on CI.

#include "SettingsModel.h"
#include "test_expect.h"
#include "test_tempdir.h"

#include <QCoreApplication>
#include <QString>
#include <QStringList>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using schnelle_umlaute_tests::TempXdgConfigHome;

namespace {

TempXdgConfigHome *g_tempdir = nullptr;

std::string configPath() {
    return g_tempdir->path() + "/fcitx5/conf/schnelle-umlaute.conf";
}

void ensureConfDir() {
    std::filesystem::create_directories(g_tempdir->path() + "/fcitx5/conf");
}

void writeConfig(const std::string &body) {
    ensureConfDir();
    std::FILE *fp = std::fopen(configPath().c_str(), "w");
    if (!fp) {
        std::fprintf(stderr, "open %s failed\n", configPath().c_str());
        std::abort();
    }
    std::fwrite(body.data(), 1, body.size(), fp);
    std::fclose(fp);
}

std::string readConfig() {
    std::FILE *fp = std::fopen(configPath().c_str(), "r");
    if (!fp)
        return {};
    std::string out;
    char buf[4096];
    // Read until a short fread tells us the stream is exhausted, then
    // exit the loop. Re-entering fread on an EOF-stream is what the
    // unix.Stream analyzer flags as undefined behaviour.
    bool streamEnded = false;
    while (!streamEnded) {
        size_t n = std::fread(buf, 1, sizeof(buf), fp);
        if (n > 0)
            out.append(buf, n);
        if (n < sizeof(buf))
            streamEnded = true;
    }
    std::fclose(fp);
    return out;
}

void resetTempdir() { g_tempdir->reset(); }

} // namespace

// -- static validators -------------------------------------------------------

// One codepoint, not whitespace. Empty is treated as "not set" and is
// allowed so the QML custom-leader field can be cleared without error.
void testIsValidLeaderKey() {
    EXPECT(SettingsModel::isValidLeaderKey(QString()));
    EXPECT(SettingsModel::isValidLeaderKey(QStringLiteral("")));
    EXPECT(SettingsModel::isValidLeaderKey(QStringLiteral(";")));
    EXPECT(SettingsModel::isValidLeaderKey(QStringLiteral("a")));
    EXPECT(SettingsModel::isValidLeaderKey(QString::fromUtf8("€")));
    // Multiple characters — rejected.
    EXPECT(!SettingsModel::isValidLeaderKey(QStringLiteral("ab")));
    // Whitespace — rejected (Space already has its own dedicated flag).
    EXPECT(!SettingsModel::isValidLeaderKey(QStringLiteral(" ")));
    EXPECT(!SettingsModel::isValidLeaderKey(QStringLiteral("\t")));
}

// Only the four known theme names are accepted. Everything else is ignored
// at load time and rejected at setTheme.
void testIsValidTheme() {
    EXPECT(SettingsModel::isValidTheme(QStringLiteral("schnelle-umlaute")));
    EXPECT(SettingsModel::isValidTheme(QStringLiteral("dark")));
    EXPECT(SettingsModel::isValidTheme(QStringLiteral("light")));
    EXPECT(SettingsModel::isValidTheme(QStringLiteral("contrast")));
    EXPECT(!SettingsModel::isValidTheme(QStringLiteral("")));
    EXPECT(!SettingsModel::isValidTheme(QStringLiteral("Dark")));
    EXPECT(!SettingsModel::isValidTheme(QStringLiteral("solarized")));
}

// Only the three OverlayPlacement enum names are accepted; anything else is
// ignored at load and rejected at the setter so editor and addon can't diverge.
void testIsValidPlacement() {
    EXPECT(SettingsModel::isValidPlacement(QStringLiteral("Grid")));
    EXPECT(SettingsModel::isValidPlacement(QStringLiteral("MouseCursor")));
    EXPECT(SettingsModel::isValidPlacement(QStringLiteral("TextCaret")));
    EXPECT(!SettingsModel::isValidPlacement(QStringLiteral("")));
    EXPECT(!SettingsModel::isValidPlacement(QStringLiteral("grid")));
    EXPECT(!SettingsModel::isValidPlacement(QStringLiteral("Gibberish")));
}

// -- default state -----------------------------------------------------------

// A fresh install (no config file) must come up with the documented defaults.
// Tightening any of these needs an intentional schema update, not an accident.
void testDefaultsOnMissingFile() {
    resetTempdir();
    SettingsModel s;
    EXPECT(s.delayLowercase() == 400);
    EXPECT(s.delayUppercase() == 700);
    EXPECT(s.delayLowercaseMin() == 0);
    EXPECT(s.delayUppercaseMin() == 0);
    EXPECT(s.leaderSpace() == true);
    EXPECT(s.leaderLeft() == false);
    EXPECT(s.leaderRight() == false);
    EXPECT(s.leaderUp() == false);
    EXPECT(s.leaderDown() == false);
    EXPECT(s.leaderAlt() == false);
    EXPECT(s.customKey1Enabled() == false);
    EXPECT(s.customKey2Enabled() == false);
    EXPECT(s.appFilterMode() == QStringLiteral("Disabled"));
    EXPECT(s.blacklist().isEmpty());
    EXPECT(s.whitelist().isEmpty());
    EXPECT(s.overlayEnabled() == false);
    EXPECT(s.overlayShowOnTrigger() == false);
    EXPECT(s.overlayPosition() == QStringLiteral("TopCol4"));
    EXPECT(s.overlayPlacement() == QStringLiteral("Grid"));
    EXPECT(s.theme() == QStringLiteral("schnelle-umlaute"));
}

// -- save/load round-trip ---------------------------------------------------

// Every scalar setter writes, the reloaded model must yield the same value.
void testScalarRoundTrip() {
    resetTempdir();
    {
        SettingsModel s;
        s.setDelayLowercase(525);
        s.setDelayUppercase(800);
        s.setDelayLowercaseMin(150);
        s.setDelayUppercaseMin(225);
        s.setLeaderSpace(false);
        s.setLeaderLeft(true);
        s.setLeaderAlt(true);
        s.setCustomKey1Enabled(true);
        s.setCustomKey1(QStringLiteral(";"));
        s.setAppFilterMode(QStringLiteral("Blacklist"));
        s.setOverlayEnabled(true);
        s.setOverlayShowOnTrigger(true);
        s.setTheme(QStringLiteral("dark"));
    }
    SettingsModel s2;
    EXPECT(s2.delayLowercase() == 525);
    EXPECT(s2.delayUppercase() == 800);
    EXPECT(s2.delayLowercaseMin() == 150);
    EXPECT(s2.delayUppercaseMin() == 225);
    EXPECT(s2.leaderSpace() == false);
    EXPECT(s2.leaderLeft() == true);
    EXPECT(s2.leaderAlt() == true);
    EXPECT(s2.customKey1Enabled() == true);
    EXPECT(s2.customKey1() == QStringLiteral(";"));
    EXPECT(s2.appFilterMode() == QStringLiteral("Blacklist"));
    EXPECT(s2.overlayEnabled() == true);
    EXPECT(s2.overlayShowOnTrigger() == true);
    EXPECT(s2.theme() == QStringLiteral("dark"));
}

void testBlacklistAddAndRoundTrip() {
    resetTempdir();
    {
        SettingsModel s;
        s.setAppFilterMode(QStringLiteral("Blacklist"));
        s.addBlacklistEntry(QStringLiteral("Kitty"));
        s.addBlacklistEntry(QStringLiteral("WezTerm"));
    }
    SettingsModel s2;
    EXPECT(s2.blacklist().size() == 2);
    EXPECT(s2.blacklist().contains(QStringLiteral("Kitty")));
    EXPECT(s2.blacklist().contains(QStringLiteral("WezTerm")));
}

void testWhitelistAddAndRemove() {
    resetTempdir();
    SettingsModel s;
    s.setAppFilterMode(QStringLiteral("Whitelist"));
    s.addWhitelistEntry(QStringLiteral("LibreOffice"));
    s.addWhitelistEntry(QStringLiteral("Firefox"));
    EXPECT(s.whitelist().size() == 2);
    s.removeWhitelistEntry(0);
    EXPECT(s.whitelist().size() == 1);
    EXPECT(s.whitelist()[0] == QStringLiteral("Firefox"));
}

// Trim-before-compare keeps sloppy input like " Kitty " from creating near-
// duplicates; empty strings are rejected so nobody can add a blank line.
void testBlacklistDedupAndTrim() {
    resetTempdir();
    SettingsModel s;
    s.addBlacklistEntry(QStringLiteral("Kitty"));
    s.addBlacklistEntry(QStringLiteral("  Kitty  "));
    s.addBlacklistEntry(QStringLiteral(""));
    s.addBlacklistEntry(QStringLiteral("   "));
    EXPECT(s.blacklist().size() == 1);
    EXPECT(s.blacklist()[0] == QStringLiteral("Kitty"));
}

// -- legacy position migration ----------------------------------------------

// v1.1 wrote a single "Position=TopCenter" (from the old 3×3 grid). v1.2
// splits into Row+Column (7×3). A file from an older version must load into
// the equivalent column on the new grid, not reset to default.
void testLegacyPositionMigration() {
    resetTempdir();
    writeConfig("[Overlay]\n"
                "Enabled=True\n"
                "Position=TopCenter\n");
    SettingsModel s;
    EXPECT(s.overlayEnabled() == true);
    EXPECT(s.overlayPosition() == QStringLiteral("TopCol4"));
}

void testLegacyPositionCornerMigration() {
    resetTempdir();
    writeConfig("[Overlay]\n"
                "Position=BottomRight\n");
    SettingsModel s;
    EXPECT(s.overlayPosition() == QStringLiteral("BottomCol7"));
}

// A value that isn't one of the 9 legacy names passes through unchanged —
// either it's already a new-style "TopCol3" or it's garbage we shouldn't
// silently rename. Leaving it intact surfaces the problem to the user.
void testUnknownPositionPassesThrough() {
    resetTempdir();
    writeConfig("[Overlay]\n"
                "Position=TopCol2\n");
    SettingsModel s;
    EXPECT(s.overlayPosition() == QStringLiteral("TopCol2"));
}

// When both old Position= and new Row/Column are present, Row+Column win —
// the new format is authoritative on anything fcitx5 1.2+ wrote.
void testNewRowColumnBeatsLegacyPosition() {
    resetTempdir();
    writeConfig("[Overlay]\n"
                "Position=TopCenter\n"
                "Row=Bottom\n"
                "Column=Col7\n");
    SettingsModel s;
    EXPECT(s.overlayPosition() == QStringLiteral("BottomCol7"));
}

// Partial new state: only Row is present → Column defaults to Col4 (center).
// Stops a malformed file from surfacing as an empty "Top" position.
void testPartialRowColumnFillsColumnDefault() {
    resetTempdir();
    writeConfig("[Overlay]\n"
                "Row=Bottom\n");
    SettingsModel s;
    EXPECT(s.overlayPosition() == QStringLiteral("BottomCol4"));
}

// -- placement: round-trip, validation, legacy migration --------------------

// A valid placement survives save/load.
void testPlacementRoundTrip() {
    resetTempdir();
    {
        SettingsModel s;
        s.setOverlayPlacement(QStringLiteral("TextCaret"));
    }
    SettingsModel s2;
    EXPECT(s2.overlayPlacement() == QStringLiteral("TextCaret"));
}

// setOverlayPlacement must reject unknown values so the UI can't persist a
// placement that load() would then ignore (same contract as setTheme).
void testSetPlacementRejectsUnknown() {
    resetTempdir();
    SettingsModel s;
    s.setOverlayPlacement(QStringLiteral("MouseCursor"));
    EXPECT(s.overlayPlacement() == QStringLiteral("MouseCursor"));
    s.setOverlayPlacement(QStringLiteral("Gibberish"));
    EXPECT(s.overlayPlacement() == QStringLiteral("MouseCursor"));
}

// A corrupt/hand-edited Placement must not overwrite the in-memory default,
// so editor and addon agree on Grid instead of silently diverging.
void testUnknownPlacementIgnoredAtLoad() {
    resetTempdir();
    writeConfig("[Overlay]\n"
                "Placement=Gibberish\n");
    SettingsModel s;
    EXPECT(s.overlayPlacement() == QStringLiteral("Grid"));
}

// Pre-enum (<=1.2.3) configs wrote AtCursor=True for the mouse mode. With no
// explicit Placement, a leftover AtCursor=True must upgrade to MouseCursor.
void testLegacyAtCursorMigratesToMouseCursor() {
    resetTempdir();
    writeConfig("[Overlay]\n"
                "AtCursor=True\n");
    SettingsModel s;
    EXPECT(s.overlayPlacement() == QStringLiteral("MouseCursor"));
}

// AtCursor=False on a legacy file leaves the Grid default untouched.
void testLegacyAtCursorFalseKeepsGrid() {
    resetTempdir();
    writeConfig("[Overlay]\n"
                "AtCursor=False\n");
    SettingsModel s;
    EXPECT(s.overlayPlacement() == QStringLiteral("Grid"));
}

// When an explicit Placement is present, a stray AtCursor=True must not
// override it — the migration only fills the Grid default. Placement is read
// before AtCursor, so the upgrade sees a non-default value and backs off.
void testExplicitPlacementBeatsLegacyAtCursor() {
    resetTempdir();
    writeConfig("[Overlay]\n"
                "Placement=TextCaret\n"
                "AtCursor=True\n");
    SettingsModel s;
    EXPECT(s.overlayPlacement() == QStringLiteral("TextCaret"));
}

// -- theme guard -------------------------------------------------------------

// A config written by a future version with an unknown theme name must not
// overwrite the in-memory default, so the UI has something valid to show.
void testUnknownThemeIgnoredAtLoad() {
    resetTempdir();
    writeConfig("[Theme]\n"
                "Theme=solarized\n");
    SettingsModel s;
    EXPECT(s.theme() == QStringLiteral("schnelle-umlaute"));
}

// setTheme must also reject unknown values so the QML UI can't save a
// theme that load() would then ignore — that would silently revert the
// user's choice on the next start.
void testSetThemeRejectsUnknown() {
    resetTempdir();
    SettingsModel s;
    s.setTheme(QStringLiteral("dark"));
    EXPECT(s.theme() == QStringLiteral("dark"));
    s.setTheme(QStringLiteral("solarized"));
    EXPECT(s.theme() == QStringLiteral("dark"));
}

// -- isActiveLeaderKey -------------------------------------------------------

// Used by the QML editor to warn when a user tries to map the same character
// that's also configured as a custom leader — no keystrokes would reach the
// mapping because the leader timeout would swallow them first.
void testIsActiveLeaderKeyRespectsEnabledFlag() {
    resetTempdir();
    SettingsModel s;
    s.setCustomKey1Enabled(false);
    s.setCustomKey1(QStringLiteral(";"));
    // Disabled → not active even if the key is set.
    EXPECT(!s.isActiveLeaderKey(QStringLiteral(";")));

    s.setCustomKey1Enabled(true);
    EXPECT(s.isActiveLeaderKey(QStringLiteral(";")));
    EXPECT(!s.isActiveLeaderKey(QStringLiteral("a")));

    s.setCustomKey2Enabled(true);
    s.setCustomKey2(QStringLiteral("j"));
    EXPECT(s.isActiveLeaderKey(QStringLiteral("j")));

    EXPECT(!s.isActiveLeaderKey(QString()));
}

// -- on-disk format guard ---------------------------------------------------

// Spot-check the shape of the file save() writes: the fcitx5 addon parses
// this with its own INI reader (FCITX_CONFIGURATION), so we need the
// bracketed sections and key=value lines to stay intact.
void testOnDiskFormatHasExpectedSections() {
    resetTempdir();
    {
        SettingsModel s;
        s.setDelayLowercase(500);
        s.setOverlayEnabled(true);
    }
    const auto raw = readConfig();
    EXPECT(raw.find("[Delay]") != std::string::npos);
    EXPECT(raw.find("Lowercase=500") != std::string::npos);
    EXPECT(raw.find("[Leader]") != std::string::npos);
    EXPECT(raw.find("[Leader/Custom]") != std::string::npos);
    EXPECT(raw.find("[AppFilter]") != std::string::npos);
    EXPECT(raw.find("[Overlay]") != std::string::npos);
    EXPECT(raw.find("Enabled=True") != std::string::npos);
    // New 1.2 keys must be written — not the legacy "Position=".
    EXPECT(raw.find("Row=") != std::string::npos);
    EXPECT(raw.find("Column=") != std::string::npos);
    EXPECT(raw.find("[Theme]") != std::string::npos);
}

// -- test runner -------------------------------------------------------------

using TestFn = void (*)();
struct TestCase {
    const char *name;
    TestFn fn;
};

const TestCase kTests[] = {
    {"testIsValidLeaderKey", testIsValidLeaderKey},
    {"testIsValidTheme", testIsValidTheme},
    {"testIsValidPlacement", testIsValidPlacement},
    {"testDefaultsOnMissingFile", testDefaultsOnMissingFile},
    {"testScalarRoundTrip", testScalarRoundTrip},
    {"testBlacklistAddAndRoundTrip", testBlacklistAddAndRoundTrip},
    {"testWhitelistAddAndRemove", testWhitelistAddAndRemove},
    {"testBlacklistDedupAndTrim", testBlacklistDedupAndTrim},
    {"testLegacyPositionMigration", testLegacyPositionMigration},
    {"testLegacyPositionCornerMigration", testLegacyPositionCornerMigration},
    {"testUnknownPositionPassesThrough", testUnknownPositionPassesThrough},
    {"testNewRowColumnBeatsLegacyPosition",
     testNewRowColumnBeatsLegacyPosition},
    {"testPartialRowColumnFillsColumnDefault",
     testPartialRowColumnFillsColumnDefault},
    {"testPlacementRoundTrip", testPlacementRoundTrip},
    {"testSetPlacementRejectsUnknown", testSetPlacementRejectsUnknown},
    {"testUnknownPlacementIgnoredAtLoad", testUnknownPlacementIgnoredAtLoad},
    {"testLegacyAtCursorMigratesToMouseCursor",
     testLegacyAtCursorMigratesToMouseCursor},
    {"testLegacyAtCursorFalseKeepsGrid", testLegacyAtCursorFalseKeepsGrid},
    {"testExplicitPlacementBeatsLegacyAtCursor",
     testExplicitPlacementBeatsLegacyAtCursor},
    {"testUnknownThemeIgnoredAtLoad", testUnknownThemeIgnoredAtLoad},
    {"testSetThemeRejectsUnknown", testSetThemeRejectsUnknown},
    {"testIsActiveLeaderKeyRespectsEnabledFlag",
     testIsActiveLeaderKeyRespectsEnabledFlag},
    {"testOnDiskFormatHasExpectedSections",
     testOnDiskFormatHasExpectedSections},
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    TempXdgConfigHome tempdir("testsettingsmodel");
    g_tempdir = &tempdir;
    for (const auto &tc : kTests) {
        tc.fn();
        std::fprintf(stderr, "ok %s\n", tc.name);
    }
    return 0;
}
