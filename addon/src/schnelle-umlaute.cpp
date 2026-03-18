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
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <ctime>
#include <algorithm>

namespace fcitx {

constexpr uint32_t kMaxUnicodeCodepoint = 0x10FFFF;
constexpr uint64_t kMicrosecondsPerSecond = 1'000'000;
constexpr uint64_t kNanosecondsPerMicrosecond = 1'000;
constexpr uint64_t kMicrosecondsPerMillisecond = 1'000;

constexpr int kDelayMin = 50;
constexpr int kDelayMax = 2000;
constexpr int kDelayStep = 25;

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
    Option<int, IntConstrainWithStep> delayLowercase{this, "DelayLowercase", "Delay for lowercase letters (ms)", 400, IntConstrainWithStep(kDelayMin, kDelayMax, kDelayStep)};
    Option<int, IntConstrainWithStep> delayUppercase{this, "DelayUppercase", "Delay for uppercase letters (ms)", 700, IntConstrainWithStep(kDelayMin, kDelayMax, kDelayStep)};
    Option<LeaderKey> leaderKey{this, "LeaderKey", "Activation key (Leader Key)", LeaderKey::Space};
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
        config_.delayLowercase.setValue(std::clamp(*config_.delayLowercase, kDelayMin, kDelayMax));
        config_.delayUppercase.setValue(std::clamp(*config_.delayUppercase, kDelayMin, kDelayMax));
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
        int rawCode = keyEvent.rawKey().code();

        // Track physical key state for repeat detection.
        // A key already in heldRawCodes_ is a repeat (auto-repeat).
        bool isNewKeyPress = true;
        if (isPress) {
            isNewKeyPress = heldRawCodes_.insert(rawCode).second;
        } else {
            heldRawCodes_.erase(rawCode);
        }

        // Get character from key
        uint32_t unicode = Key::keySymToUnicode(key.sym());
        std::string keyChar;
        if (unicode > 0 && unicode < kMaxUnicodeCodepoint) {
            keyChar = utf8::UCS4ToUTF8(unicode);
        }

        // =========================================
        // ORDERING GUARD: After a recent commitString, route Space
        // through commitString too so both go through the same channel.
        // Without this, committed text and raw key events can arrive
        // at the application out of order in browsers and WezTerm.
        // Skip when an accent key is waiting — Space should act as
        // leader key for conversion (e.g. "as" + Space → "aß").
        // =========================================
        if (recentlyCommitted_ && isPress) {
            recentlyCommitted_ = false;
            if (key.sym() == FcitxKey_space && !waitingKey_) {
                keyEvent.inputContext()->commitString(" ");
                keyEvent.filterAndAccept();
                return;
            }
        }

        // =========================================
        // HANDLE KEY RELEASE FIRST
        // =========================================
        if (!isPress) {
            // Check if releasing the cycling input key
            // Compare physical keycode so shifted chars (!, @, #) match their base key
            if (cyclingInput_ && inputKeyPressed_ && rawCode == waitingKeyCode_) {

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
                keyEvent.filterAndAccept();
                return;
            }

            // Check if releasing waiting key (before first Space)
            // PREEDIT: Commit the preedit as the original character
            // Compare physical keycode so shifted chars (!, @, #) and uppercase
            // letters match even if Shift is released first
            if (waitingKey_ && inputKeyPressed_ && rawCode == waitingKeyCode_) {
                commitPendingKey(keyEvent.inputContext());
                keyEvent.filterAndAccept();
                return;
            }
            return;
        }

        // =========================================
        // ORDERING GUARD: Ensure correct character order after timeout
        // =========================================
        // Commit pending char in the SAME commitString as the following key
        // so both travel through one XIM event — impossible to reorder.
        if (waitingKey_ && isTimeoutExpired()) {
            auto* ic = keyEvent.inputContext();
            std::string pending = *waitingKey_;
            ic->inputPanel().reset();
            ic->updatePreedit();
            waitingKey_.reset();
            waitingKeyCode_ = 0;
            savedContextRef_.unwatch();
            cancelTimeout();
            inputKeyPressed_ = false;

            if (key.sym() == FcitxKey_space) {
                ic->commitString(pending + " ");
                recentlyCommitted_ = true;
                keyEvent.filterAndAccept();
                return;
            }
            ic->commitString(pending);
            recentlyCommitted_ = true;
        }

        // =========================================
        // SKIP IF MODIFIER KEYS ARE PRESSED (Ctrl, Alt, Super)
        // This allows shortcuts like Ctrl+C, Alt+F4, etc. to work
        // =========================================
        KeyStates modifiers = key.states();
        if (modifiers.test(KeyState::Ctrl) || modifiers.test(KeyState::Alt) ||
            modifiers.test(KeyState::Super)) {
            // Commit any pending preedit before letting the shortcut through
            commitPendingKey(keyEvent.inputContext());
            commitCyclingValue(keyEvent.inputContext());
            inputKeyPressed_ = false;
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
                    updateClientPreedit(keyEvent.inputContext(), nextOutput);

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
                        updateClientPreedit(ic, it->second[0]);
                    } else {
                        // Single output - commit directly
                        ic->inputPanel().reset();
                        ic->updatePreedit();
                        ic->commitString(it->second[0]);
                        inputKeyPressed_ = false;
                        waitingKeyCode_ = 0;
                        recentlyCommitted_ = true;
                    }

                    waitingKey_.reset();
                    savedContextRef_.unwatch();
                    cancelTimeout();
                    keyEvent.filterAndAccept();
                    return;
                }
            }

            // Not in gesture - let leader key through
            return;
        }

        // =========================================
        // HANDLE ACCENT KEYS (a, o, u, etc.)
        // Use PREEDIT mode - show character as preview, commit/change on Space
        // =========================================
        if (!keyChar.empty() && umlautMap_.find(keyChar) != umlautMap_.end()) {
            // Ignore key repeat while waiting or cycling
            if ((waitingKey_ && *waitingKey_ == keyChar) ||
                (cyclingInput_ && *cyclingInput_ == keyChar)) {
                keyEvent.filterAndAccept();
                return;
            }

            // During an active gesture, suppress repeats of other held keys.
            // Prevents held keys from interfering with another key's
            // waiting/cycling. After the gesture ends, repeats are allowed
            // to start new gestures (e.g. hold 'a'+'s', cycle 's', release
            // 's' → 'a' repeat can now start a new 'a' gesture).
            if (!isNewKeyPress && (waitingKey_ || cyclingInput_)) {
                keyEvent.filterAndAccept();
                return;
            }

            // New accent key - commit any pending state first
            commitPendingKey(keyEvent.inputContext());
            commitCyclingValue(keyEvent.inputContext());

            // Show character in PREEDIT (not committed yet - can be changed!)
            waitingKey_ = keyChar;
            waitingKeyCode_ = keyEvent.rawKey().code();
            inputKeyPressed_ = true;
            {
                timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                startTimeUsec_ = static_cast<uint64_t>(ts.tv_sec) * kMicrosecondsPerSecond
                               + ts.tv_nsec / kNanosecondsPerMicrosecond;
            }

            // Save the InputContext to use in timeout callback
            auto* ic = keyEvent.inputContext();
            savedContextRef_ = ic->watch();

            scheduleTimeout();

            // Set preedit text
            updateClientPreedit(ic, keyChar);

            keyEvent.filterAndAccept();
            return;
        }

        // =========================================
        // OTHER KEYS - Reset state
        // PREEDIT: Commit the preedit as-is, then let the key through
        // =========================================
        commitPendingKey(keyEvent.inputContext());
        commitCyclingValue(keyEvent.inputContext());
        // Let key through
    }

    void reset(const InputMethodEntry &, InputContextEvent &event) override {
        // Don't clear state if input key is still pressed!
        // Some apps (Chromium, Neovide) call reset() after every commit.
        if (inputKeyPressed_) {
            return;  // Keep all state intact
        }

        clearAllState();
    }

    void enable() { enabled_ = true; }

    void disable() {
        enabled_ = false;
        clearAllState();
        recentlyCommitted_ = false;
    }

private:
    void clearAllState() {
        waitingKey_.reset();
        savedContextRef_.unwatch();
        inputKeyPressed_ = false;
        // Note: recentlyCommitted_ is intentionally NOT cleared here.
        // Apps like WezTerm and Chromium call reset() after every commit,
        // which would destroy the ordering guard before Space arrives.
        cancelTimeout();
        resetCycling();
        heldRawCodes_.clear();
    }

    void updateClientPreedit(InputContext* ic, const std::string& text) {
        Text preedit(text);
        preedit.setCursor(preedit.textLength());
        ic->inputPanel().setClientPreedit(preedit);
        ic->updatePreedit();
    }

    void commitPendingKey(InputContext* ic) {
        if (!waitingKey_) return;
        ic->inputPanel().reset();
        ic->commitString(*waitingKey_);
        ic->updatePreedit();
        waitingKey_.reset();
        waitingKeyCode_ = 0;
        savedContextRef_.unwatch();
        cancelTimeout();
        inputKeyPressed_ = false;
        recentlyCommitted_ = true;
    }

    void commitCyclingValue(InputContext* ic) {
        if (!cyclingInput_) return;
        auto it = umlautMap_.find(*cyclingInput_);
        if (it != umlautMap_.end() && cyclingIndex_ < it->second.size()) {
            ic->inputPanel().reset();
            ic->commitString(it->second[cyclingIndex_]);
            ic->updatePreedit();
            recentlyCommitted_ = true;
        }
        inputKeyPressed_ = false;
        resetCycling();
    }

    void resetCycling() {
        cyclingInput_.reset();
        cyclingIndex_ = 0;
        waitingKeyCode_ = 0;
    }

    // Intentionally no whitespace trimming: leading/trailing spaces in outputs
    // are valid (e.g. mapping a key to " " so terminal commands skip history).
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

    int getEffectiveDelay() const {
        if (!waitingKey_) return *config_.delayLowercase;
        bool isUpper = waitingKey_->length() == 1 &&
                       std::isupper(static_cast<unsigned char>((*waitingKey_)[0]));
        return isUpper ? *config_.delayUppercase : *config_.delayLowercase;
    }

    bool isTimeoutExpired() const {
        if (!waitingKey_) return false;

        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t now_usec = static_cast<uint64_t>(ts.tv_sec) * kMicrosecondsPerSecond
                          + ts.tv_nsec / kNanosecondsPerMicrosecond;
        uint64_t elapsed_ms = (now_usec - startTimeUsec_) / kMicrosecondsPerMillisecond;

        return elapsed_ms > static_cast<uint64_t>(getEffectiveDelay());
    }

    void scheduleTimeout() {
        if (!waitingKey_) return;

        timeoutEvent_.reset();

        int effectiveDelay = getEffectiveDelay();

        auto* eventLoop = &instance_->eventLoop();

        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t now_usec = static_cast<uint64_t>(ts.tv_sec) * kMicrosecondsPerSecond
                          + ts.tv_nsec / kNanosecondsPerMicrosecond;
        uint64_t target_usec = now_usec + static_cast<uint64_t>(effectiveDelay) * kMicrosecondsPerMillisecond;

        auto savedKey = *waitingKey_;
        auto savedRef = savedContextRef_;
        timeoutEvent_ = eventLoop->addTimeEvent(
            CLOCK_MONOTONIC,
            target_usec,
            0,
            [this, savedKey, savedRef](EventSourceTime *, uint64_t) {
                if (waitingKey_ && *waitingKey_ == savedKey) {
                    auto* ctx = savedRef.get();
                    if (ctx && ctx == savedContextRef_.get()) {
                        ctx->inputPanel().reset();
                        ctx->commitString(*waitingKey_);
                        ctx->updatePreedit();
                        recentlyCommitted_ = true;
                    }
                    waitingKey_.reset();
                    waitingKeyCode_ = 0;
                    savedContextRef_.unwatch();
                    inputKeyPressed_ = false;
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
    uint64_t startTimeUsec_ = 0;
    std::unique_ptr<EventSourceTime> timeoutEvent_;

    // KEY INSIGHT: Track if input key is physically pressed
    bool inputKeyPressed_ = false;
    int waitingKeyCode_ = 0;

    // Save the InputContext where waitingKey_ was set to avoid sending to wrong window
    // Uses TrackableObjectReference to safely detect if the window was closed
    TrackableObjectReference<InputContext> savedContextRef_;

    // Set after commitPendingKey to route next Space through commitString
    bool recentlyCommitted_ = false;

    // Cycling state (after first Space, while input key held)
    std::optional<std::string> cyclingInput_;
    size_t cyclingIndex_ = 0;

    // Track physically held keys to distinguish fresh presses from repeats
    std::unordered_set<int> heldRawCodes_;

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
