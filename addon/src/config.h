#ifndef SCHNELLE_UMLAUTE_CONFIG_H
#define SCHNELLE_UMLAUTE_CONFIG_H

// Typed configuration layer for the Schnelle Umlaute addon. All
// FCITX_CONFIGURATION blocks, enum definitions, and helper annotations
// live here so the main engine file can focus on runtime behavior.

#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>
#include <fcitx-config/rawconfig.h>

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
            "Single character on the opposite keyboard half of Leader 1.")};);

FCITX_CONFIGURATION(LeaderConfig,
                    Option<bool> space{this, "Space", "Space", true};
                    Option<bool> left{this, "Left", "Left Arrow", false};
                    Option<bool> right{this, "Right", "Right Arrow", false};
                    Option<bool> up{this, "Up", "Up Arrow", false};
                    Option<bool> down{this, "Down", "Down Arrow", false};
                    Option<bool> alt{this, "Alt", "Alt/AltGr", false};
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

FCITX_CONFIGURATION(
    SchnelleUmlauteConfig, Option<DelayConfig> delay{this, "Delay", "Delay"};
    Option<LeaderConfig> leader{this, "Leader", "Leader Keys"};
    Option<MappingsConfig> mappings{this, "Mappings", "Mappings"};
    Option<AppFilterConfig> appFilter{this, "AppFilter", "App Filter"};
    Option<OverlayConfig> overlay{this, "Overlay", "Overlay"};);

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
FCITX_CONFIGURATION(
    ProfileEntryConfig, Option<std::string> name{this, "Name", "Name", ""};
    Option<std::string> file{this, "File", "File", ""};
    Option<std::string> selectKey{this, "SelectKey", "Select shortcut", ""};);

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
