#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextproperty.h>
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
    Option<bool> leaderSpace{this, "LeaderSpace", "Space", true};
    Option<bool> leaderLeft{this, "LeaderLeft", "Left Arrow", false};
    Option<bool> leaderRight{this, "LeaderRight", "Right Arrow", false};
    Option<bool> leaderUp{this, "LeaderUp", "Up Arrow", false};
    Option<bool> leaderDown{this, "LeaderDown", "Down Arrow", false};
    Option<bool> leaderAlt{this, "LeaderAlt", "<b><font color=\"red\">experimental</font></b> Alt/AltGr", false};
    Option<std::string> customLeaderKey{this, "CustomLeaderKey", "<b><font color=\"red\">experimental</font></b> Custom Leader Key", ""};
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
// Per-IC State: Each InputContext (application window) gets its own state.
// This prevents focus switches from corrupting gesture state across windows.
// =============================================================================

class SchnelleUmlauteState : public InputContextProperty {
public:
    // Waiting state (before first Space)
    std::optional<std::string> waitingKey_;
    uint64_t startTimeUsec_ = 0;
    std::unique_ptr<EventSourceTime> timeoutEvent_;

    // Track if input key is physically pressed
    bool inputKeyPressed_ = false;
    int waitingKeyCode_ = 0;

    // Set after commit to route next Space through commitString (ordering guard).
    // Intentionally NOT cleared in clearAllState() — apps like WezTerm and
    // Chromium call reset() after every commit, which would destroy the
    // ordering guard before Space arrives.
    bool recentlyCommitted_ = false;

    // Cycling state (after first Space, while input key held)
    std::optional<std::string> cyclingInput_;
    size_t cyclingIndex_ = 0;

    // Track physically held keys to distinguish fresh presses from repeats
    std::unordered_set<int> heldRawCodes_;

    // Suppress auto-repeat after single-output commit until key is released.
    // Without this, held accent keys generate repeat events that start new
    // unwanted gestures after the conversion is already committed (e.g. "üu").
    int committedKeyCode_ = 0;

    // Track consumed Alt/AltGr leader press to also consume the release.
    // Prevents compositor state confusion from an orphan modifier release
    // and TUI side effects from stray Alt release events.
    int consumedAltCode_ = 0;

    void clearAllState() {
        waitingKey_.reset();
        inputKeyPressed_ = false;
        waitingKeyCode_ = 0;
        // Note: recentlyCommitted_ is intentionally NOT cleared here.
        cancelTimeout();
        resetCycling();
        heldRawCodes_.clear();
        committedKeyCode_ = 0;
        consumedAltCode_ = 0;
    }

    void resetCycling() {
        cyclingInput_.reset();
        cyclingIndex_ = 0;
    }

    void cancelTimeout() {
        timeoutEvent_.reset();
    }

    bool isTimeoutExpired(int effectiveDelay) const {
        if (!waitingKey_) return false;
        uint64_t now_usec = nowUsec();
        uint64_t elapsed_ms = (now_usec - startTimeUsec_) / kMicrosecondsPerMillisecond;
        return elapsed_ms > static_cast<uint64_t>(effectiveDelay);
    }

    static uint64_t nowUsec() {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * kMicrosecondsPerSecond
             + ts.tv_nsec / kNanosecondsPerMicrosecond;
    }
};

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
        : instance_(instance),
          factory_([](InputContext &) { return new SchnelleUmlauteState; }) {
        instance_->inputContextManager().registerProperty(
            "schnelle-umlaute-state", &factory_);
        reloadConfig();
    }

    const Configuration *getConfig() const override { return &config_; }
    void setConfig(const RawConfig &config) override {
        config_.load(config);
        config_.delayLowercase.setValue(std::clamp(*config_.delayLowercase, kDelayMin, kDelayMax));
        config_.delayUppercase.setValue(std::clamp(*config_.delayUppercase, kDelayMin, kDelayMax));

        // Sanitize custom leader key: trim whitespace, keep only first
        // UTF-8 character. Only a single key is valid — spaces would
        // silently shadow the Space toggle, and multi-char strings
        // would never match a single keypress.
        std::string customKey = *config_.customLeaderKey;
        size_t start = customKey.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) {
            customKey = "";
        } else {
            size_t end = customKey.find_last_not_of(" \t\n\r");
            customKey = customKey.substr(start, end - start + 1);
        }
        if (customKey.length() > 1) {
            unsigned char first = static_cast<unsigned char>(customKey[0]);
            size_t charLen = 1;
            if (first >= 0xF0) charLen = 4;
            else if (first >= 0xE0) charLen = 3;
            else if (first >= 0xC0) charLen = 2;
            if (charLen <= customKey.length()) {
                customKey = customKey.substr(0, charLen);
            }
        }
        config_.customLeaderKey.setValue(customKey);

        safeSaveAsIni(config_, "conf/schnelle-umlaute.conf");
        applyConfig();
    }

    void reloadConfig() override {
        readAsIni(config_, "conf/schnelle-umlaute.conf");
        applyConfig();
    }

    std::vector<InputMethodEntry> listInputMethods() override {
        std::vector<InputMethodEntry> methods;
        InputMethodEntry entry("schnelle-umlaute", "Schnelle Umlaute", "de", "schnelle-umlaute");
        entry.setIcon("input-keyboard").setLabel("ä").setConfigurable(false);
        methods.push_back(std::move(entry));
        return methods;
    }

    void keyEvent(const InputMethodEntry &entry, KeyEvent &keyEvent) override {
        auto *ic = keyEvent.inputContext();
        auto *state = ic->propertyFor(&factory_);

        auto key = keyEvent.key();
        bool isPress = !keyEvent.isRelease();
        int rawCode = keyEvent.rawKey().code();

        // Track physical key state for repeat detection.
        // A key already in heldRawCodes_ is a repeat (auto-repeat).
        bool isNewKeyPress = true;
        if (isPress) {
            isNewKeyPress = state->heldRawCodes_.insert(rawCode).second;
        } else {
            state->heldRawCodes_.erase(rawCode);
        }

        // Get character from key
        uint32_t unicode = Key::keySymToUnicode(key.sym());
        std::string keyChar;
        if (unicode > 0 && unicode < kMaxUnicodeCodepoint) {
            keyChar = utf8::UCS4ToUTF8(unicode);
        }

        // Pure modifier key presses (Shift, Ctrl, Alt, Super, etc.)
        // pass through without affecting gesture state.
        // Only Modifier+Key combinations trigger the modifier check below.
        if (isPress && key.sym() >= FcitxKey_Shift_L && key.sym() <= FcitxKey_Hyper_R) {
            // Allow Alt_L/Alt_R through as leader when configured and gesture active.
            // ISO_Level3_Shift (AltGr on EU layouts, 0xfe03) is outside this range
            // and passes through naturally.
            if (*config_.leaderAlt &&
                (key.sym() == FcitxKey_Alt_L || key.sym() == FcitxKey_Alt_R) &&
                (state->waitingKey_ || state->cyclingInput_)) {
                // Fall through to leader key handling
            } else {
                return;
            }
        }

        // =========================================
        // ORDERING GUARD: After a recent commitString, route Space
        // through commitString too so both go through the same channel.
        // Without this, committed text and raw key events can arrive
        // at the application out of order in browsers and WezTerm.
        // Skip when an accent key is waiting — Space should act as
        // leader key for conversion (e.g. "as" + Space → "aß").
        // Modifier combinations (Ctrl+Space, Alt+Space) are excluded
        // so shortcuts like IM toggle are not swallowed.
        // =========================================
        if (state->recentlyCommitted_ && isPress) {
            state->recentlyCommitted_ = false;
            if (key.sym() == FcitxKey_space && !state->waitingKey_ && !hasModifiers(key)) {
                ic->commitString(" ");
                keyEvent.filterAndAccept();
                return;
            }
        }

        // =========================================
        // HANDLE KEY RELEASE FIRST
        // =========================================
        if (!isPress) {
            // Consume Alt/AltGr release when the press was consumed as leader.
            // Prevents compositor state confusion and TUI side effects.
            if (state->consumedAltCode_ != 0 && rawCode == state->consumedAltCode_) {
                state->consumedAltCode_ = 0;
                keyEvent.filterAndAccept();
                return;
            }

            // Consume release of key that was committed via single-output.
            // The press was filterAndAccepted, so the release is an orphan.
            if (state->committedKeyCode_ != 0 && rawCode == state->committedKeyCode_) {
                state->committedKeyCode_ = 0;
                keyEvent.filterAndAccept();
                return;
            }

            // Check if releasing the cycling input key
            // Compare physical keycode so shifted chars (!, @, #) match their base key
            if (state->cyclingInput_ && state->inputKeyPressed_ && rawCode == state->waitingKeyCode_) {

                // Commit the current preedit value
                auto it = umlautMap_.find(*state->cyclingInput_);
                if (it != umlautMap_.end() && state->cyclingIndex_ < it->second.size()) {
                    ic->inputPanel().reset();
                    ic->commitString(it->second[state->cyclingIndex_]);
                    ic->updatePreedit();
                }

                state->inputKeyPressed_ = false;
                state->resetCycling();
                keyEvent.filterAndAccept();
                return;
            }

            // Check if releasing waiting key (before first Space)
            // PREEDIT: Commit the preedit as the original character
            // Compare physical keycode so shifted chars (!, @, #) and uppercase
            // letters match even if Shift is released first
            if (state->waitingKey_ && state->inputKeyPressed_ && rawCode == state->waitingKeyCode_) {
                commitPendingKey(ic, state);
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
        // Modifier combinations (Ctrl+Space) are excluded so shortcuts
        // are not swallowed — pending char is committed separately instead.
        if (state->waitingKey_ && state->isTimeoutExpired(getEffectiveDelay(state))) {
            std::string pending = *state->waitingKey_;
            ic->inputPanel().reset();
            ic->updatePreedit();
            state->waitingKey_.reset();
            state->waitingKeyCode_ = 0;
            state->cancelTimeout();
            state->inputKeyPressed_ = false;

            if (key.sym() == FcitxKey_space && !hasModifiers(key)) {
                ic->commitString(pending + " ");
                state->recentlyCommitted_ = true;
                keyEvent.filterAndAccept();
                return;
            }
            ic->commitString(pending);
            state->recentlyCommitted_ = true;
        }

        // =========================================
        // HANDLE MODIFIER COMBINATIONS (Ctrl+C, Alt+F4, etc.)
        // Unlike the pure modifier early-return above (Shift/Ctrl alone),
        // this handles a modifier HELD + another key pressed.
        // =========================================
        if (hasModifiers(key)) {
            // Commit any pending preedit before letting the shortcut through
            commitPendingKey(ic, state);
            commitCyclingValue(ic, state);
            state->inputKeyPressed_ = false;
            return;  // Let the shortcut through
        }

        // =========================================
        // HANDLE LEADER KEY (Space/Arrows/Alt/Custom)
        // =========================================
        if (isLeaderKey(key, keyChar)) {
            bool isAlt = isAltLeaderSym(key.sym());

            // CASE 1: Currently in cycling mode
            if (state->cyclingInput_) {
                // Check if input key is still pressed
                if (!state->inputKeyPressed_) {
                    state->resetCycling();
                    return;  // Let Space through
                }

                auto it = umlautMap_.find(*state->cyclingInput_);
                if (it != umlautMap_.end() && it->second.size() > 1) {
                    // Cycle to next variant
                    state->cyclingIndex_ = (state->cyclingIndex_ + 1) % it->second.size();
                    const std::string& nextOutput = it->second[state->cyclingIndex_];

                    // Update preedit with new variant (no deletion needed!)
                    updateClientPreedit(ic, nextOutput);

                    if (isAlt) state->consumedAltCode_ = rawCode;
                    keyEvent.filterAndAccept();
                    return;
                }
            }

            // CASE 2: First leader key press (start cycling)
            // PREEDIT: Update preedit to show first umlaut (don't commit yet!)
            if (state->waitingKey_ && !state->isTimeoutExpired(getEffectiveDelay(state))) {
                auto it = umlautMap_.find(*state->waitingKey_);
                if (it != umlautMap_.end() && !it->second.empty()) {

                    // Start cycling if multiple outputs - stay in preedit
                    if (it->second.size() > 1) {
                        state->cyclingInput_ = *state->waitingKey_;
                        state->cyclingIndex_ = 0;

                        // Update preedit with first variant
                        updateClientPreedit(ic, it->second[0]);
                    } else {
                        // Single output - commit directly
                        ic->inputPanel().reset();
                        ic->updatePreedit();
                        ic->commitString(it->second[0]);
                        state->committedKeyCode_ = state->waitingKeyCode_;
                        state->inputKeyPressed_ = false;
                        state->waitingKeyCode_ = 0;
                        state->recentlyCommitted_ = true;
                    }

                    state->waitingKey_.reset();
                    state->cancelTimeout();
                    if (isAlt) state->consumedAltCode_ = rawCode;
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
            if ((state->waitingKey_ && *state->waitingKey_ == keyChar) ||
                (state->cyclingInput_ && *state->cyclingInput_ == keyChar)) {
                keyEvent.filterAndAccept();
                return;
            }

            // During an active gesture, suppress repeats of other held keys.
            // Prevents held keys from interfering with another key's
            // waiting/cycling. After the gesture ends, repeats are allowed
            // to start new gestures (e.g. hold 'a'+'s', cycle 's', release
            // 's' → 'a' repeat can now start a new 'a' gesture).
            if (!isNewKeyPress && (state->waitingKey_ || state->cyclingInput_)) {
                keyEvent.filterAndAccept();
                return;
            }

            // Suppress auto-repeat of key that was just committed via single-output.
            // After single-output commit, the input key may still be held, generating
            // repeat events. Without this guard, repeats start new unwanted gestures
            // (e.g. 'u' + AltGr → "ü" then repeat 'u' → "üu").
            if (!isNewKeyPress && state->committedKeyCode_ != 0 &&
                rawCode == state->committedKeyCode_) {
                keyEvent.filterAndAccept();
                return;
            }

            // New accent key - commit any pending state first
            commitPendingKey(ic, state);
            commitCyclingValue(ic, state);

            // Show character in PREEDIT (not committed yet - can be changed!)
            state->waitingKey_ = keyChar;
            state->waitingKeyCode_ = keyEvent.rawKey().code();
            state->inputKeyPressed_ = true;
            state->startTimeUsec_ = SchnelleUmlauteState::nowUsec();

            scheduleTimeout(ic, state);

            // Set preedit text
            updateClientPreedit(ic, keyChar);

            keyEvent.filterAndAccept();
            return;
        }

        // =========================================
        // OTHER KEYS - Reset state
        // PREEDIT: Commit the preedit as-is, then let the key through
        // =========================================
        commitPendingKey(ic, state);
        commitCyclingValue(ic, state);
        // Let key through
    }

    void activate(const InputMethodEntry &, InputContextEvent &event) override {
        // Ensure clean state when switching TO this input method.
        // Catches residual state after crashes or unexpected restarts.
        auto *state = event.inputContext()->propertyFor(&factory_);
        state->clearAllState();
        state->recentlyCommitted_ = false;
    }

    void deactivate(const InputMethodEntry &, InputContextEvent &event) override {
        // Called on genuine focus changes (FocusOut / IC switch).
        // Clears all state so gestures don't leak across windows.
        auto *ic = event.inputContext();
        auto *state = ic->propertyFor(&factory_);

        // On IM switch (Ctrl+Space): commit pending preedit before clearing.
        // The server does NOT auto-commit preedit on IM switch, only on FocusOut.
        if (event.type() == EventType::InputContextSwitchInputMethod) {
            commitPendingKey(ic, state);
            commitCyclingValue(ic, state);
        }

        state->clearAllState();
        state->recentlyCommitted_ = false;
    }

    void reset(const InputMethodEntry &, InputContextEvent &event) override {
        // Don't clear state if input key is still pressed!
        // Some apps (Chromium, Neovide) call reset() after every commit.
        auto *state = event.inputContext()->propertyFor(&factory_);
        if (state->inputKeyPressed_) {
            return;  // Keep all state intact
        }

        state->clearAllState();
    }

private:
    // Apply in-memory config: rebuild mappings and log active leaders.
    // Shared by setConfig (values already loaded) and reloadConfig (read from disk).
    void applyConfig() {
        loadMappingsFromConfig();

        std::string leaders;
        if (*config_.leaderSpace) leaders += "Space ";
        if (*config_.leaderLeft) leaders += "Left ";
        if (*config_.leaderRight) leaders += "Right ";
        if (*config_.leaderUp) leaders += "Up ";
        if (*config_.leaderDown) leaders += "Down ";
        if (*config_.leaderAlt) leaders += "Alt/AltGr ";
        const std::string &custom = *config_.customLeaderKey;
        if (!custom.empty()) leaders += "Custom('" + custom + "') ";
        if (leaders.empty()) leaders = "None ";

        FCITX_INFO() << "Schnelle: Config loaded - DelayLowercase=" << *config_.delayLowercase
                     << "ms, DelayUppercase=" << *config_.delayUppercase
                     << "ms, Leaders=" << leaders
                     << ", Mappings=" << umlautMap_.size();
    }

    void updateClientPreedit(InputContext *ic, const std::string &text) {
        Text preedit(text);
        preedit.setCursor(preedit.textLength());
        ic->inputPanel().setClientPreedit(preedit);
        ic->updatePreedit();
    }

    void commitPendingKey(InputContext *ic, SchnelleUmlauteState *state) {
        if (!state->waitingKey_) return;
        ic->inputPanel().reset();
        ic->commitString(*state->waitingKey_);
        ic->updatePreedit();
        state->waitingKey_.reset();
        state->waitingKeyCode_ = 0;
        state->cancelTimeout();
        state->inputKeyPressed_ = false;
        state->recentlyCommitted_ = true;
    }

    void commitCyclingValue(InputContext *ic, SchnelleUmlauteState *state) {
        if (!state->cyclingInput_) return;
        auto it = umlautMap_.find(*state->cyclingInput_);
        if (it != umlautMap_.end() && state->cyclingIndex_ < it->second.size()) {
            ic->inputPanel().reset();
            ic->commitString(it->second[state->cyclingIndex_]);
            ic->updatePreedit();
            state->recentlyCommitted_ = true;
        }
        state->inputKeyPressed_ = false;
        state->resetCycling();
    }

    // Intentionally no whitespace trimming: leading/trailing spaces in outputs
    // are valid (e.g. mapping a key to " " so terminal commands skip history).
    std::vector<std::string> splitOutputs(const std::string &output) {
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

    // Check for Ctrl/Alt/Super in key state. Shift is intentionally
    // excluded — it is needed for uppercase accent mappings (Shift+A → Ä).
    static bool hasModifiers(const Key &key) {
        KeyStates mods = key.states();
        return mods.test(KeyState::Ctrl) || mods.test(KeyState::Alt) ||
               mods.test(KeyState::Super);
    }

    static bool isAltLeaderSym(KeySym sym) {
        return sym == FcitxKey_Alt_L || sym == FcitxKey_Alt_R ||
               sym == FcitxKey_ISO_Level3_Shift;
    }

    bool isLeaderKey(const Key &key, const std::string &keyChar) const {
        KeySym sym = key.sym();

        // Check Alt/AltGr leader (works on all layouts)
        // - Alt_L/Alt_R: Left/Right Alt on US layouts
        // - ISO_Level3_Shift: AltGr on European layouts (physical Right Alt)
        if (*config_.leaderAlt && isAltLeaderSym(sym)) {
            return true;
        }

        // Check custom leader key
        const std::string &custom = *config_.customLeaderKey;
        if (!custom.empty() && !keyChar.empty() && keyChar == custom) {
            return true;
        }

        // Check individual leader key toggles
        if (*config_.leaderSpace && sym == FcitxKey_space) return true;
        if (*config_.leaderLeft && sym == FcitxKey_Left) return true;
        if (*config_.leaderRight && sym == FcitxKey_Right) return true;
        if (*config_.leaderUp && sym == FcitxKey_Up) return true;
        if (*config_.leaderDown && sym == FcitxKey_Down) return true;

        return false;
    }

    int getEffectiveDelay(const SchnelleUmlauteState *state) const {
        if (!state->waitingKey_) return *config_.delayLowercase;
        bool isUpper = state->waitingKey_->length() == 1 &&
                       std::isupper(static_cast<unsigned char>((*state->waitingKey_)[0]));
        return isUpper ? *config_.delayUppercase : *config_.delayLowercase;
    }

    void scheduleTimeout(InputContext *ic, SchnelleUmlauteState *state) {
        if (!state->waitingKey_) return;

        state->cancelTimeout();

        int effectiveDelay = getEffectiveDelay(state);
        auto *eventLoop = &instance_->eventLoop();

        uint64_t now_usec = SchnelleUmlauteState::nowUsec();
        uint64_t target_usec = now_usec + static_cast<uint64_t>(effectiveDelay) * kMicrosecondsPerMillisecond;

        auto savedKey = *state->waitingKey_;
        auto savedRef = ic->watch();
        state->timeoutEvent_ = eventLoop->addTimeEvent(
            CLOCK_MONOTONIC,
            target_usec,
            0,
            [state, savedKey, savedRef](EventSourceTime *, uint64_t) {
                // Validate IC first — if destroyed, state is gone too.
                auto *ctx = savedRef.get();
                if (!ctx) return false;

                if (state->waitingKey_ && *state->waitingKey_ == savedKey) {
                    ctx->inputPanel().reset();
                    ctx->commitString(*state->waitingKey_);
                    ctx->updatePreedit();
                    state->recentlyCommitted_ = true;
                    state->waitingKey_.reset();
                    state->waitingKeyCode_ = 0;
                    state->inputKeyPressed_ = false;
                }
                // Don't reset timeoutEvent_ here — destroying the EventSource
                // inside its own callback is a use-after-free risk. Returning
                // false disables the timer; the unique_ptr is cleaned up by
                // the next scheduleTimeout() or cancelTimeout() call.
                return false;
            }
        );
    }

    Instance *instance_;
    SchnelleUmlauteConfig config_;
    FactoryFor<SchnelleUmlauteState> factory_;

    // Mappings (shared across all InputContexts, read-only after config load)
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
