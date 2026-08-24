#ifndef SCHNELLE_UMLAUTE_CONFIG_H
#define SCHNELLE_UMLAUTE_CONFIG_H

// Typed configuration layer for the Schnelle Umlaute addon. All
// FCITX_CONFIGURATION blocks, enum definitions, and helper annotations
// live here so the main engine file can focus on runtime behavior.

#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>
#include <fcitx-config/rawconfig.h>

#include "hand_classifier.h"
#include "profile_paths.h"

#include <string>
#include <utility>
#include <vector>

namespace fcitx {

// Delay slider bounds (ms). kDeferredCommitDelayMs is the internal wait
// between committing an Alt-cycling result and sending a following Space —
// keeps the pair ordered through XIM on terminals like WezTerm.
constexpr int kDelayMin = 50;
constexpr int kDelayMax = 2000;
constexpr int kDelayStep = 10;
constexpr int kDeferredCommitDelayMs = 5;

// Backstop for the cycling phase, which is otherwise ended only by the input
// key's release (see armCyclingWatchdog): how long the variant picker may sit
// completely untouched, as a multiple of the accent window. Ten windows of
// silence is far past any tapping rhythm and still short enough that a gesture
// whose release was swallowed cannot outlive the sentence being typed.
constexpr int kCyclingWatchdogFactor = 10;

// Ceiling on a whole cycling phase, as a multiple of that backstop, counted
// from the gesture's own start. Every re-arm is a claim that the gesture is
// still being driven, and one of those claims can be wrong: after a swallowed
// release the input key is physically up, yet a fresh press of it carries the
// gesture's own key code and reads as auto-repeat, because only a release
// erases that code from the held set. Such a press is swallowed and postpones
// the backstop, so a gesture that is already stuck can be fed by the very keys
// it eats. The ceiling turns any such misreading into a delay instead of a
// permanent state.
//
// It deliberately does not try to tell a driven gesture from a stuck one. That
// distinction is not available: a leader press steps the picker either way, and
// whether the user meant to step it or was reaching for an application shortcut
// is not something the addon can see. Anchoring on the last visible step would
// therefore bound the wrong case and leave the intended one unbounded.
//
// Generous on purpose, because the two failure costs are not symmetric. Cutting
// a real session short commits the character on screen, which is visible and
// correctable; letting a stuck one run swallows keystrokes silently. Ten
// backstops are a hundred accent windows, far outside any session someone
// actually holds a key through, and any key other than the leader ends the
// gesture long before it anyway.
constexpr int kCyclingWatchdogCapFactor = 10;

// The shortest window the slider offers must still leave a backstop that does
// not feel twitchy. Asserted instead of clamped at runtime: a clamp would be
// dead code, because the slider's own lower bound already decides this.
static_assert(kDelayMin * kCyclingWatchdogFactor >= 500,
              "shortest accent window leaves a twitchy cycling backstop");

// Minimum-hold lower bound (ms). The accent window is [min, max]: a leader
// that arrives before min has elapsed yields the plain character instead of
// the accent. 0 reproduces the historic behavior (no lower bound), so it is
// the default and the floor of the slider.
constexpr int kMinHoldMin = 0;

// Custom Option constraint: integer slider with min/max/step exposed to
// the config UI via dumpDescription. Fcitx5's built-in IntConstrain has
// no step support, so this is needed for snap-to-10 delay sliders.
class IntConstrainWithStep {
public:
    using Type = int;
    IntConstrainWithStep(int min, int max, int step)
        : min_(min), max_(max), step_(step) {}
    bool check(int value) const { return value >= min_ && value <= max_; }
    void dumpDescription(RawConfig &config) const {
        marshallOption(config["IntMin"], min_);
        marshallOption(config["IntMax"], max_);
        marshallOption(config["IntStep"], step_);
    }

private:
    int min_;
    int max_;
    int step_;
};

/// Annotation that sets placeholder text, optional compact mode, and tooltip.
struct PlaceholderAnnotation {
    PlaceholderAnnotation(std::string text, bool compact = false,
                          std::string tooltip = "")
        : text_(std::move(text)), compact_(compact),
          tooltip_(std::move(tooltip)) {}
    bool skipDescription() const { return false; }
    bool skipSave() const { return false; }
    void dumpDescription(RawConfig &config) const {
        config.setValueByPath("Placeholder", text_);
        if (compact_) {
            config.setValueByPath("Compact", "True");
        }
        if (!tooltip_.empty()) {
            config.setValueByPath("Tooltip", tooltip_);
        }
    }

private:
    std::string text_;
    bool compact_;
    std::string tooltip_;
};

// Each case defines an accent window [min, max] in milliseconds. "Lowercase"
// and "Uppercase" are the upper bound (max), i.e. the latest moment a leader
// can still trigger the accent, and keep their historic key names so existing
// config files round-trip unchanged. "LowercaseMin"/"UppercaseMin" are the
// lower bound (minimum hold time); they default to 0, i.e. no lower bound.
// min is meant to stay below max. The editor clamps the handles; a hand-edited
// config with min >= max is degenerate and would make the accent unreachable,
// so the engine ignores such a lower bound (see getEffectiveMinHold).
FCITX_CONFIGURATION(
    DelayConfig,
    Option<int, IntConstrainWithStep> lowercase{
        this, "Lowercase", "Lowercase (ms)", 400,
        IntConstrainWithStep(kDelayMin, kDelayMax, kDelayStep)};
    Option<int, IntConstrainWithStep> uppercase{
        this, "Uppercase", "Uppercase (ms)", 700,
        IntConstrainWithStep(kDelayMin, kDelayMax, kDelayStep)};
    Option<int, IntConstrainWithStep> lowercaseMin{
        this, "LowercaseMin", "Lowercase minimum hold (ms)", 0,
        IntConstrainWithStep(kMinHoldMin, kDelayMax, kDelayStep)};
    Option<int, IntConstrainWithStep> uppercaseMin{
        this, "UppercaseMin", "Uppercase minimum hold (ms)", 0,
        IntConstrainWithStep(kMinHoldMin, kDelayMax, kDelayStep)};);

// A custom leader is a physical key, captured as a real key press in the
// editor. That press stores two things: the keycode, which is what the engine
// matches and hand-classifies, and the character, which is only shown in the UI
// and checked against the mappings.
//
// The keycode identifies the key regardless of layout and regardless of Shift,
// which changes the character but not the key. A leader whose keycode is
// kNoKeyCode has no captured key and is inactive.
FCITX_CONFIGURATION(
    CustomLeaderConfig, Option<bool> customKeyEnabled{this, "CustomKeyEnabled",
                                                      "Custom Leader 1", false};
    OptionWithAnnotation<std::string, PlaceholderAnnotation> customKey{
        this,
        "CustomKey",
        "  \xe2\x86\xb3 Key",
        "",
        {},
        {},
        PlaceholderAnnotation(
            "e.g. ; or #", true,
            "Single character. Must not be a mapped input key.")};
    Option<int> customKeyCode{this, "CustomKeyCode", "  \xe2\x86\xb3 Key code",
                              kNoKeyCode};
    Option<bool> customKeyReverse{this, "CustomKeyReverse",
                                  "Custom Leader 1 reverses", false};
    Option<bool> customKey2Enabled{this, "CustomKey2Enabled",
                                   "Custom Leader 2 (hand-split)", false};
    OptionWithAnnotation<std::string, PlaceholderAnnotation> customKey2{
        this,
        "CustomKey2",
        "  \xe2\x86\xb3 Key",
        "",
        {},
        {},
        PlaceholderAnnotation(
            "e.g. j or f", true,
            "Single character on the opposite keyboard half of Leader 1.")};
    Option<int> customKey2Code{this, "CustomKey2Code",
                               "  \xe2\x86\xb3 Key code", kNoKeyCode};
    Option<bool> customKey2Reverse{this, "CustomKey2Reverse",
                                   "Custom Leader 2 reverses", false};);

// Per-leader cycle direction. Each directional leader carries its own
// "reverse" flag next to its enable flag: enable decides whether the key is a
// leader at all, reverse decides whether it steps backward (-1) instead of
// forward (+1). The two are orthogonal, so any mix is valid, including every
// arrow reversed. Default false keeps configs stepping forward. Every leader
// carries a direction: Space, the arrows, Alt and AltGr (enabled
// independently, Alt =
// the left Alt, AltGr = ISO_Level3_Shift / the right Alt), and each custom
// leader (its reverse flag lives in CustomLeaderConfig next to its key).
FCITX_CONFIGURATION(LeaderConfig,
                    Option<bool> space{this, "Space", "Space", true};
                    Option<bool> spaceReverse{this, "SpaceReverse",
                                              "Space reverses", false};
                    Option<bool> left{this, "Left", "Left Arrow", false};
                    Option<bool> leftReverse{this, "LeftReverse",
                                             "Left Arrow reverses", false};
                    Option<bool> right{this, "Right", "Right Arrow", false};
                    Option<bool> rightReverse{this, "RightReverse",
                                              "Right Arrow reverses", false};
                    Option<bool> up{this, "Up", "Up Arrow", false};
                    Option<bool> upReverse{this, "UpReverse",
                                           "Up Arrow reverses", false};
                    Option<bool> down{this, "Down", "Down Arrow", false};
                    Option<bool> downReverse{this, "DownReverse",
                                             "Down Arrow reverses", false};
                    Option<bool> alt{this, "Alt", "Alt", false};
                    Option<bool> altReverse{this, "AltReverse",
                                            "Alt reverses", false};
                    Option<bool> altGr{this, "AltGr", "AltGr", false};
                    Option<bool> altGrReverse{this, "AltGrReverse",
                                              "AltGr reverses", false};
                    Option<CustomLeaderConfig> custom{this, "Custom",
                                                      "Custom Leader Keys"};);

FCITX_CONFIGURATION(MappingsConfig,
                    ExternalOption editor{
                        this, "Editor", "Mapping Editor",
                        "fcitx://config/addon/schnelle-umlaute/mappings.txt"};);

FCITX_CONFIG_ENUM(AppFilterMode, Disabled, Blacklist, Whitelist);

FCITX_CONFIGURATION(
    AppFilterConfig,
    Option<AppFilterMode> mode{this, "Mode", "Mode", AppFilterMode::Disabled};
    Option<std::vector<std::string>> blacklist{
        this, "Blacklist", "Blacklist", {}};
    Option<std::vector<std::string>> whitelist{
        this, "Whitelist", "Whitelist", {}};);

// 7-column × 3-row grid. 7 columns (odd count) keeps a true center
// column for fullscreen use while giving three stops per half on a 4K
// split-screen. Col1 = far left (screen-edge anchor), Col4 = center,
// Col7 = far right. Col2/3/5/6 are placed proportionally by the overlay
// daemon using the active screen width. Split into two enums because
// FCITX_CONFIG_ENUM caps at 12 values (FCITX_FOR_EACH limit).
FCITX_CONFIG_ENUM(OverlayRow, Top, Center, Bottom);
FCITX_CONFIG_ENUM(OverlayColumn, Col1, Col2, Col3, Col4, Col5, Col6, Col7);

// Where the cycle overlay appears.
//   Grid        - the fixed Row/Column position below.
//   MouseCursor - anchored at the mouse pointer; the grid is the fallback
//                 when the compositor can't report the pointer.
//   TextCaret   - at the text input cursor where you are typing. Renders
//                 through fcitx5's input-panel candidate window (the
//                 compositor anchors it at the caret; on X11 via the client
//                 cursor rect), so it needs no layer-shell, but uses the
//                 standard candidate-window look (no custom theme, no bar).
FCITX_CONFIG_ENUM(OverlayPlacement, Grid, MouseCursor, TextCaret);

FCITX_CONFIGURATION(
    OverlayConfig, Option<bool> enabled{this, "Enabled", "Enabled", false};
    Option<bool> showOnTrigger{this, "ShowOnTrigger",
                               "Preview in the trigger window (all mapped keys)",
                               false};
    // See OverlayPlacement above. Grid/MouseCursor drive the layer-shell
    // daemon (the grid is the fallback when the compositor can't report the
    // pointer); TextCaret renders through fcitx5's input-panel candidate
    // window so it needs no layer-shell. See OverlayRenderer in
    // addon/overlay/main.cpp and showCaretOverlay in schnelle-umlaute.cpp.
    Option<OverlayPlacement> placement{this, "Placement", "Placement",
                                       OverlayPlacement::Grid};
    // Draws a timing bar above the overlay for the whole accent gesture: a
    // lead-in segment (min-hold) fills, then a window segment counts down
    // (the [min, max] leader window). Shows the overlay from key-press (t=0)
    // so the lead-in is visible. See Overlay.qml and OverlayController.
    Option<bool> progressBar{this, "ProgressBar", "Show timing progress bar",
                             false};
    Option<OverlayRow> row{this, "Row", "Vertical position", OverlayRow::Top};
    Option<OverlayColumn> column{this, "Column", "Horizontal position",
                                 OverlayColumn::Col4};);

// Behavior toggles that are neither timing, leader, nor overlay. Its own group
// so a plain bool does not sit loose in SchnelleUmlauteConfig. SortByFrequency
// reorders each key's cycling variants by how often the user commits them
// (most-used first); it is non-destructive — the stored order is untouched and
// returns when the toggle is off.
FCITX_CONFIGURATION(
    BehaviorConfig,
    Option<bool> sortByFrequency{
        this, "SortByFrequency",
        "Sort each key's variants by how often you use them", false};);

FCITX_CONFIGURATION(
    SchnelleUmlauteConfig, Option<DelayConfig> delay{this, "Delay", "Delay"};
    Option<LeaderConfig> leader{this, "Leader", "Leader Keys"};
    Option<MappingsConfig> mappings{this, "Mappings", "Mappings"};
    Option<AppFilterConfig> appFilter{this, "AppFilter", "App Filter"};
    Option<OverlayConfig> overlay{this, "Overlay", "Overlay"};
    Option<BehaviorConfig> behavior{this, "Behavior", "Behavior"};);

// Mapping profiles. A profile is a named mapping set; the active one feeds
// umlautMap_. These live in a SEPARATE file (schnelle-umlaute/profiles.conf),
// NOT in schnelle-umlaute.conf above, because the editor's SettingsModel
// rewrites the whole .conf on save and would clobber profile metadata. The
// editor's ProfileListModel owns profiles.conf; the engine only reads it.
//
// Each entry maps a display Name to a relative File under the addon's config
// dir ("mappings.txt" for the Standard profile, "profiles/<slug>.txt" for the
// rest) plus an optional SelectKey hotkey. CycleNext/CyclePrev switch through
// the list. Hotkeys are stored as plain portable combo strings (e.g.
// "Control+Alt+1"); the QML editor (which does not link fcitx-config) writes
// them by hand and the engine parses each with fcitx::Key() and matches via
// Key::check(). Plain strings keep the editor/engine-shared INI trivial,
// unlike a KeyList which serializes as a nested sub-section.
//
// CONTRACT: the INI key names below ("Name", "File", "SelectKey", "Favorite",
// and "Active"/"CycleNext"/"CyclePrev" in ProfilesConfig) plus the True/False
// bool spelling are the on-disk format. The editor's ProfileListModel reads and
// writes the exact same strings by hand (load()/save()). The FCITX_CONFIGURATION
// macro requires string literals here, so they cannot be shared as a constant;
// keep the two sides in sync when changing any key.
FCITX_CONFIGURATION(
    ProfileEntryConfig, Option<std::string> name{this, "Name", "Name", ""};
    Option<std::string> file{this, "File", "File", ""};
    Option<std::string> selectKey{this, "SelectKey", "Select shortcut", ""};
    // Marks a profile for the cycle shortcut. If any profile is a favorite the
    // cycle steps through favorites only; if none is, it steps through all.
    Option<bool> favorite{this, "Favorite", "Favorite for cycling", false};);

FCITX_CONFIGURATION(
    ProfilesConfig,
    Option<std::vector<ProfileEntryConfig>> profiles{this, "Profiles",
                                                     "Profiles", {}};
    Option<std::string> active{this, "Active", "Active profile",
                               schnelle_umlaute::kStandardProfile};
    Option<std::string> cycleNext{this, "CycleNext", "Cycle to next profile",
                                  ""};
    Option<std::string> cyclePrev{this, "CyclePrev", "Cycle to previous profile",
                                  ""};);

// What fcitx5-config-qt and the KDE KCM render when the user clicks the
// gear icon next to "Schnelle Umlaute". Exposing exactly one ExternalOption
// (and no other keys) triggers the configtool's single-external fast path
// in ConfigWidget::extractOnlyExternalCommand — the dialog is skipped and
// schnelle-umlaute-editor is launched directly.
//
// All real settings (delays, leader keys, app filter, overlay position,
// mappings) live in SchnelleUmlauteConfig above and are still read/written
// through ~/.config/fcitx5/conf/schnelle-umlaute.conf. The editor writes
// that file and triggers a DBus ReloadAddonConfig, which re-enters our
// reloadConfig() to repopulate config_.
FCITX_CONFIGURATION(ExternalEditorConfig,
                    ExternalOption editor{this, "Editor", "Open Editor",
                                          "schnelle-umlaute-editor"};);

} // namespace fcitx

#endif
