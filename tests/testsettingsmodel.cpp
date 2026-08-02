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
#include <QSignalSpy>
#include <QString>
#include <QStringList>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

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
    EXPECT(s.leaderSpaceReverse() == false);
    EXPECT(s.leaderLeft() == false);
    EXPECT(s.leaderRight() == false);
    EXPECT(s.leaderUp() == false);
    EXPECT(s.leaderDown() == false);
    EXPECT(s.leaderAlt() == false);
    EXPECT(s.leaderAltReverse() == false);
    EXPECT(s.leaderAltGr() == false);
    EXPECT(s.leaderAltGrReverse() == false);
    EXPECT(s.leaderLeftReverse() == false);
    EXPECT(s.leaderRightReverse() == false);
    EXPECT(s.leaderUpReverse() == false);
    EXPECT(s.leaderDownReverse() == false);
    EXPECT(s.customKey1Enabled() == false);
    EXPECT(s.customKey2Enabled() == false);
    EXPECT(s.customKey1Reverse() == false);
    EXPECT(s.customKey2Reverse() == false);
    // No key captured yet. Never invent a position for an unset leader.
    EXPECT(s.customKey1Code() == fcitx::kNoKeyCode);
    EXPECT(s.customKey2Code() == fcitx::kNoKeyCode);
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
        // Enable Left before disabling Space: the editor never lets the last
        // effective leader be turned off, so a second leader must exist first.
        s.setLeaderLeft(true);
        s.setLeaderSpace(false);
        // The "Alt forward, AltGr reverse" pairing, plus the orthogonality of
        // the enable and reverse flags, must survive the round-trip.
        s.setLeaderAlt(true);
        s.setLeaderAltGr(true);
        s.setLeaderAltGrReverse(true);
        // Reverse flags are orthogonal to the enable flags: leaderLeftReverse
        // pairs with an enabled Left, leaderDownReverse is set while Down stays
        // disabled, so the round-trip proves the flag persists on its own.
        s.setLeaderLeftReverse(true);
        s.setLeaderDownReverse(true);
        // Space carries a direction too, orthogonal to its enable: Space is
        // disabled above, yet its reverse flip must persist on its own.
        s.setLeaderSpaceReverse(true);
        s.setCustomKey1Enabled(true);
        // '#' is the character that starts a comment in this INI-ish file. It is
        // only ever written on the VALUE side (`CustomKey=#`), where it is just
        // text, but pin that: a parser that stripped it would silently blank the
        // leader's display character and its mapped-input collision check.
        // The captured physical key must survive the round-trip too: it is what
        // the addon matches and hand-classifies, so losing it would take the
        // leader and the dual split down with it. 20 = the '#' key.
        s.captureCustomKey1(QStringLiteral("#"), 20);
        // Custom-leader direction is orthogonal too: Custom 1 is enabled and
        // reversed, while Custom 2 stays disabled but still carries a reverse
        // flag, so the round-trip proves each persists on its own.
        s.setCustomKey1Reverse(true);
        s.setCustomKey2Reverse(true);
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
    EXPECT(s2.leaderSpaceReverse() == true);
    EXPECT(s2.leaderLeft() == true);
    EXPECT(s2.leaderAlt() == true);
    EXPECT(s2.leaderAltReverse() == false);
    EXPECT(s2.leaderAltGr() == true);
    EXPECT(s2.leaderAltGrReverse() == true);
    EXPECT(s2.leaderLeftReverse() == true);
    EXPECT(s2.leaderRightReverse() == false);
    EXPECT(s2.leaderUpReverse() == false);
    EXPECT(s2.leaderDownReverse() == true);
    EXPECT(s2.customKey1Enabled() == true);
    EXPECT(s2.customKey1() == QStringLiteral("#"));
    EXPECT(s2.customKey1Code() == 20);
    EXPECT(s2.customKey1Reverse() == true);
    EXPECT(s2.customKey2Enabled() == false);
    EXPECT(s2.customKey2Reverse() == true);
    EXPECT(s2.appFilterMode() == QStringLiteral("Blacklist"));
    EXPECT(s2.overlayEnabled() == true);
    EXPECT(s2.overlayShowOnTrigger() == true);
    EXPECT(s2.theme() == QStringLiteral("dark"));
}

// The editor must never let the last effective leader be turned off, and a
// custom leader counts as effective only when it is enabled AND has a key
// captured. Each refused change leaves the value on and emits
// leaderRemovalBlocked so the UI can explain the snap-back.
void testLastLeaderGuard() {
    resetTempdir();
    SettingsModel s;
    QSignalSpy blocked(&s, &SettingsModel::leaderRemovalBlocked);

    // A fresh config has only Space as a leader.
    EXPECT(s.leaderSpace() == true);
    EXPECT(s.effectiveLeaderCount() == 1);

    // Turning off the sole leader is refused: the value stays on and the block
    // is signalled.
    s.setLeaderSpace(false);
    EXPECT(s.leaderSpace() == true);
    EXPECT(s.effectiveLeaderCount() == 1);
    EXPECT(blocked.count() == 1);

    // With a second leader present, Space can be turned off.
    s.setLeaderLeft(true);
    EXPECT(s.effectiveLeaderCount() == 2);
    s.setLeaderSpace(false);
    EXPECT(s.leaderSpace() == false);
    EXPECT(s.leaderLeft() == true);
    EXPECT(s.effectiveLeaderCount() == 1);
    EXPECT(blocked.count() == 1); // an allowed change does not signal a block

    // Left is now the last one and cannot be turned off either.
    s.setLeaderLeft(false);
    EXPECT(s.leaderLeft() == true);
    EXPECT(blocked.count() == 2);

    // An enabled custom leader with NO key captured is not effective, so it does
    // not rescue the guard: turning off Left is still refused.
    s.setCustomKey1Enabled(true);
    EXPECT(s.effectiveLeaderCount() == 1);
    s.setLeaderLeft(false);
    EXPECT(s.leaderLeft() == true);
    EXPECT(blocked.count() == 3);

    // Capturing a key makes the custom leader effective; Left may now go off.
    s.captureCustomKey1(QStringLiteral("j"), 44);
    EXPECT(s.effectiveLeaderCount() == 2);
    s.setLeaderLeft(false);
    EXPECT(s.leaderLeft() == false);
    EXPECT(s.effectiveLeaderCount() == 1);

    // The custom leader is now the last effective one: disabling it is refused.
    s.setCustomKey1Enabled(false);
    EXPECT(s.customKey1Enabled() == true);
    EXPECT(s.effectiveLeaderCount() == 1);
    EXPECT(blocked.count() == 4);
}

// The same guard covers the other way a custom leader loses effectiveness:
// clearing its key (a capture that resolves to kNoKeyCode). Unassigning the sole
// effective leader's key is refused, so a future clear affordance cannot reach
// zero leaders.
void testClearLastLeaderKeyGuard() {
    resetTempdir();
    SettingsModel s;
    QSignalSpy blocked(&s, &SettingsModel::leaderRemovalBlocked);

    // Make the custom leader the sole effective leader.
    s.captureCustomKey1(QStringLiteral("j"), 44);
    s.setCustomKey1Enabled(true);
    EXPECT(s.effectiveLeaderCount() == 2); // Space + custom1
    s.setLeaderSpace(false);
    EXPECT(s.effectiveLeaderCount() == 1); // only custom1
    EXPECT(s.customKey1HasKey() == true);

    // Clearing its key would leave zero effective leaders: the capture is
    // refused, so the key and the count stay.
    s.captureCustomKey1(QString(), fcitx::kNoKeyCode);
    EXPECT(s.customKey1HasKey() == true);
    EXPECT(s.effectiveLeaderCount() == 1);
    EXPECT(blocked.count() == 1);

    // With a second leader present, the key may be cleared.
    s.setLeaderSpace(true);
    EXPECT(s.effectiveLeaderCount() == 2);
    s.captureCustomKey1(QString(), fcitx::kNoKeyCode);
    EXPECT(s.customKey1HasKey() == false);
    EXPECT(s.effectiveLeaderCount() == 1); // only Space
}

// The no-character navigation keys are named from their keycode; every other
// key (printable or the "no key" sentinel) has no special name.
void testSpecialLeaderName() {
    resetTempdir();
    SettingsModel s;
    EXPECT(s.specialLeaderName(110) == QStringLiteral("Home"));
    EXPECT(s.specialLeaderName(115) == QStringLiteral("End"));
    EXPECT(s.specialLeaderName(112) == QStringLiteral("Page Up"));
    EXPECT(s.specialLeaderName(117) == QStringLiteral("Page Down"));
    EXPECT(s.specialLeaderName(118) == QStringLiteral("Insert"));
    EXPECT(s.specialLeaderName(135) == QStringLiteral("Menu"));
    EXPECT(s.specialLeaderName(38).isEmpty()); // 'a' key, printable
    EXPECT(s.specialLeaderName(fcitx::kNoKeyCode).isEmpty());
}

// clearCustomKey unsets the captured key, but is guarded like the toggles: the
// sole effective leader's key cannot be cleared.
void testClearCustomKeyMethod() {
    resetTempdir();
    SettingsModel s;
    QSignalSpy blocked(&s, &SettingsModel::leaderRemovalBlocked);

    // Space is on, so clearing an enabled custom leader's key is allowed and
    // leaves the enable flag untouched.
    s.captureCustomKey1(QStringLiteral("j"), 44);
    s.setCustomKey1Enabled(true);
    EXPECT(s.customKey1HasKey() == true);
    s.clearCustomKey1();
    EXPECT(s.customKey1HasKey() == false);
    EXPECT(s.customKey1().isEmpty());
    EXPECT(s.customKey1Enabled() == true);

    // Make the custom leader the sole effective leader: clearing its key is
    // refused.
    s.captureCustomKey1(QStringLiteral("j"), 44);
    s.setLeaderSpace(false);
    EXPECT(s.effectiveLeaderCount() == 1);
    s.clearCustomKey1();
    EXPECT(s.customKey1HasKey() == true);
    EXPECT(blocked.count() == 1);
}

// One captured key press stores both halves of a leader together, so the file
// never holds the new keycode next to the previous key's character, and QML can
// ask hasKey instead of restating the "no key" sentinel.
void testCaptureCustomKeyRoundTrip() {
    resetTempdir();
    {
        SettingsModel s;
        s.setCustomKey1Enabled(true);
        EXPECT(s.customKey1HasKey() == false);
        s.captureCustomKey1(QStringLiteral("f"), 41);
        EXPECT(s.customKey1HasKey() == true);

        // Re-capturing replaces both halves at once.
        s.captureCustomKey1(QStringLiteral("j"), 44);
    }
    SettingsModel s2;
    EXPECT(s2.customKey1() == QStringLiteral("j"));
    EXPECT(s2.customKey1Code() == 44);
    EXPECT(s2.customKey1HasKey() == true);
}

// A hand-edited keycode that cannot name a real key reads back as "no key", so
// the editor shows it as unassigned instead of pretending it works. Both ends
// of the range matter: an out-of-range code is as unpressable as a negative one,
// and either would otherwise count as a configured leader and arm the split.
void testInvalidKeyCodeReadsAsUnassigned() {
    const char *unusable[] = {"-1", "0", "776", "99999", "notanumber"};
    for (const char *code : unusable) {
        resetTempdir();
        writeConfig(std::string("[Leader/Custom]\n"
                                "CustomKeyEnabled=True\n"
                                "CustomKey=f\n"
                                "CustomKeyCode=") +
                    code + "\n");
        SettingsModel s;
        EXPECT(s.customKey1Code() == fcitx::kNoKeyCode);
        EXPECT(s.customKey1HasKey() == false);
    }

    // The boundary itself is a key a keyboard may report and must survive.
    resetTempdir();
    writeConfig("[Leader/Custom]\n"
                "CustomKeyEnabled=True\n"
                "CustomKey=f\n"
                "CustomKeyCode=775\n");
    SettingsModel s;
    EXPECT(s.customKey1Code() == fcitx::kMaxKeyCode);
    EXPECT(s.customKey1HasKey() == true);
}

// Qt never reports CapsLock in a key event's modifiers, so a capture under
// CapsLock arrives uppercased. The label and the mapped-input collision check
// compare characters, so the stored one is folded down. Non-ASCII included: the
// engine's own fold only covers ASCII.
void testCaptureFoldsCaseIncludingNonAscii() {
    resetTempdir();
    {
        SettingsModel s;
        s.setCustomKey1Enabled(true);
        s.captureCustomKey1(QStringLiteral("F"), 41);
        EXPECT(s.customKey1() == QStringLiteral("f"));

        s.setCustomKey2Enabled(true);
        s.captureCustomKey2(QString::fromUtf8("Ä"), 48);
        EXPECT(s.customKey2() == QString::fromUtf8("ä"));
    }
    SettingsModel s2;
    EXPECT(s2.customKey1() == QStringLiteral("f"));
    EXPECT(s2.customKey2() == QString::fromUtf8("ä"));
}

// Case mapping is allowed to expand a codepoint: Turkish 'İ' (U+0130) folds to
// 'i' plus a combining dot. The stored character must stay a single codepoint
// regardless, or the editor flags a working leader as invalid. The unfolded
// character is kept in that case.
void testCaptureKeepsSingleCodepointWhenFoldExpands() {
    resetTempdir();
    const QString dottedI = QString::fromUtf8("\xC4\xB0"); // U+0130
    EXPECT(dottedI.toLower().toUcs4().size() == 2);        // the fold expands

    SettingsModel s;
    s.setCustomKey1Enabled(true);
    s.captureCustomKey1(dottedI, 31);
    EXPECT(s.customKey1().toUcs4().size() == 1);
    EXPECT(s.customKey1() == dottedI);
    EXPECT(SettingsModel::isValidLeaderKey(s.customKey1()));
    // The key itself is unaffected: matching never looks at the character.
    EXPECT(s.customKey1Code() == 31);
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

// -- automatic light/dark ----------------------------------------------------

// The derivation the editor and the daemon both run. Pure, so it can be
// checked without a desktop that actually reports a colour scheme.
void testEffectiveThemeDerivation() {
    using schnelle_umlaute::SystemScheme;
    const QString manual = QStringLiteral("nord");
    const QString light = QStringLiteral("solarized-light");
    const QString dark = QStringLiteral("dracula");

    // Off: the manual pick wins whatever the desktop says, so the setting is
    // inert until it is switched on.
    EXPECT(schnelle_umlaute::effectiveTheme(false, manual, light, dark,
                                            SystemScheme::Light) == manual);
    EXPECT(schnelle_umlaute::effectiveTheme(false, manual, light, dark,
                                            SystemScheme::Dark) == manual);

    // On: the pair decides and the manual pick is ignored, but not lost.
    EXPECT(schnelle_umlaute::effectiveTheme(true, manual, light, dark,
                                            SystemScheme::Light) == light);
    EXPECT(schnelle_umlaute::effectiveTheme(true, manual, light, dark,
                                            SystemScheme::Dark) == dark);

    // A desktop that reports nothing keeps the manual pick, so switching the
    // mode on there does not cost the user the theme they chose.
    EXPECT(schnelle_umlaute::effectiveTheme(true, manual, light, dark,
                                            SystemScheme::Unknown) == manual);

    // A hand-edited pair entry that names no known theme falls back the same
    // way instead of leaving a process on a nameless palette.
    EXPECT(schnelle_umlaute::effectiveTheme(true, manual,
                                            QStringLiteral("solarized"), dark,
                                            SystemScheme::Light) == manual);

    // Only when the manual pick is unusable too does the default step in. The
    // daemon needs that: it reads Theme= without the editor's guards.
    EXPECT(schnelle_umlaute::effectiveTheme(
               true, QStringLiteral("solarized"), light, dark,
               SystemScheme::Unknown) == schnelle_umlaute::defaultTheme());
}

// The three new keys round-trip, and an unknown pair entry is ignored at load
// exactly as an unknown Theme= is.
void testAutoThemeRoundTrip() {
    resetTempdir();
    writeConfig("[Theme]\n"
                "Theme=nord\n"
                "Auto=True\n"
                "ThemeLight=catppuccin-latte\n"
                "ThemeDark=gruvbox-dark\n");
    SettingsModel s;
    EXPECT(s.theme() == QStringLiteral("nord"));
    EXPECT(s.themeAuto());
    EXPECT(s.themeLight() == QStringLiteral("catppuccin-latte"));
    EXPECT(s.themeDark() == QStringLiteral("gruvbox-dark"));

    resetTempdir();
    writeConfig("[Theme]\n"
                "ThemeLight=solarized\n");
    SettingsModel t;
    EXPECT(!t.themeAuto());
    EXPECT(t.themeLight() == schnelle_umlaute::defaultLightTheme());
    EXPECT(t.themeDark() == schnelle_umlaute::defaultDarkTheme());
}

// Turning the automatic mode on must not touch the manual pick: it is the
// value to come back to when the mode goes off again.
void testAutoKeepsManualTheme() {
    resetTempdir();
    SettingsModel s;
    s.setTheme(QStringLiteral("nord"));
    s.setThemeAuto(true);
    EXPECT(s.theme() == QStringLiteral("nord"));
    s.setThemeAuto(false);
    EXPECT(s.theme() == QStringLiteral("nord"));
    EXPECT(s.effectiveTheme() == QStringLiteral("nord"));
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

// -- save() error reporting --------------------------------------------------

// Make every write below the conf directory fail by putting a regular FILE
// where that DIRECTORY belongs: mkpath and the subsequent open then fail with
// ENOTDIR. Deliberately not a chmod — CI runs its jobs in root containers,
// where permission bits would simply be ignored and the test would silently
// stop testing anything.
void blockConfDir() {
    const std::string dir = g_tempdir->path() + "/fcitx5/conf";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(g_tempdir->path() + "/fcitx5");
    std::FILE *fp = std::fopen(dir.c_str(), "w");
    if (!fp) {
        std::fprintf(stderr, "could not occupy %s\n", dir.c_str());
        std::abort();
    }
    std::fclose(fp);
}

void unblockConfDir() {
    std::filesystem::remove(g_tempdir->path() + "/fcitx5/conf");
    ensureConfDir();
}

// A save that cannot write reports once per cause, not once per setter.
// save() runs on every setter and the delay range slider drives its setter on
// every mouse move, so an unwritable config dir would otherwise emit the
// identical message dozens of times for a single drag. A recovered save
// re-arms the reporting, so a problem that returns is reported again instead
// of being swallowed for the rest of the session.
void testSaveErrorReportedOncePerCause() {
    resetTempdir();
    SettingsModel s;
    // Blocked only now: the constructor's load() must still see an ordinary
    // (absent) config, so this tests the write path and nothing else.
    blockConfDir();
    QSignalSpy errors(&s, &SettingsModel::errorOccurred);

    s.setDelayLowercase(410);
    EXPECT(errors.count() == 1);
    const QString cause = errors.at(0).at(0).toString();
    EXPECT(!cause.isEmpty());

    // Further failing saves carry the identical message and stay quiet.
    s.setDelayLowercase(420);
    s.setDelayLowercase(430);
    s.setDelayUppercase(710);
    EXPECT(errors.count() == 1);

    // A successful save reports nothing and clears the remembered cause.
    unblockConfDir();
    s.setDelayLowercase(440);
    EXPECT(errors.count() == 1);
    EXPECT(readConfig().find("Lowercase=440") != std::string::npos);

    // The same failure coming back is reported again.
    blockConfDir();
    s.setDelayLowercase(450);
    EXPECT(errors.count() == 2);
    EXPECT(errors.at(1).at(0).toString() == cause);
    unblockConfDir();
}

// The suppression is time-boxed, not open-ended: once the window has passed,
// the next failing save reports again. Without this, a user whose config dir
// stays unwritable would see one message and then have every later toggle fail
// in silence — the exact behaviour reportSaveError exists to prevent.
//
// Waits past the real window instead of faking a clock: elapsed time only ever
// grows, so the wait itself cannot produce a false failure. The wait is derived
// from kSaveErrorRepeatMs so it cannot drift away from the value it waits out.
//
// The suppressed step before it is not equally airtight: two consecutive
// setters more than kSaveErrorRepeatMs apart would break it. That needs a stall
// large enough to take the rest of the suite with it, so it is accepted rather
// than papered over with a test seam that would stop exercising the real clock.
void testSaveErrorRepeatsAfterWindow() {
    resetTempdir();
    SettingsModel s;
    blockConfDir();
    QSignalSpy errors(&s, &SettingsModel::errorOccurred);

    s.setDelayLowercase(410);
    EXPECT(errors.count() == 1);

    // Immediately after, still suppressed.
    s.setDelayLowercase(420);
    EXPECT(errors.count() == 1);

    std::this_thread::sleep_for(
        std::chrono::milliseconds(SettingsModel::kSaveErrorRepeatMs + 200));

    // Same cause, but the window has passed: reported again.
    s.setDelayLowercase(430);
    EXPECT(errors.count() == 2);
    EXPECT(errors.at(1).at(0).toString() == errors.at(0).at(0).toString());
    unblockConfDir();
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
    {"testLastLeaderGuard", testLastLeaderGuard},
    {"testClearLastLeaderKeyGuard", testClearLastLeaderKeyGuard},
    {"testSpecialLeaderName", testSpecialLeaderName},
    {"testClearCustomKeyMethod", testClearCustomKeyMethod},
    {"testCaptureCustomKeyRoundTrip", testCaptureCustomKeyRoundTrip},
    {"testCaptureFoldsCaseIncludingNonAscii",
     testCaptureFoldsCaseIncludingNonAscii},
    {"testCaptureKeepsSingleCodepointWhenFoldExpands",
     testCaptureKeepsSingleCodepointWhenFoldExpands},
    {"testInvalidKeyCodeReadsAsUnassigned", testInvalidKeyCodeReadsAsUnassigned},
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
    {"testEffectiveThemeDerivation", testEffectiveThemeDerivation},
    {"testAutoThemeRoundTrip", testAutoThemeRoundTrip},
    {"testAutoKeepsManualTheme", testAutoKeepsManualTheme},
    {"testIsActiveLeaderKeyRespectsEnabledFlag",
     testIsActiveLeaderKeyRespectsEnabledFlag},
    {"testOnDiskFormatHasExpectedSections",
     testOnDiskFormatHasExpectedSections},
    {"testSaveErrorReportedOncePerCause", testSaveErrorReportedOncePerCause},
    {"testSaveErrorRepeatsAfterWindow", testSaveErrorRepeatsAfterWindow},
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
