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
);

class SchnelleUmlauteEngine : public InputMethodEngineV2 {
public:
    SchnelleUmlauteEngine(Instance *instance)
        : instance_(instance), enabled_(true) {

        // Initialize umlaut map
        umlautMap_["a"] = "ä";
        umlautMap_["o"] = "ö";
        umlautMap_["u"] = "ü";
        umlautMap_["s"] = "ß";
        umlautMap_["A"] = "Ä";
        umlautMap_["O"] = "Ö";
        umlautMap_["U"] = "Ü";

        // Load configuration
        reloadConfig();
    }

    const Configuration *getConfig() const override { return &config_; }
    void setConfig(const RawConfig &config) override {
        config_.load(config);

        // Validate and clamp values to valid range (50-2000ms) and round to 25ms steps
        auto clampAndRound = [](int value, int min, int max, int step) {
            value = std::max(min, std::min(max, value));
            // Round to nearest step
            int remainder = value % step;
            if (remainder < step / 2) {
                value -= remainder;
            } else {
                value += (step - remainder);
            }
            return value;
        };

        config_.delayLowercase.setValue(clampAndRound(*config_.delayLowercase, 50, 2000, 25));
        config_.delayUppercase.setValue(clampAndRound(*config_.delayUppercase, 50, 2000, 25));

        safeSaveAsIni(config_, "conf/schnelle-umlaute.conf");
        reloadConfig();
    }

    void reloadConfig() override {
        readAsIni(config_, "conf/schnelle-umlaute.conf");
        FCITX_INFO() << "Schnelle: Config loaded - DelayLowercase=" << *config_.delayLowercase
                     << "ms, DelayUppercase=" << *config_.delayUppercase << "ms";
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

        // Check if Space key is pressed (ignore modifiers like Shift)
        if (waitingKey_ && key.sym() == FcitxKey_space && isPress) {
            // Space pressed while waiting: check if within delay
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
