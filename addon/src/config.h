#ifndef SCHNELLE_UMLAUTE_CONFIG_H
#define SCHNELLE_UMLAUTE_CONFIG_H

// Typed configuration layer for the Schnelle Umlaute addon. All
// FCITX_CONFIGURATION blocks, enum definitions, and helper annotations
// live here so the main engine file can focus on runtime behavior.

#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>
#include <fcitx-config/rawconfig.h>

#include <string>
#include <utility>
#include <vector>

namespace fcitx {

// Delay slider bounds (ms). kDeferredCommitDelayMs is the internal wait
// between committing an Alt-cycling result and sending a following Space —
// keeps the pair ordered through XIM on terminals like WezTerm.
constexpr int kDelayMin = 50;
constexpr int kDelayMax = 2000;
constexpr int kDelayStep = 25;
constexpr int kDeferredCommitDelayMs = 5;

// Custom Option constraint: integer slider with min/max/step exposed to
// the config UI via dumpDescription. Fcitx5's built-in IntConstrain has
// no step support, so this is needed for snap-to-25 delay sliders.
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


FCITX_CONFIGURATION(
    DelayConfig,
    Option<int, IntConstrainWithStep> lowercase{this, "Lowercase", "Lowercase (ms)", 400, IntConstrainWithStep(kDelayMin, kDelayMax, kDelayStep)};
    Option<int, IntConstrainWithStep> uppercase{this, "Uppercase", "Uppercase (ms)", 700, IntConstrainWithStep(kDelayMin, kDelayMax, kDelayStep)};
);

FCITX_CONFIGURATION(
    CustomLeaderConfig,
    Option<bool> customKeyEnabled{this, "CustomKeyEnabled",
        "Custom Leader 1", false};
    OptionWithAnnotation<std::string, PlaceholderAnnotation> customKey{
        this, "CustomKey", "  \xe2\x86\xb3 Key", "",
        {}, {}, PlaceholderAnnotation("e.g. ; or #", true,
            "Single character. Must not be a mapped input key.")};
    Option<bool> customKey2Enabled{this, "CustomKey2Enabled",
        "Custom Leader 2 (hand-split)", false};
    OptionWithAnnotation<std::string, PlaceholderAnnotation> customKey2{
        this, "CustomKey2", "  \xe2\x86\xb3 Key", "",
        {}, {}, PlaceholderAnnotation("e.g. j or f", true,
            "Single character on the opposite keyboard half of Leader 1.")};
);

FCITX_CONFIGURATION(
    LeaderConfig,
    Option<bool> space{this, "Space", "Space", true};
    Option<bool> left{this, "Left", "Left Arrow", false};
    Option<bool> right{this, "Right", "Right Arrow", false};
    Option<bool> up{this, "Up", "Up Arrow", false};
    Option<bool> down{this, "Down", "Down Arrow", false};
    Option<bool> alt{this, "Alt", "Alt/AltGr", false};
    Option<CustomLeaderConfig> custom{this, "Custom", "Custom Leader Keys"};
);

FCITX_CONFIGURATION(
    MappingsConfig,
    ExternalOption editor{this, "Editor", "Mapping Editor",
        "fcitx://config/addon/schnelle-umlaute/mappings.txt"};
);

FCITX_CONFIG_ENUM(AppFilterMode, Disabled, Blacklist, Whitelist);

FCITX_CONFIGURATION(
    AppFilterConfig,
    Option<AppFilterMode> mode{this, "Mode", "Mode", AppFilterMode::Disabled};
    Option<std::vector<std::string>> blacklist{
        this, "Blacklist", "Blacklist", {}};
    Option<std::vector<std::string>> whitelist{
        this, "Whitelist", "Whitelist", {}};
);

FCITX_CONFIG_ENUM(OverlayPosition, TopLeft, TopCenter, TopRight,
                  CenterLeft, Center, CenterRight,
                  BottomLeft, BottomCenter, BottomRight);

FCITX_CONFIGURATION(
    OverlayConfig,
    Option<bool> enabled{this, "Enabled", "Enabled", false};
    Option<OverlayPosition> position{this, "Position", "Position",
                                     OverlayPosition::TopCenter};
);

FCITX_CONFIGURATION(
    SchnelleUmlauteConfig,
    Option<DelayConfig> delay{this, "Delay", "Delay"};
    Option<LeaderConfig> leader{this, "Leader", "Leader Keys"};
    Option<MappingsConfig> mappings{this, "Mappings", "Mappings"};
    Option<AppFilterConfig> appFilter{this, "AppFilter", "App Filter"};
    Option<OverlayConfig> overlay{this, "Overlay", "Overlay"};
);

} // namespace fcitx

#endif
