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
    DelayConfig,
    Option<int, IntConstrainWithStep> lowercase{this, "Lowercase", "Lowercase (ms)", 400, IntConstrainWithStep(kDelayMin, kDelayMax, kDelayStep)};
    Option<int, IntConstrainWithStep> uppercase{this, "Uppercase", "Uppercase (ms)", 700, IntConstrainWithStep(kDelayMin, kDelayMax, kDelayStep)};
);

FCITX_CONFIGURATION(
    LeaderConfig,
    Option<bool> space{this, "Space", "Space", true};
    Option<bool> left{this, "Left", "Left Arrow", false};
    Option<bool> right{this, "Right", "Right Arrow", false};
    Option<bool> up{this, "Up", "Up Arrow", false};
    Option<bool> down{this, "Down", "Down Arrow", false};
    Option<bool> alt{this, "Alt", "\xe2\x9a\xa0 experimental \xe2\x80\x93 Alt/AltGr", false};
    Option<std::string> customKey{this, "CustomKey", "\xe2\x9a\xa0 experimental \xe2\x80\x93 Custom Leader Key", ""};
);

FCITX_CONFIGURATION(
    MappingsConfig,
    Option<std::string> input1{this, "Input1", "Input 1", "a"};
    Option<std::string> output1{this, "Output1", "Output 1", "\xc3\xa4"};
    Option<std::string> input2{this, "Input2", "Input 2", "o"};
    Option<std::string> output2{this, "Output2", "Output 2", "\xc3\xb6"};
    Option<std::string> input3{this, "Input3", "Input 3", "u"};
    Option<std::string> output3{this, "Output3", "Output 3", "\xc3\xbc"};
    Option<std::string> input4{this, "Input4", "Input 4", "s"};
    Option<std::string> output4{this, "Output4", "Output 4", "\xc3\x9f"};
    Option<std::string> input5{this, "Input5", "Input 5", "A"};
    Option<std::string> output5{this, "Output5", "Output 5", "\xc3\x84"};
    Option<std::string> input6{this, "Input6", "Input 6", "O"};
    Option<std::string> output6{this, "Output6", "Output 6", "\xc3\x96"};
    Option<std::string> input7{this, "Input7", "Input 7", "U"};
    Option<std::string> output7{this, "Output7", "Output 7", "\xc3\x9c"};
    Option<std::string> input8{this, "Input8", "Input 8", ""};
    Option<std::string> output8{this, "Output8", "Output 8", ""};
    Option<std::string> input9{this, "Input9", "Input 9", ""};
    Option<std::string> output9{this, "Output9", "Output 9", ""};
    Option<std::string> input10{this, "Input10", "Input 10", ""};
    Option<std::string> output10{this, "Output10", "Output 10", ""};
    Option<std::string> input11{this, "Input11", "Input 11", ""};
    Option<std::string> output11{this, "Output11", "Output 11", ""};
    Option<std::string> input12{this, "Input12", "Input 12", ""};
    Option<std::string> output12{this, "Output12", "Output 12", ""};
    Option<std::string> input13{this, "Input13", "Input 13", ""};
    Option<std::string> output13{this, "Output13", "Output 13", ""};
    Option<std::string> input14{this, "Input14", "Input 14", ""};
    Option<std::string> output14{this, "Output14", "Output 14", ""};
    Option<std::string> input15{this, "Input15", "Input 15", ""};
    Option<std::string> output15{this, "Output15", "Output 15", ""};
    Option<std::string> input16{this, "Input16", "Input 16", ""};
    Option<std::string> output16{this, "Output16", "Output 16", ""};
    Option<std::string> input17{this, "Input17", "Input 17", ""};
    Option<std::string> output17{this, "Output17", "Output 17", ""};
    Option<std::string> input18{this, "Input18", "Input 18", ""};
    Option<std::string> output18{this, "Output18", "Output 18", ""};
    Option<std::string> input19{this, "Input19", "Input 19", ""};
    Option<std::string> output19{this, "Output19", "Output 19", ""};
    Option<std::string> input20{this, "Input20", "Input 20", ""};
    Option<std::string> output20{this, "Output20", "Output 20", ""};
    Option<std::string> input21{this, "Input21", "Input 21", ""};
    Option<std::string> output21{this, "Output21", "Output 21", ""};
    Option<std::string> input22{this, "Input22", "Input 22", ""};
    Option<std::string> output22{this, "Output22", "Output 22", ""};
    Option<std::string> input23{this, "Input23", "Input 23", ""};
    Option<std::string> output23{this, "Output23", "Output 23", ""};
    Option<std::string> input24{this, "Input24", "Input 24", ""};
    Option<std::string> output24{this, "Output24", "Output 24", ""};
    Option<std::string> input25{this, "Input25", "Input 25", ""};
    Option<std::string> output25{this, "Output25", "Output 25", ""};
    Option<std::string> input26{this, "Input26", "Input 26", ""};
    Option<std::string> output26{this, "Output26", "Output 26", ""};
    Option<std::string> input27{this, "Input27", "Input 27", ""};
    Option<std::string> output27{this, "Output27", "Output 27", ""};
    Option<std::string> input28{this, "Input28", "Input 28", ""};
    Option<std::string> output28{this, "Output28", "Output 28", ""};
    Option<std::string> input29{this, "Input29", "Input 29", ""};
    Option<std::string> output29{this, "Output29", "Output 29", ""};
    Option<std::string> input30{this, "Input30", "Input 30", ""};
    Option<std::string> output30{this, "Output30", "Output 30", ""};
);

FCITX_CONFIGURATION(
    SchnelleUmlauteConfig,
    Option<DelayConfig> delay{this, "Delay", "Delay"};
    Option<LeaderConfig> leader{this, "Leader", "Leader Keys"};
    Option<MappingsConfig> mappings{this, "Mappings", "Mappings"};
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
        // IntConstrainWithStep rejects out-of-range delay values (uses default).
        // Custom leader key is sanitized at runtime in applyConfig().
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
        if (unicode > 0 && unicode <= kMaxUnicodeCodepoint) {
            keyChar = utf8::UCS4ToUTF8(unicode);
        }

        // Pure modifier key presses (Shift, Ctrl, Alt, Super, etc.)
        // pass through without affecting gesture state.
        // Only Modifier+Key combinations trigger the modifier check below.
        if (isPress && key.sym() >= FcitxKey_Shift_L && key.sym() <= FcitxKey_Hyper_R) {
            // Allow Alt_L/Alt_R through as leader when configured and gesture active.
            // ISO_Level3_Shift (AltGr on EU layouts, 0xfe03) is outside this range
            // and passes through naturally.
            if (*config_.leader->alt &&
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
    // Apply in-memory config: rebuild mappings, sanitize custom key, log.
    // Shared by setConfig (values already loaded) and reloadConfig (read from disk).
    void applyConfig() {
        loadMappingsFromConfig();

        // Sanitize custom leader key: trim whitespace, keep only first
        // UTF-8 character.  Cached for runtime use — the config file
        // stores the original value so the UI round-trips correctly.
        cachedCustomKey_ = sanitizeCustomKey(*config_.leader->customKey);

        std::string leaders;
        if (*config_.leader->space) leaders += "Space ";
        if (*config_.leader->left) leaders += "Left ";
        if (*config_.leader->right) leaders += "Right ";
        if (*config_.leader->up) leaders += "Up ";
        if (*config_.leader->down) leaders += "Down ";
        if (*config_.leader->alt) leaders += "Alt/AltGr ";
        if (!cachedCustomKey_.empty()) leaders += "Custom('" + cachedCustomKey_ + "') ";
        if (leaders.empty()) leaders = "None ";

        FCITX_INFO() << "Schnelle: Config loaded - DelayLowercase=" << *config_.delay->lowercase
                     << "ms, DelayUppercase=" << *config_.delay->uppercase
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
    // Empty segments between commas are skipped: "a,,b" → ["a", "b"].
    // A literal comma cannot be used as output.
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

        const auto *m = &*config_.mappings;
        addMapping(*m->input1, *m->output1);
        addMapping(*m->input2, *m->output2);
        addMapping(*m->input3, *m->output3);
        addMapping(*m->input4, *m->output4);
        addMapping(*m->input5, *m->output5);
        addMapping(*m->input6, *m->output6);
        addMapping(*m->input7, *m->output7);
        addMapping(*m->input8, *m->output8);
        addMapping(*m->input9, *m->output9);
        addMapping(*m->input10, *m->output10);
        addMapping(*m->input11, *m->output11);
        addMapping(*m->input12, *m->output12);
        addMapping(*m->input13, *m->output13);
        addMapping(*m->input14, *m->output14);
        addMapping(*m->input15, *m->output15);
        addMapping(*m->input16, *m->output16);
        addMapping(*m->input17, *m->output17);
        addMapping(*m->input18, *m->output18);
        addMapping(*m->input19, *m->output19);
        addMapping(*m->input20, *m->output20);
        addMapping(*m->input21, *m->output21);
        addMapping(*m->input22, *m->output22);
        addMapping(*m->input23, *m->output23);
        addMapping(*m->input24, *m->output24);
        addMapping(*m->input25, *m->output25);
        addMapping(*m->input26, *m->output26);
        addMapping(*m->input27, *m->output27);
        addMapping(*m->input28, *m->output28);
        addMapping(*m->input29, *m->output29);
        addMapping(*m->input30, *m->output30);
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
        if (*config_.leader->alt && isAltLeaderSym(sym)) {
            return true;
        }

        // Check custom leader key (sanitized at config load)
        if (!cachedCustomKey_.empty() && !keyChar.empty() && keyChar == cachedCustomKey_) {
            return true;
        }

        // Check individual leader key toggles
        if (*config_.leader->space && sym == FcitxKey_space) return true;
        if (*config_.leader->left && sym == FcitxKey_Left) return true;
        if (*config_.leader->right && sym == FcitxKey_Right) return true;
        if (*config_.leader->up && sym == FcitxKey_Up) return true;
        if (*config_.leader->down && sym == FcitxKey_Down) return true;

        return false;
    }

    int getEffectiveDelay(const SchnelleUmlauteState *state) const {
        if (!state->waitingKey_) return *config_.delay->lowercase;
        bool isUpper = state->waitingKey_->length() == 1 &&
                       std::isupper(static_cast<unsigned char>((*state->waitingKey_)[0]));
        return isUpper ? *config_.delay->uppercase : *config_.delay->lowercase;
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

    // Sanitize custom leader key: trim whitespace, keep only first UTF-8
    // character.  Only a single key is valid — spaces would silently shadow
    // the Space toggle, and multi-char strings would never match a keypress.
    static std::string sanitizeCustomKey(const std::string &raw) {
        size_t start = raw.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = raw.find_last_not_of(" \t\n\r");
        std::string trimmed = raw.substr(start, end - start + 1);
        if (trimmed.length() > 1) {
            unsigned char first = static_cast<unsigned char>(trimmed[0]);
            size_t charLen = 1;
            if (first >= 0xF0) charLen = 4;
            else if (first >= 0xE0) charLen = 3;
            else if (first >= 0xC0) charLen = 2;
            if (charLen <= trimmed.length()) {
                trimmed = trimmed.substr(0, charLen);
            }
        }
        return trimmed;
    }

    Instance *instance_;
    SchnelleUmlauteConfig config_;
    FactoryFor<SchnelleUmlauteState> factory_;

    // Mappings (shared across all InputContexts, read-only after config load)
    std::unordered_map<std::string, std::vector<std::string>> umlautMap_;
    // Sanitized custom leader key (trimmed, single UTF-8 char)
    std::string cachedCustomKey_;
};

class SchnelleUmlauteEngineFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        return new SchnelleUmlauteEngine(manager->instance());
    }
};

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::SchnelleUmlauteEngineFactory)
