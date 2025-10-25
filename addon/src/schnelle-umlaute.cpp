#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx/inputcontext.h>
#include <fcitx-utils/utf8.h>
#include <chrono>
#include <string>
#include <unordered_map>

namespace fcitx {

class SchnelleUmlauteEngine : public InputMethodEngineV2 {
public:
    SchnelleUmlauteEngine(Instance *instance)
        : instance_(instance), enabled_(true), delay_ms_(400), delay_ms_upper_(700) {

        // Initialize umlaut map
        umlautMap_["a"] = "ä";
        umlautMap_["o"] = "ö";
        umlautMap_["u"] = "ü";
        umlautMap_["s"] = "ß";
        umlautMap_["A"] = "Ä";
        umlautMap_["O"] = "Ö";
        umlautMap_["U"] = "Ü";
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
            // Accent key pressed: suppress and start timer
            waitingKey_ = keyChar;
            startTime_ = std::chrono::steady_clock::now();
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
                keyEvent.filterAndAccept();
                return;
            }
        }

        // Key released or timeout
        if (waitingKey_) {
            if (!isPress && keyChar == *waitingKey_) {
                // Waiting key released: commit normal character
                commitWaitingKey(keyEvent.inputContext());
                keyEvent.filterAndAccept();
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
    }

    void enable() {
        enabled_ = true;
    }

    void disable() {
        enabled_ = false;
        waitingKey_.reset();
    }

    void setDelay(int delayMs) {
        delay_ms_ = delayMs;
    }

private:
    bool isTimeoutExpired() const {
        if (!waitingKey_) return false;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - startTime_).count();

        // Use longer delay for uppercase letters (requires holding Shift)
        bool isUpperCase = waitingKey_->length() == 1 && std::isupper((*waitingKey_)[0]);
        int effectiveDelay = isUpperCase ? delay_ms_upper_ : delay_ms_;

        return elapsed > effectiveDelay;
    }

    void commitWaitingKey(InputContext *ic) {
        if (waitingKey_) {
            ic->commitString(*waitingKey_);
            waitingKey_.reset();
        }
    }

    Instance *instance_;
    bool enabled_;
    int delay_ms_;
    int delay_ms_upper_;

    // Hold & Wait state
    std::optional<std::string> waitingKey_;
    std::chrono::steady_clock::time_point startTime_;

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
