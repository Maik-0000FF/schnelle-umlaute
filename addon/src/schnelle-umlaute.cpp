#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx-utils/textformatflags.h>
#include <fcitx-utils/utf8.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/log.h>
#include <fcitx-config/configuration.h>
#include <fcitx-config/iniparser.h>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <time.h>
#include <algorithm>

namespace fcitx {

// Leader key options (matching PowerToys Quick Accents)
FCITX_CONFIG_ENUM(LeaderKey, Space, LeftArrow, RightArrow, SpaceOrLeft,
                  SpaceOrRight, LeftOrRight, All);

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
    Option<std::string> mapping21Input{this, "Mapping21Input", "Input 21", ""};
    Option<std::string> mapping21Output{this, "Mapping21Output", "Output 21", ""};
    Option<std::string> mapping22Input{this, "Mapping22Input", "Input 22", ""};
    Option<std::string> mapping22Output{this, "Mapping22Output", "Output 22", ""};
    Option<std::string> mapping23Input{this, "Mapping23Input", "Input 23", ""};
    Option<std::string> mapping23Output{this, "Mapping23Output", "Output 23", ""};
    Option<std::string> mapping24Input{this, "Mapping24Input", "Input 24", ""};
    Option<std::string> mapping24Output{this, "Mapping24Output", "Output 24", ""};
    Option<std::string> mapping25Input{this, "Mapping25Input", "Input 25", ""};
    Option<std::string> mapping25Output{this, "Mapping25Output", "Output 25", ""};
    Option<std::string> mapping26Input{this, "Mapping26Input", "Input 26", ""};
    Option<std::string> mapping26Output{this, "Mapping26Output", "Output 26", ""};
    Option<std::string> mapping27Input{this, "Mapping27Input", "Input 27", ""};
    Option<std::string> mapping27Output{this, "Mapping27Output", "Output 27", ""};
    Option<std::string> mapping28Input{this, "Mapping28Input", "Input 28", ""};
    Option<std::string> mapping28Output{this, "Mapping28Output", "Output 28", ""};
    Option<std::string> mapping29Input{this, "Mapping29Input", "Input 29", ""};
    Option<std::string> mapping29Output{this, "Mapping29Output", "Output 29", ""};
    Option<std::string> mapping30Input{this, "Mapping30Input", "Input 30", ""};
    Option<std::string> mapping30Output{this, "Mapping30Output", "Output 30", ""};
);

// =============================================================================
// VelocityAccents-Style Implementation
// =============================================================================
// Key insight: Track whether input key is PHYSICALLY PRESSED
// - Cycling only works while input key is held down
// - No timer during cycling - cycle as long as you want
// - When input key is released, cycling ends
// =============================================================================

class SchnelleUmlauteEngine : public InputMethodEngineV2 {
public:
    SchnelleUmlauteEngine(Instance *instance)
        : instance_(instance), enabled_(true) {
        reloadConfig();
    }

    const Configuration *getConfig() const override { return &config_; }
    void setConfig(const RawConfig &config) override {
        config_.load(config);
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
        entry.setIcon("input-keyboard").setLabel("ä").setConfigurable(false);
        methods.push_back(std::move(entry));
        return methods;
    }

    void keyEvent(const InputMethodEntry &entry, KeyEvent &keyEvent) override {
        if (!enabled_) return;

        auto key = keyEvent.key();
        bool isPress = !keyEvent.isRelease();

        // Get character from key
        uint32_t unicode = Key::keySymToUnicode(key.sym());
        std::string keyChar;
        if (unicode > 0 && unicode < 0x10FFFF) {
            keyChar = utf8::UCS4ToUTF8(unicode);
        }

        // =========================================
        // HANDLE KEY RELEASE FIRST
        // =========================================
        if (!isPress) {
            // Check if releasing the cycling input key
            // Note: Compare case-insensitively because user might release Shift
            // before releasing the letter key
            if (cyclingInput_ && inputKeyPressed_ && !keyChar.empty() &&
                cyclingInput_->length() == 1 && keyChar.length() == 1 &&
                std::tolower(static_cast<unsigned char>((*cyclingInput_)[0])) == std::tolower(static_cast<unsigned char>(keyChar[0]))) {

                // Commit the current preedit value
                auto it = umlautMap_.find(*cyclingInput_);
                if (it != umlautMap_.end() && cyclingIndex_ < it->second.size()) {
                    auto* ic = keyEvent.inputContext();
                    ic->inputPanel().reset();
                    ic->commitString(it->second[cyclingIndex_]);
                    ic->updatePreedit();
                }

                inputKeyPressed_ = false;
                resetCycling();
                return;
            }

            // Check if releasing waiting key (before first Space)
            // PREEDIT: Commit the preedit as the original character
            // Note: Compare case-insensitively because user might release Shift
            // before releasing the letter key (e.g., Shift+A pressed, Shift released,
            // then 'a' release event comes but waitingKey_ is "A")
            if (waitingKey_ && inputKeyPressed_ && !keyChar.empty() &&
                waitingKey_->length() == 1 && keyChar.length() == 1 &&
                std::tolower(static_cast<unsigned char>((*waitingKey_)[0])) == std::tolower(static_cast<unsigned char>(keyChar[0]))) {
                auto* ic = keyEvent.inputContext();
                ic->inputPanel().reset();
                ic->commitString(*waitingKey_);
                ic->updatePreedit();
                waitingKey_.reset();
                savedContextRef_.unwatch();
                cancelTimeout();
                inputKeyPressed_ = false;
                return;
            }
            return;
        }

        // =========================================
        // SKIP IF MODIFIER KEYS ARE PRESSED (Ctrl, Alt, Super)
        // This allows shortcuts like Ctrl+C, Alt+F4, etc. to work
        // =========================================
        KeyStates modifiers = key.states();
        if (modifiers.test(KeyState::Ctrl) || modifiers.test(KeyState::Alt) ||
            modifiers.test(KeyState::Super)) {
            // Commit any pending preedit before letting the shortcut through
            if (waitingKey_) {
                auto* ic = keyEvent.inputContext();
                ic->inputPanel().reset();
                ic->commitString(*waitingKey_);
                ic->updatePreedit();
                waitingKey_.reset();
                savedContextRef_.unwatch();
                cancelTimeout();
            }
            inputKeyPressed_ = false;
            resetCycling();
            return;  // Let the shortcut through
        }

        // =========================================
        // HANDLE LEADER KEY (Space/Arrows)
        // =========================================
        if (isLeaderKey(key)) {
            // CASE 1: Currently in cycling mode
            if (cyclingInput_) {
                // Check if input key is still pressed
                if (!inputKeyPressed_) {
                    resetCycling();
                    return;  // Let Space through
                }

                auto it = umlautMap_.find(*cyclingInput_);
                if (it != umlautMap_.end() && it->second.size() > 1) {
                    // Cycle to next variant
                    cyclingIndex_ = (cyclingIndex_ + 1) % it->second.size();
                    const std::string& nextOutput = it->second[cyclingIndex_];

                    // Update preedit with new variant (no deletion needed!)
                    auto* ic = keyEvent.inputContext();
                    Text preedit(nextOutput);
                    preedit.setCursor(preedit.textLength());
                    ic->inputPanel().setClientPreedit(preedit);
                    ic->updatePreedit();

                    keyEvent.filterAndAccept();
                    return;
                }
            }

            // CASE 2: First leader key press (start cycling)
            // PREEDIT: Update preedit to show first umlaut (don't commit yet!)
            if (waitingKey_ && !isTimeoutExpired()) {
                auto it = umlautMap_.find(*waitingKey_);
                if (it != umlautMap_.end() && !it->second.empty()) {
                    auto* ic = keyEvent.inputContext();

                    // Start cycling if multiple outputs - stay in preedit
                    if (it->second.size() > 1) {
                        cyclingInput_ = *waitingKey_;
                        cyclingIndex_ = 0;

                        // Update preedit with first variant
                        Text preedit(it->second[0]);
                        preedit.setCursor(preedit.textLength());
                        ic->inputPanel().setClientPreedit(preedit);
                        ic->updatePreedit();
                    } else {
                        // Single output - commit directly
                        ic->inputPanel().reset();
                        ic->updatePreedit();
                        ic->commitString(it->second[0]);
                    }

                    waitingKey_.reset();
                    savedContextRef_.unwatch();
                    cancelTimeout();
                    keyEvent.filterAndAccept();
                    return;
                }
            }

            // Not in gesture - let Space through
            return;
        }

        // =========================================
        // HANDLE ACCENT KEYS (a, o, u, etc.)
        // Use PREEDIT mode - show character as preview, commit/change on Space
        // =========================================
        bool isAccentKey = umlautMap_.find(keyChar) != umlautMap_.end();

        if (isAccentKey) {
            // Ignore key repeat while waiting or cycling
            if ((waitingKey_ && *waitingKey_ == keyChar) ||
                (cyclingInput_ && *cyclingInput_ == keyChar)) {
                keyEvent.filterAndAccept();
                return;
            }

            // New accent key - commit any existing preedit first
            if (waitingKey_) {
                // Commit previous preedit as-is
                auto* ic = keyEvent.inputContext();
                ic->inputPanel().reset();
                ic->commitString(*waitingKey_);
                ic->updatePreedit();
                waitingKey_.reset();
                savedContextRef_.unwatch();
                cancelTimeout();
            }
            resetCycling();

            // Show character in PREEDIT (not committed yet - can be changed!)
            waitingKey_ = keyChar;
            inputKeyPressed_ = true;
            startTime_ = std::chrono::steady_clock::now();

            // Save the InputContext to use in timeout callback
            auto* ic = keyEvent.inputContext();
            savedContextRef_ = ic->watch();

            scheduleTimeout();

            // Set preedit text
            Text preedit(keyChar);
            preedit.setCursor(preedit.textLength());
            ic->inputPanel().setClientPreedit(preedit);
            ic->updatePreedit();

            keyEvent.filterAndAccept();
            return;
        }

        // =========================================
        // OTHER KEYS - Reset state
        // PREEDIT: Commit the preedit as-is, then let the key through
        // =========================================
        if (waitingKey_) {
            // Commit the preedit as the original character
            auto* ic = keyEvent.inputContext();
            ic->inputPanel().reset();
            ic->commitString(*waitingKey_);
            ic->updatePreedit();
            waitingKey_.reset();
            savedContextRef_.unwatch();
            cancelTimeout();
        }
        resetCycling();
        // Let key through
    }

    void reset(const InputMethodEntry &, InputContextEvent &event) override {
        // Don't clear state if input key is still pressed!
        // Some apps (Chromium, Neovide) call reset() after every commit.
        if (inputKeyPressed_) {
            return;  // Keep all state intact
        }

        waitingKey_.reset();
        savedContextRef_.unwatch();
        inputKeyPressed_ = false;
        cancelTimeout();
        resetCycling();
    }

    void enable() { enabled_ = true; }

    void disable() {
        enabled_ = false;
        waitingKey_.reset();
        savedContextRef_.unwatch();
        inputKeyPressed_ = false;
        cancelTimeout();
        resetCycling();
    }

private:
    void resetCycling() {
        cyclingInput_.reset();
        cyclingIndex_ = 0;
    }

    std::vector<std::string> splitOutputs(const std::string& output) {
        std::vector<std::string> outputs;
        if (output.empty()) return outputs;

        size_t start = 0;
        for (size_t i = 0; i < output.length(); ++i) {
            if (output[i] == ',') {
                if (i > start) {
                    outputs.push_back(output.substr(start, i - start));
                }
                start = i + 1;
            }
        }
        if (start < output.length()) {
            outputs.push_back(output.substr(start));
        }
        return outputs;
    }

    void loadMappingsFromConfig() {
        umlautMap_.clear();
        auto addMapping = [this](const std::string& input, const std::string& output) {
            if (!input.empty() && !output.empty()) {
                auto outputs = splitOutputs(output);
                if (!outputs.empty()) {
                    umlautMap_[input] = outputs;
                }
            }
        };

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
        addMapping(*config_.mapping21Input, *config_.mapping21Output);
        addMapping(*config_.mapping22Input, *config_.mapping22Output);
        addMapping(*config_.mapping23Input, *config_.mapping23Output);
        addMapping(*config_.mapping24Input, *config_.mapping24Output);
        addMapping(*config_.mapping25Input, *config_.mapping25Output);
        addMapping(*config_.mapping26Input, *config_.mapping26Output);
        addMapping(*config_.mapping27Input, *config_.mapping27Output);
        addMapping(*config_.mapping28Input, *config_.mapping28Output);
        addMapping(*config_.mapping29Input, *config_.mapping29Output);
        addMapping(*config_.mapping30Input, *config_.mapping30Output);
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
                return sym == FcitxKey_space;
        }
    }

    bool isTimeoutExpired() const {
        if (!waitingKey_) return false;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - startTime_).count();

        bool isUpperCase = waitingKey_->length() == 1 && std::isupper((*waitingKey_)[0]);
        int effectiveDelay = isUpperCase ? *config_.delayUppercase : *config_.delayLowercase;

        return elapsed > effectiveDelay;
    }

    void scheduleTimeout() {
        if (!waitingKey_) return;

        timeoutEvent_.reset();

        bool isUpperCase = waitingKey_->length() == 1 && std::isupper((*waitingKey_)[0]);
        int effectiveDelay = isUpperCase ? *config_.delayUppercase : *config_.delayLowercase;

        auto* eventLoop = &instance_->eventLoop();

        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t now_usec = static_cast<uint64_t>(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
        uint64_t target_usec = now_usec + static_cast<uint64_t>(effectiveDelay) * 1000;

        auto savedKey = *waitingKey_;
        auto savedRef = savedContextRef_;
        timeoutEvent_ = eventLoop->addTimeEvent(
            CLOCK_MONOTONIC,
            target_usec,
            0,
            [this, savedKey, savedRef](EventSourceTime *, uint64_t) {
                // PREEDIT: Commit the preedit as-is when timeout expires
                if (waitingKey_ && *waitingKey_ == savedKey) {
                    // Only commit to the ORIGINAL context where the key was pressed
                    // This prevents sending text to the wrong window after focus change
                    // Uses TrackableObjectReference: get() returns nullptr if window was closed
                    auto* ctx = savedRef.get();
                    if (ctx && ctx == savedContextRef_.get() && ctx->hasFocus()) {
                        ctx->inputPanel().reset();
                        ctx->commitString(*waitingKey_);
                        ctx->updatePreedit();
                    }
                    // If focus changed or window closed, silently discard the pending key
                    waitingKey_.reset();
                    savedContextRef_.unwatch();
                }
                timeoutEvent_.reset();
                return false;
            }
        );
    }

    void cancelTimeout() {
        timeoutEvent_.reset();
    }

    Instance *instance_;
    bool enabled_;
    SchnelleUmlauteConfig config_;

    // Waiting state (before first Space)
    std::optional<std::string> waitingKey_;
    std::chrono::steady_clock::time_point startTime_;
    std::unique_ptr<EventSourceTime> timeoutEvent_;

    // KEY INSIGHT: Track if input key is physically pressed
    bool inputKeyPressed_ = false;

    // Save the InputContext where waitingKey_ was set to avoid sending to wrong window
    // Uses TrackableObjectReference to safely detect if the window was closed
    TrackableObjectReference<InputContext> savedContextRef_;

    // Cycling state (after first Space, while input key held)
    std::optional<std::string> cyclingInput_;
    size_t cyclingIndex_ = 0;

    // Mappings
    std::unordered_map<std::string, std::vector<std::string>> umlautMap_;
};

class SchnelleUmlauteEngineFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        return new SchnelleUmlauteEngine(manager->instance());
    }
};

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::SchnelleUmlauteEngineFactory)
