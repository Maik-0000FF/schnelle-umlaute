#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx/inputcontext.h>
#include <fcitx-utils/utf8.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/log.h>
#include <fcitx-config/configuration.h>
#include <fcitx-config/iniparser.h>
#include <chrono>
#include <string>
#include <unordered_map>
#include <memory>
#include <time.h>
#include <algorithm>

namespace fcitx {

// Leader key options (matching PowerToys Quick Accents)
// Using FCITX_CONFIG_ENUM to generate marshalling functions
FCITX_CONFIG_ENUM(LeaderKey, Space, LeftArrow, RightArrow, SpaceOrLeft,
                  SpaceOrRight, LeftOrRight, All);

// Custom constrain for integer with step
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

FCITX_CONFIGURATION(
    SchnelleUmlauteConfig,
    Option<int, IntConstrainWithStep> delayLowercase{this, "DelayLowercase", "Delay für Kleinbuchstaben (ms)", 400, IntConstrainWithStep(50, 2000, 25)};
    Option<int, IntConstrainWithStep> delayUppercase{this, "DelayUppercase", "Delay für Großbuchstaben (ms)", 700, IntConstrainWithStep(50, 2000, 25)};
    Option<LeaderKey> leaderKey{this, "LeaderKey", "Aktivierungstaste (Leader Key)", LeaderKey::Space};

    // 20 custom mapping slots - direct options for proper DefaultValue support
    Option<std::string> mapping1Input{this, "Mapping1Input", "Input 1", "a"};
    Option<std::string> mapping1Output{this, "Mapping1Output", "Output 1", "ä"};
    Option<std::string> mapping2Input{this, "Mapping2Input", "Input 2", "o"};
    Option<std::string> mapping2Output{this, "Mapping2Output", "Output 2", "ö"};
    Option<std::string> mapping3Input{this, "Mapping3Input", "Input 3", "u"};
    Option<std::string> mapping3Output{this, "Mapping3Output", "Output 3", "ü"};
    Option<std::string> mapping4Input{this, "Mapping4Input", "Input 4", "s"};
    Option<std::string> mapping4Output{this, "Mapping4Output", "Output 4", "ß"};
    Option<std::string> mapping5Input{this, "Mapping5Input", "Input 5", "A"};
    Option<std::string> mapping5Output{this, "Mapping5Output", "Output 5", "Ä"};
    Option<std::string> mapping6Input{this, "Mapping6Input", "Input 6", "O"};
    Option<std::string> mapping6Output{this, "Mapping6Output", "Output 6", "Ö"};
    Option<std::string> mapping7Input{this, "Mapping7Input", "Input 7", "U"};
    Option<std::string> mapping7Output{this, "Mapping7Output", "Output 7", "Ü"};
    Option<std::string> mapping8Input{this, "Mapping8Input", "Input 8", ""};
    Option<std::string> mapping8Output{this, "Mapping8Output", "Output 8", ""};
    Option<std::string> mapping9Input{this, "Mapping9Input", "Input 9", ""};
    Option<std::string> mapping9Output{this, "Mapping9Output", "Output 9", ""};
    Option<std::string> mapping10Input{this, "Mapping10Input", "Input 10", ""};
    Option<std::string> mapping10Output{this, "Mapping10Output", "Output 10", ""};
    Option<std::string> mapping11Input{this, "Mapping11Input", "Input 11", ""};
    Option<std::string> mapping11Output{this, "Mapping11Output", "Output 11", ""};
    Option<std::string> mapping12Input{this, "Mapping12Input", "Input 12", ""};
    Option<std::string> mapping12Output{this, "Mapping12Output", "Output 12", ""};
    Option<std::string> mapping13Input{this, "Mapping13Input", "Input 13", ""};
    Option<std::string> mapping13Output{this, "Mapping13Output", "Output 13", ""};
    Option<std::string> mapping14Input{this, "Mapping14Input", "Input 14", ""};
    Option<std::string> mapping14Output{this, "Mapping14Output", "Output 14", ""};
    Option<std::string> mapping15Input{this, "Mapping15Input", "Input 15", ""};
    Option<std::string> mapping15Output{this, "Mapping15Output", "Output 15", ""};
    Option<std::string> mapping16Input{this, "Mapping16Input", "Input 16", ""};
    Option<std::string> mapping16Output{this, "Mapping16Output", "Output 16", ""};
    Option<std::string> mapping17Input{this, "Mapping17Input", "Input 17", ""};
    Option<std::string> mapping17Output{this, "Mapping17Output", "Output 17", ""};
    Option<std::string> mapping18Input{this, "Mapping18Input", "Input 18", ""};
    Option<std::string> mapping18Output{this, "Mapping18Output", "Output 18", ""};
    Option<std::string> mapping19Input{this, "Mapping19Input", "Input 19", ""};
    Option<std::string> mapping19Output{this, "Mapping19Output", "Output 19", ""};
    Option<std::string> mapping20Input{this, "Mapping20Input", "Input 20", ""};
    Option<std::string> mapping20Output{this, "Mapping20Output", "Output 20", ""};
);

class SchnelleUmlauteEngine : public InputMethodEngineV2 {
public:
    SchnelleUmlauteEngine(Instance *instance)
        : instance_(instance), enabled_(true) {

        // Load configuration and build umlaut map from config
        // Defaults are set in .conf.in file
        reloadConfig();
    }

    const Configuration *getConfig() const override { return &config_; }
    void setConfig(const RawConfig &config) override {
        config_.load(config);

        // Validate and clamp values to valid range (50-2000ms)
        auto clamp = [](int value, int min, int max) {
            return std::max(min, std::min(max, value));
        };

        config_.delayLowercase.setValue(clamp(*config_.delayLowercase, 50, 2000));
        config_.delayUppercase.setValue(clamp(*config_.delayUppercase, 50, 2000));

        safeSaveAsIni(config_, "conf/schnelle-umlaute.conf");
        reloadConfig();
    }

    void reloadConfig() override {
        readAsIni(config_, "conf/schnelle-umlaute.conf");

        // Load mappings from config
        loadMappingsFromConfig();

        const char* leaderKeyName = "Unknown";
        switch (*config_.leaderKey) {
            case LeaderKey::Space: leaderKeyName = "Space"; break;
            case LeaderKey::LeftArrow: leaderKeyName = "Left Arrow"; break;
            case LeaderKey::RightArrow: leaderKeyName = "Right Arrow"; break;
            case LeaderKey::SpaceOrLeft: leaderKeyName = "Space or Left"; break;
            case LeaderKey::SpaceOrRight: leaderKeyName = "Space or Right"; break;
            case LeaderKey::LeftOrRight: leaderKeyName = "Left or Right"; break;
            case LeaderKey::All: leaderKeyName = "All (Space/Left/Right)"; break;
        }

        FCITX_INFO() << "Schnelle: Config loaded - DelayLowercase=" << *config_.delayLowercase
                     << "ms, DelayUppercase=" << *config_.delayUppercase
                     << "ms, LeaderKey=" << leaderKeyName
                     << ", Mappings=" << umlautMap_.size();
    }

    std::vector<InputMethodEntry> listInputMethods() override {
        std::vector<InputMethodEntry> methods;

        InputMethodEntry entry("schnelle-umlaute", "Schnelle Umlaute", "de", "schnelle-umlaute");
        entry.setIcon("input-keyboard")
             .setLabel("ä")
             .setConfigurable(false);

        methods.push_back(std::move(entry));

        return methods;
    }

    void keyEvent(const InputMethodEntry &entry, KeyEvent &keyEvent) override {
        if (!enabled_) {
            return; // Pass through when disabled
        }

        auto key = keyEvent.key();
        bool isPress = keyEvent.isRelease() == false;

        // Check if Leader key is pressed FIRST (before checking keyChar)
        // Arrow keys have no Unicode character, so we must check them before keyChar logic
        if (waitingKey_ && isLeaderKey(key) && isPress) {
            // Leader key pressed while waiting: check if within delay
            if (!isTimeoutExpired()) {
                // Commit umlaut
                auto umlaut = umlautMap_[*waitingKey_];
                keyEvent.inputContext()->commitString(umlaut);
                waitingKey_.reset();
                cancelTimeout();
                keyEvent.filterAndAccept();
                return;
            }
        }

        // Get character from key - convert uint32_t to string
        uint32_t unicode = Key::keySymToUnicode(key.sym());
        std::string keyChar;
        if (unicode > 0 && unicode < 0x10FFFF) {
            keyChar = utf8::UCS4ToUTF8(unicode);
        }

        if (keyChar.empty()) {
            // Handle special case for waiting key timeout
            if (waitingKey_ && isTimeoutExpired()) {
                commitWaitingKey(keyEvent.inputContext());
            }
            return;
        }

        // Check if this is an accent key (a, o, u, s, A, O, U)
        bool isAccentKey = umlautMap_.find(keyChar) != umlautMap_.end();

        if (isPress && isAccentKey) {
            // Ignore key repeat - don't reset timer
            if (waitingKey_ && *waitingKey_ == keyChar) {
                keyEvent.filterAndAccept();
                return;
            }

            // Accent key pressed: suppress and start timer
            waitingKey_ = keyChar;
            startTime_ = std::chrono::steady_clock::now();
            scheduleTimeout();
            keyEvent.filterAndAccept();
            return;
        }

        // Key released or timeout
        if (waitingKey_) {
            if (!isPress && keyChar == *waitingKey_) {
                // Waiting key released: commit normal character
                commitWaitingKey(keyEvent.inputContext());
                // DON'T filter the release event - let it pass through
                // This helps terminals like Ghostty handle the input correctly
                return;
            } else if (isTimeoutExpired()) {
                // Timeout expired: commit normal character
                commitWaitingKey(keyEvent.inputContext());
            }
        }

        // Other key pressed while waiting: output waiting key first, then pass through
        if (waitingKey_ && isPress && keyChar != *waitingKey_) {
            commitWaitingKey(keyEvent.inputContext());
            // Don't filter - let the new key pass through normally
        }
    }

    void reset(const InputMethodEntry &, InputContextEvent &event) override {
        // Reset state when switching input method or focus changes
        waitingKey_.reset();
        cancelTimeout();
    }

    void enable() {
        enabled_ = true;
    }

    void disable() {
        enabled_ = false;
        waitingKey_.reset();
        cancelTimeout();
    }

private:
    void loadMappingsFromConfig() {
        // Clear existing mappings
        umlautMap_.clear();

        // Helper lambda to add mapping if both input and output are non-empty
        auto addMapping = [this](const std::string& input, const std::string& output) {
            if (!input.empty() && !output.empty()) {
                umlautMap_[input] = output;
            }
        };

        // Load all 20 mapping slots
        addMapping(*config_.mapping1Input, *config_.mapping1Output);
        addMapping(*config_.mapping2Input, *config_.mapping2Output);
        addMapping(*config_.mapping3Input, *config_.mapping3Output);
        addMapping(*config_.mapping4Input, *config_.mapping4Output);
        addMapping(*config_.mapping5Input, *config_.mapping5Output);
        addMapping(*config_.mapping6Input, *config_.mapping6Output);
        addMapping(*config_.mapping7Input, *config_.mapping7Output);
        addMapping(*config_.mapping8Input, *config_.mapping8Output);
        addMapping(*config_.mapping9Input, *config_.mapping9Output);
        addMapping(*config_.mapping10Input, *config_.mapping10Output);
        addMapping(*config_.mapping11Input, *config_.mapping11Output);
        addMapping(*config_.mapping12Input, *config_.mapping12Output);
        addMapping(*config_.mapping13Input, *config_.mapping13Output);
        addMapping(*config_.mapping14Input, *config_.mapping14Output);
        addMapping(*config_.mapping15Input, *config_.mapping15Output);
        addMapping(*config_.mapping16Input, *config_.mapping16Output);
        addMapping(*config_.mapping17Input, *config_.mapping17Output);
        addMapping(*config_.mapping18Input, *config_.mapping18Output);
        addMapping(*config_.mapping19Input, *config_.mapping19Output);
        addMapping(*config_.mapping20Input, *config_.mapping20Output);
    }

    bool isLeaderKey(const Key &key) const {
        KeySym sym = key.sym();
        LeaderKey leader = *config_.leaderKey;

        switch (leader) {
            case LeaderKey::Space:
                return sym == FcitxKey_space;
            case LeaderKey::LeftArrow:
                return sym == FcitxKey_Left;
            case LeaderKey::RightArrow:
                return sym == FcitxKey_Right;
            case LeaderKey::SpaceOrLeft:
                return sym == FcitxKey_space || sym == FcitxKey_Left;
            case LeaderKey::SpaceOrRight:
                return sym == FcitxKey_space || sym == FcitxKey_Right;
            case LeaderKey::LeftOrRight:
                return sym == FcitxKey_Left || sym == FcitxKey_Right;
            case LeaderKey::All:
                return sym == FcitxKey_space || sym == FcitxKey_Left || sym == FcitxKey_Right;
            default:
                return sym == FcitxKey_space; // Fallback to Space
        }
    }

    bool isTimeoutExpired() const {
        if (!waitingKey_) return false;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - startTime_).count();

        // Use longer delay for uppercase letters (requires holding Shift)
        bool isUpperCase = waitingKey_->length() == 1 && std::isupper((*waitingKey_)[0]);
        int effectiveDelay = isUpperCase ? *config_.delayUppercase : *config_.delayLowercase;

        return elapsed > effectiveDelay;
    }

    void scheduleTimeout() {
        if (!waitingKey_) return;

        // Cancel existing timer
        timeoutEvent_.reset();

        // Calculate delay based on uppercase
        bool isUpperCase = waitingKey_->length() == 1 && std::isupper((*waitingKey_)[0]);
        int effectiveDelay = isUpperCase ? *config_.delayUppercase : *config_.delayLowercase;

        // Schedule timeout callback
        auto* eventLoop = &instance_->eventLoop();

        // Get current time and add delay (addTimeEvent expects absolute timestamp)
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t now_usec = static_cast<uint64_t>(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
        uint64_t target_usec = now_usec + static_cast<uint64_t>(effectiveDelay) * 1000;

        timeoutEvent_ = eventLoop->addTimeEvent(
            CLOCK_MONOTONIC,
            target_usec,  // Absolute timestamp in microseconds
            0,  // accuracy
            [this](EventSourceTime *, uint64_t) {
                // Timeout expired - commit the waiting key
                if (waitingKey_) {
                    if (auto *ic = instance_->lastFocusedInputContext()) {
                        ic->commitString(*waitingKey_);
                        waitingKey_.reset();
                    }
                }
                timeoutEvent_.reset();  // Clean up timer
                return false;  // One-shot timer
            }
        );
    }

    void cancelTimeout() {
        timeoutEvent_.reset();
    }

    void commitWaitingKey(InputContext *ic) {
        if (waitingKey_) {
            ic->commitString(*waitingKey_);
            waitingKey_.reset();
        }
        cancelTimeout();
    }

    Instance *instance_;
    bool enabled_;
    SchnelleUmlauteConfig config_;

    // Hold & Wait state
    std::optional<std::string> waitingKey_;
    std::chrono::steady_clock::time_point startTime_;
    std::unique_ptr<EventSourceTime> timeoutEvent_;

    // Umlaut mapping
    std::unordered_map<std::string, std::string> umlautMap_;
};

class SchnelleUmlauteEngineFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        return new SchnelleUmlauteEngine(manager->instance());
    }
};

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::SchnelleUmlauteEngineFactory)
