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
#include <fcitx-utils/standardpaths.h>
#include <fcitx-utils/fs.h>
#include <fcitx-config/configuration.h>
#include <fcitx-config/iniparser.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <algorithm>

namespace fcitx {

constexpr uint32_t kMaxUnicodeCodepoint = 0x10FFFF;
constexpr uint64_t kMicrosecondsPerSecond = 1'000'000;
constexpr uint64_t kNanosecondsPerMicrosecond = 1'000;
constexpr uint64_t kMicrosecondsPerMillisecond = 1'000;

constexpr int kDelayMin = 50;
constexpr int kDelayMax = 2000;
constexpr int kDelayStep = 25;
constexpr int kDeferredCommitDelayMs = 5;

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
    LeaderConfig,
    Option<bool> space{this, "Space", "Space", true};
    Option<bool> left{this, "Left", "Left Arrow", false};
    Option<bool> right{this, "Right", "Right Arrow", false};
    Option<bool> up{this, "Up", "Up Arrow", false};
    Option<bool> down{this, "Down", "Down Arrow", false};
    Option<bool> alt{this, "Alt", "\xe2\x9a\xa0 experimental \xe2\x80\x93 Alt/AltGr", false};
    Option<bool> customKeyEnabled{this, "CustomKeyEnabled",
        "\xe2\x9a\xa0 Custom Leader 1", false};
    OptionWithAnnotation<std::string, PlaceholderAnnotation> customKey{
        this, "CustomKey", "  \xe2\x86\xb3 Key", "",
        {}, {}, PlaceholderAnnotation("e.g. ; or #", true,
            "Single character. Must not be a mapped input key.")};
    Option<bool> customKey2Enabled{this, "CustomKey2Enabled",
        "\xe2\x9a\xa0 Custom Leader 2 (hand-split)", false};
    OptionWithAnnotation<std::string, PlaceholderAnnotation> customKey2{
        this, "CustomKey2", "  \xe2\x86\xb3 Key", "",
        {}, {}, PlaceholderAnnotation("e.g. j or f", true,
            "Single character on the opposite keyboard half of Leader 1.")};
);

FCITX_CONFIGURATION(
    SchnelleUmlauteConfig,
    Option<DelayConfig> delay{this, "Delay", "Delay"};
    Option<LeaderConfig> leader{this, "Leader", "Leader Keys"};
    ExternalOption mappingEditor{this, "MappingEditor", "Mapping Editor",
        "fcitx://config/addon/schnelle-umlaute/mappings.txt"};
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

    // Active Alt-led cycling session. Set when Alt starts cycling,
    // cleared only by deferred commit timer or clearAllState().
    // Survives auto-repeat release-press gaps on KWin Wayland where
    // cycling is temporarily reset between pairs.
    bool altGestureSession_ = false;

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
        altGestureSession_ = false;
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

    void setSubConfig(const std::string &path,
                      const RawConfig &config) override {
        if (path == "mappings.txt") {
            // If RawConfig contains mapping data, use it directly.
            // Otherwise, reload from file (normal configtool path).
            umlautMap_.clear();
            for (int i = 0; ; ++i) {
                auto input = config.valueByPath(
                    std::to_string(i) + "/Input");
                auto output = config.valueByPath(
                    std::to_string(i) + "/Output");
                if (!input || input->empty()) break;
                if (output && !output->empty()) {
                    umlautMap_[*input] = splitOutputs(*output);
                }
            }
            if (umlautMap_.empty()) {
                loadMappingsFromFile();
            }
            FCITX_INFO() << "Schnelle: Mappings reloaded, count="
                         << umlautMap_.size();
        }
    }

    std::vector<InputMethodEntry> listInputMethods() override {
        std::vector<InputMethodEntry> methods;
        InputMethodEntry entry("schnelle-umlaute", "Schnelle Umlaute", "de", "schnelle-umlaute");
        entry.setIcon("input-keyboard").setLabel("ä").setConfigurable(false);
        methods.push_back(std::move(entry));
        return methods;
    }

    void keyEvent(const InputMethodEntry & /*entry*/, KeyEvent &keyEvent) override {
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
                // Don't clear consumedAltCode_ here — during KWin Wayland
                // auto-repeat gaps, no gesture is active but the Alt session
                // may still be ongoing. Cleared via clearAllState() or when
                // a non-gesture key is pressed.
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
                // Don't clear consumedAltCode_ here. On KWin Wayland,
                // Alt auto-repeat sends release-press pairs — clearing on
                // release leaves a gap where input key events leak through
                // hasModifiers. Cleared when Alt arrives outside a gesture
                // (pure modifier handler / leader key handler) or via
                // clearAllState().
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

                if (state->altGestureSession_) {
                    // Alt-led gesture on KWin Wayland: defer commit.
                    // Auto-repeat sends release-press pairs; committing here
                    // would destroy cycling state. A short timer distinguishes
                    // auto-repeat from real release.
                    state->inputKeyPressed_ = false;
                    scheduleDeferredCyclingCommit(ic, state);
                    keyEvent.filterAndAccept();
                    return;
                }

                // Non-Alt leader: commit immediately
                auto it = umlautMap_.find(*state->cyclingInput_);
                if (it != umlautMap_.end() && state->cyclingIndex_ < it->second.size()) {
                    ic->inputPanel().reset();
                    ic->commitString(it->second[state->cyclingIndex_]);
                    ic->updatePreedit();
                    state->recentlyCommitted_ = true;
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
            // When Alt is the leader key and a gesture is active, ignore
            // Alt-only modifier state — input key repeats with Alt held
            // should not commit the gesture and leak through.
            bool altLeaderBypass = *config_.leader->alt &&
                (state->waitingKey_ || state->cyclingInput_ ||
                 state->consumedAltCode_ != 0 || state->altGestureSession_);
            if (altLeaderBypass) {
                KeyStates mods = key.states();
                altLeaderBypass = mods.test(KeyState::Alt) &&
                                  !mods.test(KeyState::Ctrl) &&
                                  !mods.test(KeyState::Super);
            }
            if (!altLeaderBypass) {
                // Commit any pending preedit before letting the shortcut through
                commitPendingKey(ic, state);
                commitCyclingValue(ic, state);
                state->inputKeyPressed_ = false;
                return;  // Let the shortcut through
            }
            // Alt-only during gesture: fall through to normal key handling
        }

        // Re-press of gesture key during deferred Alt cycling commit.
        // On KWin Wayland, auto-repeat sends release-press pairs. The
        // release deferred the commit; this re-press cancels it and
        // continues cycling — no filterAndAccept needed for the key
        // because cycling stays in preedit without state transitions.
        if (state->altGestureSession_ && state->cyclingInput_ &&
            !state->inputKeyPressed_ && state->waitingKeyCode_ != 0 &&
            rawCode == state->waitingKeyCode_) {
            state->cancelTimeout();
            state->inputKeyPressed_ = true;
            keyEvent.filterAndAccept();
            return;
        }

        // =========================================
        // HANDLE LEADER KEY (Space/Arrows/Alt/Custom)
        // =========================================
        auto leaderType = classifyLeader(key, keyChar);

        // Dual custom leader split: downgrade to None if this leader is
        // not allowed for the currently active input key's keyboard half.
        if (leaderType != LeaderType::None) {
            std::string activeInput;
            if (state->cyclingInput_) activeInput = *state->cyclingInput_;
            else if (state->waitingKey_) activeInput = *state->waitingKey_;

            if (!activeInput.empty() &&
                !isDualCustomAllowed(leaderType, activeInput)) {
                leaderType = LeaderType::None;
            }
        }

        if (leaderType != LeaderType::None) {
            bool isAlt = isAltLeaderSym(key.sym());

            // CASE 1: Currently in cycling mode
            if (state->cyclingInput_) {
                // Check if input key is still pressed
                if (!state->inputKeyPressed_) {
                    // During Alt-led gesture, the input key may be in a
                    // Wayland auto-repeat gap (release-press pair). The
                    // deferred commit timer handles the real release.
                    if (!(isAlt && state->altGestureSession_)) {
                        state->resetCycling();
                        return;  // Let leader through
                    }
                }

                auto it = umlautMap_.find(*state->cyclingInput_);
                if (it != umlautMap_.end()) {
                    if (it->second.size() > 1) {
                        // Cycle to next variant
                        state->cyclingIndex_ = (state->cyclingIndex_ + 1) % it->second.size();
                        updateClientPreedit(ic, it->second[state->cyclingIndex_]);
                    }
                    // Single-output: preedit unchanged, just consume the leader.

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

                    // Alt leader: keep ALL outputs in preedit (even single).
                    // KWin Wayland doesn't reliably suppress key events with
                    // Alt modifier state via filterAndAccept. Keeping the
                    // value in preedit lets the deferred commit and re-press
                    // detection machinery protect against auto-repeat leaks.
                    // Non-Alt leaders: single output commits directly.
                    if (it->second.size() > 1 || isAlt) {
                        state->cyclingInput_ = *state->waitingKey_;
                        state->cyclingIndex_ = 0;
                        if (isAlt) state->altGestureSession_ = true;

                        // Update preedit with first variant
                        updateClientPreedit(ic, it->second[0]);
                    } else {
                        // Single output with non-Alt leader - commit directly
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

            // Not in gesture - clear stale consumedAltCode_ (covers
            // AltGr/ISO_Level3_Shift which bypasses the modifier range check)
            // and let leader key through.
            if (state->consumedAltCode_ != 0) {
                state->consumedAltCode_ = 0;
            }
            return;
        }

        // rawCode-based repeat suppression during active gesture.
        // When Alt is held as leader, some backends change the keysym
        // (e.g. "1" → symbol), so keyChar-based checks below may not
        // match the gesture key. Fall back to physical rawCode.
        if (!isNewKeyPress && state->waitingKeyCode_ != 0 &&
            rawCode == state->waitingKeyCode_ &&
            (state->waitingKey_ || state->cyclingInput_)) {
            keyEvent.filterAndAccept();
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
        // Clean up stale Alt gesture state
        state->altGestureSession_ = false;
        state->consumedAltCode_ = 0;
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
        loadMappingsFromFile();

        // Sanitize custom leader key: trim whitespace, keep only first
        // UTF-8 character.  Cached for runtime use — the config file
        // stores the original value so the UI round-trips correctly.
        cachedCustomKey_ = *config_.leader->customKeyEnabled
            ? sanitizeCustomKey(*config_.leader->customKey) : "";
        cachedCustomKey2_ = *config_.leader->customKey2Enabled
            ? sanitizeCustomKey(*config_.leader->customKey2) : "";

        // Warn if a custom leader key collides with a mapped input
        if (!cachedCustomKey_.empty() && umlautMap_.count(cachedCustomKey_)) {
            FCITX_WARN() << "Schnelle: CustomKey '" << cachedCustomKey_
                         << "' is also a mapped input"
                         << " — it cannot trigger its own mapping";
        }
        if (!cachedCustomKey2_.empty() && umlautMap_.count(cachedCustomKey2_)) {
            FCITX_WARN() << "Schnelle: CustomKey2 '" << cachedCustomKey2_
                         << "' is also a mapped input"
                         << " — it cannot trigger its own mapping";
        }

        // Warn about dual custom leader conflicts
        if (!cachedCustomKey_.empty() && !cachedCustomKey2_.empty()) {
            if (cachedCustomKey_ == cachedCustomKey2_) {
                FCITX_WARN() << "Schnelle: CustomKey and CustomKey2 are identical"
                             << " — dual split disabled, both trigger all mappings";
            } else if (isLeftHandUSQwerty(cachedCustomKey_) ==
                       isLeftHandUSQwerty(cachedCustomKey2_)) {
                FCITX_WARN() << "Schnelle: CustomKey '" << cachedCustomKey_
                             << "' and CustomKey2 '" << cachedCustomKey2_
                             << "' are on the same keyboard half"
                             << " — dual split disabled, both trigger all mappings";
            }
        }

        std::string leaders;
        if (*config_.leader->space) leaders += "Space ";
        if (*config_.leader->left) leaders += "Left ";
        if (*config_.leader->right) leaders += "Right ";
        if (*config_.leader->up) leaders += "Up ";
        if (*config_.leader->down) leaders += "Down ";
        if (*config_.leader->alt) leaders += "Alt/AltGr ";
        if (!cachedCustomKey_.empty()) leaders += "Custom1('" + cachedCustomKey_ + "') ";
        if (!cachedCustomKey2_.empty()) leaders += "Custom2('" + cachedCustomKey2_ + "') ";
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

    // Deferred cycling commit for Alt-led gestures on KWin Wayland.
    // Auto-repeat sends release-press pairs; committing on release would
    // destroy cycling state. Instead, wait 5ms — if a re-press arrives,
    // it cancels this timer and cycling continues. If not (real release),
    // the timer fires and commits the cycling value.
    void scheduleDeferredCyclingCommit(InputContext *ic, SchnelleUmlauteState *state) {
        state->cancelTimeout();

        auto savedRef = ic->watch();
        auto *eventLoop = &instance_->eventLoop();
        uint64_t now = SchnelleUmlauteState::nowUsec();
        uint64_t target = now + kDeferredCommitDelayMs * kMicrosecondsPerMillisecond;

        state->timeoutEvent_ = eventLoop->addTimeEvent(
            CLOCK_MONOTONIC, target, 0,
            [state, savedRef, this](EventSourceTime *, uint64_t) {
                // Safety: see scheduleTimeout — single-threaded event loop
                // guarantees state outlives savedRef.get() != nullptr.
                auto *ctx = savedRef.get();
                if (!ctx) return false;

                if (state->cyclingInput_) {
                    auto it = umlautMap_.find(*state->cyclingInput_);
                    if (it != umlautMap_.end() && state->cyclingIndex_ < it->second.size()) {
                        ctx->inputPanel().reset();
                        ctx->commitString(it->second[state->cyclingIndex_]);
                        ctx->updatePreedit();
                        state->recentlyCommitted_ = true;
                    }
                    state->resetCycling();
                    state->waitingKeyCode_ = 0;
                }
                state->altGestureSession_ = false;
                state->consumedAltCode_ = 0;
                return false;
            }
        );
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
        state->cancelTimeout();  // Cancel any deferred commit timer
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
    // Comma is the separator between cycling variants: "a,b" → ["a", "b"].
    // Double comma escapes a literal comma: "a,,b" → ["a,b"].
    // Empty segments are skipped: "a,,,b" → ["a", ",b"] (escape wins).
    std::vector<std::string> splitOutputs(const std::string &output) {
        std::vector<std::string> outputs;
        if (output.empty()) return outputs;

        std::string current;
        for (size_t i = 0; i < output.length(); ++i) {
            if (output[i] == ',') {
                if (i + 1 < output.length() && output[i + 1] == ',') {
                    current += ',';
                    ++i;
                } else {
                    if (!current.empty()) {
                        outputs.push_back(std::move(current));
                        current.clear();
                    }
                }
            } else {
                current += output[i];
            }
        }
        if (!current.empty()) {
            outputs.push_back(std::move(current));
        }
        return outputs;
    }

    void loadMappingsFromFile() {
        umlautMap_.clear();
        auto file = StandardPaths::global().open(
            StandardPathsType::PkgConfig, "schnelle-umlaute/mappings.txt");
        if (file.isValid()) {
            auto fp = fs::openFD(file, "r");
            if (fp) {
                char buf[4096];
                while (fgets(buf, sizeof(buf), fp.get())) {
                    std::string line(buf);
                    // Trim trailing newline
                    while (!line.empty() &&
                           (line.back() == '\n' || line.back() == '\r')) {
                        line.pop_back();
                    }
                    if (line.empty() || line[0] == '#') continue;
                    // Format: first char = input, '=' separator, rest = output
                    if (line.size() >= 3 && line[1] == '=') {
                        auto input = line.substr(0, 1);
                        auto output = line.substr(2);
                        if (!output.empty()) {
                            umlautMap_[input] = splitOutputs(output);
                        }
                    }
                }
            }
        }
        if (umlautMap_.empty()) {
            // Default mappings
            umlautMap_["a"] = {"\xc3\xa4"};
            umlautMap_["o"] = {"\xc3\xb6"};
            umlautMap_["u"] = {"\xc3\xbc"};
            umlautMap_["s"] = {"\xc3\x9f"};
            umlautMap_["A"] = {"\xc3\x84"};
            umlautMap_["O"] = {"\xc3\x96"};
            umlautMap_["U"] = {"\xc3\x9c"};
        }
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

    // Case-insensitive match for ASCII letters, exact match otherwise.
    // Allows Shift+f to match custom leader "f" so uppercase mappings
    // work naturally while holding Shift (e.g. Shift+O + Shift+F → Ö).
    static bool matchCustomKey(const std::string &keyChar, const std::string &customKey) {
        if (keyChar == customKey) return true;
        if (keyChar.size() == 1 && customKey.size() == 1) {
            char a = keyChar[0], b = customKey[0];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a >= 'a' && a <= 'z') return a == b;
        }
        return false;
    }

    // Leader classification for dual custom leader support.
    // Built-in leaders (Space, Arrows, Alt) are unrestricted.
    // Custom1/Custom2 may be restricted by dual-split logic.
    enum class LeaderType { None, BuiltIn, Custom1, Custom2 };

    LeaderType classifyLeader(const Key &key, const std::string &keyChar) const {
        KeySym sym = key.sym();

        // Alt/AltGr — built-in, unrestricted
        if (*config_.leader->alt && isAltLeaderSym(sym))
            return LeaderType::BuiltIn;

        // Custom Key 1 (sanitized at config load, case-insensitive for letters)
        if (!cachedCustomKey_.empty() && !keyChar.empty() &&
            matchCustomKey(keyChar, cachedCustomKey_))
            return LeaderType::Custom1;

        // Custom Key 2
        if (!cachedCustomKey2_.empty() && !keyChar.empty() &&
            matchCustomKey(keyChar, cachedCustomKey2_))
            return LeaderType::Custom2;

        // Built-in leader toggles
        if (*config_.leader->space && sym == FcitxKey_space) return LeaderType::BuiltIn;
        if (*config_.leader->left && sym == FcitxKey_Left) return LeaderType::BuiltIn;
        if (*config_.leader->right && sym == FcitxKey_Right) return LeaderType::BuiltIn;
        if (*config_.leader->up && sym == FcitxKey_Up) return LeaderType::BuiltIn;
        if (*config_.leader->down && sym == FcitxKey_Down) return LeaderType::BuiltIn;

        return LeaderType::None;
    }

    // US QWERTY hand classification for dual custom leader split.
    // Left hand: qwertasdfgzxcvb + digits 1-5 + symbols `~!@#$%
    // Everything else (including non-ASCII) defaults to right hand.
    static bool isLeftHandUSQwerty(const std::string &key) {
        if (key.size() != 1) return false;
        char c = key[0];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        return c == 'q' || c == 'w' || c == 'e' || c == 'r' || c == 't' ||
               c == 'a' || c == 's' || c == 'd' || c == 'f' || c == 'g' ||
               c == 'z' || c == 'x' || c == 'c' || c == 'v' || c == 'b' ||
               c == '`' || c == '1' || c == '2' || c == '3' || c == '4' ||
               c == '5' || c == '~' || c == '!' || c == '@' || c == '#' ||
               c == '$' || c == '%';
    }

    // Dual custom leader split: when BOTH custom keys are set and on
    // opposite hands, each only triggers inputs on the OTHER hand.
    // Single custom key or same-hand keys → no restriction.
    // Built-in leaders always unrestricted.
    bool isDualCustomAllowed(LeaderType leader, const std::string &inputKey) const {
        if (leader == LeaderType::BuiltIn || leader == LeaderType::None)
            return true;

        // Dual mode only when BOTH custom keys are set
        if (cachedCustomKey_.empty() || cachedCustomKey2_.empty())
            return true;

        // Identical keys → no split
        if (cachedCustomKey_ == cachedCustomKey2_)
            return true;

        bool key1Left = isLeftHandUSQwerty(cachedCustomKey_);
        bool key2Left = isLeftHandUSQwerty(cachedCustomKey2_);

        // Both keys on same hand → no split possible, allow all
        if (key1Left == key2Left) return true;

        bool inputLeft = isLeftHandUSQwerty(inputKey);

        // Left-hand leader triggers RIGHT-hand inputs (and vice versa)
        if (leader == LeaderType::Custom1)
            return key1Left ? !inputLeft : inputLeft;
        else  // Custom2
            return key2Left ? !inputLeft : inputLeft;
    }

    int getEffectiveDelay(const SchnelleUmlauteState *state) const {
        if (!state->waitingKey_) return *config_.delay->lowercase;
        bool isUpper = state->waitingKey_->length() == 1 &&
                       (*state->waitingKey_)[0] >= 'A' && (*state->waitingKey_)[0] <= 'Z';
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
                // Safety: state is owned by IC (InputContextProperty). If IC
                // is destroyed, savedRef.get() returns nullptr and we bail
                // before touching state. No race: fcitx5's event loop is
                // single-threaded, so IC can't be destroyed between the
                // check and the access.
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
        if (!utf8::validate(trimmed)) return "";
        size_t firstCharBytes = utf8::ncharByteLength(trimmed.begin(), 1);
        return trimmed.substr(0, firstCharBytes);
    }

    Instance *instance_;
    SchnelleUmlauteConfig config_;
    FactoryFor<SchnelleUmlauteState> factory_;

    // Mappings (shared across all InputContexts, read-only after config load)
    std::unordered_map<std::string, std::vector<std::string>> umlautMap_;
    // Sanitized custom leader keys (trimmed, single UTF-8 char each)
    std::string cachedCustomKey_;
    std::string cachedCustomKey2_;
};

class SchnelleUmlauteEngineFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        return new SchnelleUmlauteEngine(manager->instance());
    }
};

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::SchnelleUmlauteEngineFactory)
