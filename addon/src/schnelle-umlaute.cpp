#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/textformatflags.h>
#include <fcitx-utils/trackableobject.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/userinterface.h>
#include <xkbcommon/xkbcommon.h>
#include "app_filter.h"
#include "config.h"
#include "hand_classifier.h"
#include "mappings_loader.h"
#include "profile_paths.h"
#include "overlay/cursor_overlay_geometry.h"
#include "overlay_client.h"
#include "state.h"

namespace fcitx {

constexpr uint32_t kMaxUnicodeCodepoint = 0x10FFFF;

// =============================================================================
// VelocityAccents-Style Implementation
// =============================================================================
// Key insight: Track whether input key is PHYSICALLY PRESSED
// - Cycling only works while input key is held down
// - No timer during cycling - cycle as long as you want
// - When input key is released, cycling ends
// =============================================================================

class SchnelleUmlauteEngine final : public InputMethodEngineV2 {
public:
    SchnelleUmlauteEngine(Instance *instance)
        : instance_(instance),
          factory_([](InputContext &) { return new SchnelleUmlauteState; }) {
        instance_->inputContextManager().registerProperty(
            "schnelle-umlaute-state", &factory_);

        // Build XKB keymap from system defaults so we can resolve the
        // unshifted (Level 0) character for any physical key.  This lets
        // Shift+symbol custom leaders work (e.g. Shift+/ → '?' is
        // resolved back to '/' via the keymap).
        xkbCtx_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        if (xkbCtx_) {
            xkbKeymap_ = xkb_keymap_new_from_names(xkbCtx_, nullptr,
                                                   XKB_KEYMAP_COMPILE_NO_FLAGS);
            if (!xkbKeymap_) {
                FCITX_WARN() << "Schnelle: XKB keymap creation failed"
                             << " — custom leader resolution disabled";
            }
        } else {
            FCITX_WARN() << "Schnelle: XKB context creation failed"
                         << " — custom leader resolution disabled";
        }
        handClassifier_.build(xkbKeymap_);

        reloadConfig();
    }

    ~SchnelleUmlauteEngine() {
        if (xkbKeymap_)
            xkb_keymap_unref(xkbKeymap_);
        if (xkbCtx_)
            xkb_context_unref(xkbCtx_);
    }

    // Returns a single-ExternalOption config so fcitx5-config-qt / KDE KCM
    // hit the "only external" fast path and launch schnelle-umlaute-editor
    // directly. The real settings still live in config_ and are loaded
    // from disk by reloadConfig() or from a caller-supplied RawConfig by
    // setConfig() — see below.
    const Configuration *getConfig() const override { return &externalConfig_; }
    // Called by programmatic writers (fcitx5-remote, the testfrontend, or
    // a future tool using SetConfig on DBus). The gear-icon path never
    // arrives here any more, because configtool fast-paths on the
    // single-ExternalOption shape exposed by getConfig(). Keep the full
    // load/sanitize/save pipeline so RawConfig-based callers still work.
    void setConfig(const RawConfig &config) override {
        config_.load(config);
        normalizeCustomLeaders();
        safeSaveAsIni(config_, "conf/schnelle-umlaute.conf");
        applyConfig();
    }

    void reloadConfig() override {
        readAsIni(config_, "conf/schnelle-umlaute.conf");
        normalizeCustomLeaders();
        // Profiles live in a separate file owned by the editor's
        // ProfileListModel (kept out of schnelle-umlaute.conf, which the editor
        // fully rewrites). An absent/empty list is the pre-profiles state;
        // activeProfileFile() then falls back to the legacy mappings.txt.
        readAsIni(profiles_, std::string(schnelle_umlaute::kConfigSubdir) +
                                 "/" + schnelle_umlaute::kProfilesConf);
        applyConfig();
    }

    void setSubConfig(const std::string &path,
                      const RawConfig &config) override {
        if (path == "mappings.txt") {
            // Cancel active gestures on all ICs FIRST so the rebuild below
            // starts from quiescent state. fcitx5 is single-threaded on the
            // event loop, so no timer can fire mid-rebuild, but the foreach
            // itself runs filter pipelines (resetIC, updatePreedit) that
            // can hit umlautMap_ via observer paths — doing the wipe before
            // the rebuild keeps those reads consistent. Also avoids leaving
            // ICs in a cycling state that references a now-removed entry
            // (e.g. mapping shortened from "ä,ae" to "ä" while held).
            instance_->inputContextManager().foreach([this](InputContext *ic) {
                auto *s = ic->propertyFor(&factory_);
                s->clearAllState();
                s->recentlyCommitted_ = false;
                ic->inputPanel().reset();
                ic->updatePreedit();
                return true;
            });
            // If RawConfig contains mapping data, use it directly.
            // Otherwise, reload from file (normal configtool path).
            umlautMap_.clear();
            for (int i = 0;; ++i) {
                auto input = config.valueByPath(std::to_string(i) + "/Input");
                auto output = config.valueByPath(std::to_string(i) + "/Output");
                if (!input || input->empty())
                    break;
                if (output && !output->empty()) {
                    auto outputs = schnelle_umlaute::splitOutputs(*output);
                    if (outputs.empty()) {
                        FCITX_WARN() << "Schnelle: Mapping '" << *input
                                     << "' has no valid outputs"
                                     << " — skipped";
                        continue;
                    }
                    umlautMap_[*input] = std::move(outputs);
                }
            }
            if (umlautMap_.empty()) {
                umlautMap_ =
                    schnelle_umlaute::loadMappingsFromFile(activeProfileFile());
            }
            FCITX_INFO() << "Schnelle: Mappings reloaded, count="
                         << umlautMap_.size();
        }
    }

    void keyEvent(const InputMethodEntry & /*entry*/,
                  KeyEvent &keyEvent) override {
        auto *ic = keyEvent.inputContext();

        // App filter: let keys pass through in blacklisted/non-whitelisted apps
        if (appFilter_.isFiltered(ic))
            return;

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
        if (isPress && key.sym() >= FcitxKey_Shift_L &&
            key.sym() <= FcitxKey_Hyper_R) {
            // Allow Alt_L/Alt_R through as leader when configured and gesture
            // active. ISO_Level3_Shift (AltGr on EU layouts, 0xfe03) is outside
            // this range and passes through naturally.
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
            if (key.sym() == FcitxKey_space && !state->waitingKey_ &&
                !hasModifiers(key)) {
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
            if (state->consumedAltCode_ != 0 &&
                rawCode == state->consumedAltCode_) {
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
            if (state->committedKeyCode_ != 0 &&
                rawCode == state->committedKeyCode_) {
                state->committedKeyCode_ = 0;
                keyEvent.filterAndAccept();
                return;
            }

            // Check if releasing the cycling input key
            // Compare physical keycode so shifted chars (!, @, #) match their
            // base key
            if (state->cyclingInput_ && state->inputKeyPressed_ &&
                rawCode == state->waitingKeyCode_) {

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
                if (it != umlautMap_.end() &&
                    state->cyclingIndex_ < it->second.size()) {
                    ic->inputPanel().reset();
                    ic->commitString(it->second[state->cyclingIndex_]);
                    ic->updatePreedit();
                    state->recentlyCommitted_ = true;
                }

                state->inputKeyPressed_ = false;
                state->resetCycling();
                overlayHide();
                keyEvent.filterAndAccept();
                return;
            }

            // Check if releasing waiting key (before first Space)
            // PREEDIT: Commit the preedit as the original character
            // Compare physical keycode so shifted chars (!, @, #) and uppercase
            // letters match even if Shift is released first
            if (state->waitingKey_ && state->inputKeyPressed_ &&
                rawCode == state->waitingKeyCode_) {
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
        if (state->waitingKey_ &&
            state->isTimeoutExpired(getEffectiveDelay(state))) {
            std::string pending = *state->waitingKey_;
            ic->inputPanel().reset();
            ic->updatePreedit();
            state->waitingKey_.reset();
            state->waitingKeyCode_ = 0;
            state->cancelTimeout();
            state->inputKeyPressed_ = false;
            // Window elapsed (a key arrived right at expiry, before the
            // timeout timer fired): clear the trigger preview, mirroring the
            // timeout callback's teardown.
            hideTriggerOverlay(state);

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
        bool didAltBypass = false;
        if (hasModifiers(key)) {
            // When Alt is the leader key and a gesture is active, ignore
            // Alt-only modifier state — input key repeats with Alt held
            // should not commit the gesture and leak through.
            bool altLeaderBypass =
                *config_.leader->alt &&
                (state->waitingKey_ || state->cyclingInput_ ||
                 state->consumedAltCode_ != 0 || state->altGestureSession_);
            if (altLeaderBypass) {
                KeyStates mods = key.states();
                altLeaderBypass = mods.test(KeyState::Alt) &&
                                  !mods.test(KeyState::Ctrl) &&
                                  !mods.test(KeyState::Super);
            }
            if (!altLeaderBypass) {
                // Commit any pending preedit before letting the shortcut
                // through
                commitPendingKey(ic, state);
                commitCyclingValue(ic, state);
                state->inputKeyPressed_ = false;
                return; // Let the shortcut through
            }
            // Alt-only during gesture: resolve the physical key's base
            // character.  Some backends change the keysym when Alt is held
            // (e.g. number keys → symbols), which would prevent the accent
            // key handler from finding the mapping.  Using the Level 0
            // character from the XKB keymap ensures correct recognition.
            std::string baseChar = getBaseChar(rawCode);
            if (!baseChar.empty()) {
                keyChar = baseChar;
            }
            didAltBypass = true;
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
        auto leaderType = classifyLeader(key, keyChar, rawCode);

        // Dual custom leader split: downgrade to None if this leader is
        // not allowed for the currently active input key's keyboard half.
        if (leaderType != LeaderType::None) {
            std::string activeInput;
            if (state->cyclingInput_)
                activeInput = *state->cyclingInput_;
            else if (state->waitingKey_)
                activeInput = *state->waitingKey_;

            if (!activeInput.empty() &&
                !isDualCustomAllowed(leaderType, activeInput,
                                     state->waitingKeyCode_)) {
                leaderType = LeaderType::None;
            }
        }

        if (leaderType != LeaderType::None) {
            bool isAlt = isAltLeaderSym(key.sym());

            // A leader press ends the trigger-preview phase: cancel any pending
            // preview timer. If the preview is already visible, the cycling
            // logic below decides whether to keep it (multi-variant picker) or
            // hide it (single output).
            state->cancelOverlayShow();

            // MIN-HOLD GUARD (lower bound of the accent window)
            // Before cycling has started, a leader that arrives before the
            // minimum hold time has elapsed is not an accent trigger. Commit
            // the plain pending char now (plus a space for the Space leader,
            // in the same commitString so the order can't flip), then let the
            // leader act as a normal key. With min == 0 this never fires, so
            // the historic behavior is unchanged.
            if (!state->cyclingInput_ && state->waitingKey_ &&
                state->isBeforeMinHold(getEffectiveMinHold(state))) {
                std::string pending = *state->waitingKey_;
                // In progress mode the overlay is already up (shown at t=0), so
                // tear it down now that this turns into a plain commit.
                hideTriggerOverlay(state);
                ic->inputPanel().reset();
                ic->updatePreedit();
                state->waitingKey_.reset();
                // Arm auto-repeat suppression for the still-held input key.
                // Without this, the next auto-repeat of the held key would
                // start a fresh gesture and duplicate the character (the
                // "üu"-class bug guarded at the committedKeyCode_ check).
                state->committedKeyCode_ = state->waitingKeyCode_;
                state->waitingKeyCode_ = 0;
                state->cancelTimeout();
                state->inputKeyPressed_ = false;
                if (key.sym() == FcitxKey_space && !hasModifiers(key)) {
                    ic->commitString(pending + " ");
                    state->recentlyCommitted_ = true;
                    keyEvent.filterAndAccept();
                    return;
                }
                // Non-Space leader (arrow): commit the plain char and let the
                // leader through as a normal key. This mirrors the post-timeout
                // ordering guard above; the committed char and the raw leader
                // travel on separate XIM channels, so in theory they could
                // reorder in terminals like WezTerm (the #6 pattern), but only
                // for arrow leaders combined with a minimum hold, which is
                // rare.
                ic->commitString(pending);
                state->recentlyCommitted_ = true;
                return;
            }

            // CASE 1: Currently in cycling mode
            if (state->cyclingInput_) {
                // Check if input key is still pressed
                if (!state->inputKeyPressed_) {
                    // During Alt-led gesture, the input key may be in a
                    // Wayland auto-repeat gap (release-press pair). The
                    // deferred commit timer handles the real release.
                    if (!(isAlt && state->altGestureSession_)) {
                        state->resetCycling();
                        overlayHide();
                        return; // Let leader through
                    }
                }

                auto it = umlautMap_.find(*state->cyclingInput_);
                if (it != umlautMap_.end() && !it->second.empty()) {
                    if (it->second.size() > 1) {
                        // Cycle to next variant
                        state->cyclingIndex_ =
                            (state->cyclingIndex_ + 1) % it->second.size();
                        updateClientPreedit(ic,
                                            it->second[state->cyclingIndex_]);
                        overlayShow(ic, it->second,
                                    static_cast<int>(state->cyclingIndex_));
                    } else if (state->altGestureSession_ &&
                               !(isAlt && rawCode == state->consumedAltCode_)) {
                        // Single-output Alt cycling: a different leader (not
                        // the same Alt auto-repeat) signals the user wants to
                        // commit and continue typing.  Matches the immediate-
                        // commit behavior of non-Alt leaders with single
                        // output.
                        ic->inputPanel().reset();
                        ic->commitString(it->second[0]);
                        ic->updatePreedit();
                        state->recentlyCommitted_ = true;
                        state->inputKeyPressed_ = false;
                        // Arm auto-repeat suppression for the held input key.
                        // Without this, releasing Alt while the input key is
                        // still down would let the next repeat start a fresh
                        // gesture (üu-class duplicate).
                        state->committedKeyCode_ = state->waitingKeyCode_;
                        state->waitingKeyCode_ = 0;
                        state->resetCycling();
                        overlayHide();
                        state->altGestureSession_ = false;
                        state->consumedAltCode_ = 0;
                        // Emit the leader's character if printable so it
                        // appears as typed text instead of an Alt shortcut.
                        if (!keyChar.empty() &&
                            (keyChar.size() > 1 ||
                             static_cast<unsigned char>(keyChar[0]) >= ' ')) {
                            ic->commitString(keyChar);
                        }
                        keyEvent.filterAndAccept();
                        return;
                    }
                    // Single-output same-Alt repeat: preedit unchanged,
                    // consume.

                    if (isAlt)
                        state->consumedAltCode_ = rawCode;
                    keyEvent.filterAndAccept();
                    return;
                }
            }

            // CASE 2: First leader key press (start cycling)
            // PREEDIT: Update preedit to show first umlaut (don't commit yet!)
            if (state->waitingKey_ &&
                !state->isTimeoutExpired(getEffectiveDelay(state))) {
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
                        if (isAlt)
                            state->altGestureSession_ = true;

                        // Update preedit with first variant
                        updateClientPreedit(ic, it->second[0]);
                        // Overlay is for choosing among variants; suppress it
                        // when there's nothing to cycle (single-output Alt
                        // still needs the cycling state above for the deferred
                        // commit / re-press machinery).
                        if (it->second.size() > 1) {
                            overlayShow(ic, it->second, 0);
                            // The leader caught the window; hold the timing bar
                            // where it is while the user cycles.
                            freezeProgressOverlay();
                        } else if (overlayVisible_)
                            // Single-output Alt: no picker, but flash the cell
                            // to confirm the trigger. The commit is deferred
                            // via the alt-gesture machinery (release / re-press),
                            // so the flash fires on the Alt press itself,
                            // mirroring the non-Alt single-output branch below.
                            flashCommitOverlay(ic, state, it->second);
                        else
                            // No preview on screen (fast typing below min-hold):
                            // just tear down so we don't pop a blip out of
                            // nowhere.
                            hideTriggerOverlay(state);
                    } else {
                        // Single output with non-Alt leader - commit directly.
                        // If a trigger preview is already showing, flash the
                        // cell in the accent color to confirm the commit, then
                        // auto-hide. Otherwise (fast typing below min-hold,
                        // nothing on screen) just tear down so we don't pop a
                        // 150 ms blip out of nowhere.
                        if (overlayVisible_)
                            flashCommitOverlay(ic, state, it->second);
                        else
                            hideTriggerOverlay(state);
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
                    if (isAlt)
                        state->consumedAltCode_ = rawCode;
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
            if (!isNewKeyPress &&
                (state->waitingKey_ || state->cyclingInput_)) {
                keyEvent.filterAndAccept();
                return;
            }

            // Suppress auto-repeat of key that was just committed via
            // single-output. After single-output commit, the input key may
            // still be held, generating repeat events. Without this guard,
            // repeats start new unwanted gestures (e.g. 'u' + AltGr → "ü" then
            // repeat 'u' → "üu").
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
            if (*config_.overlay->progressBar && !overlayAtCaret()) {
                // Progress mode shows the overlay from t=0 with the timing bar
                // instead of the deferred min-hold trigger preview. The bar is
                // daemon-only; caret placement falls through to the trigger
                // preview, which only shows when ShowOnTrigger is on.
                startProgressOverlay(ic, state, keyChar);
            } else {
                // Preview the variants during the accent window (opt-in via
                // [Overlay]/ShowOnTrigger). No-op otherwise.
                scheduleTriggerOverlay(ic, state, keyChar);
            }

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
        // When Alt leader bypass was active, the key carries an Alt modifier
        // that would trigger application shortcuts (Alt+v → menu etc.).
        // Emit printable characters through commitString and consume the
        // event so the Alt modifier doesn't leak to the application.
        if (didAltBypass && !keyChar.empty() &&
            (keyChar.size() > 1 ||
             static_cast<unsigned char>(keyChar[0]) >= ' ')) {
            ic->commitString(keyChar);
            state->recentlyCommitted_ = true;
            keyEvent.filterAndAccept();
            return;
        }
        // Let key through
    }

    void activate(const InputMethodEntry &, InputContextEvent &event) override {
        // Ensure clean state when switching TO this input method.
        // Catches residual state after crashes or unexpected restarts.
        auto *state = event.inputContext()->propertyFor(&factory_);
        state->clearAllState();
        state->recentlyCommitted_ = false;
    }

    void deactivate(const InputMethodEntry &,
                    InputContextEvent &event) override {
        // Called on genuine focus changes (FocusOut / IC switch).
        // Clears all state so gestures don't leak across windows.
        auto *ic = event.inputContext();
        auto *state = ic->propertyFor(&factory_);

        // On IM switch (Ctrl+Space): commit pending preedit before clearing.
        // The server does NOT auto-commit preedit on IM switch, only on
        // FocusOut.
        if (event.type() == EventType::InputContextSwitchInputMethod) {
            commitPendingKey(ic, state);
            commitCyclingValue(ic, state);
        }

        state->clearAllState();
        state->recentlyCommitted_ = false;
        // Focus left this context: drop any visible overlay (cycling picker or
        // trigger preview) so it doesn't linger over another window.
        overlayHide();
    }

    void reset(const InputMethodEntry &, InputContextEvent &event) override {
        // Don't clear state if input key is still pressed!
        // Some apps (Chromium, Neovide) call reset() after every commit.
        auto *state = event.inputContext()->propertyFor(&factory_);
        if (state->inputKeyPressed_) {
            return; // Keep all state intact
        }

        // A running commit-flash must survive the post-commit reset that
        // Chromium and Neovide fire, otherwise the confirmation overlay would
        // vanish in the same frame as the commit (single-output commits set
        // inputKeyPressed_ = false, so the early return above doesn't catch
        // them). Pull the timer out so clearAllState()'s cancelOverlayHide()
        // becomes a no-op, then restore it and leave the overlay up. The
        // overlayVisible_ check distinguishes a live flash from a spent timer
        // (overlayHideEvent_ stays non-null after firing, like overlayShowEvent_).
        auto flash = std::move(state->overlayHideEvent_);
        state->clearAllState();
        if (flash && overlayVisible_) {
            state->overlayHideEvent_ = std::move(flash);
            return;
        }
        // Route through hideTriggerOverlay (not a bare overlayHide) so the
        // DBus Hide is suppressed when ShowOnTrigger is off. Apps like Chromium
        // and Neovide call reset() after every commit; cycling holds
        // inputKeyPressed_ and returned above, so only a trigger preview can
        // still be showing here.
        hideTriggerOverlay(state);
    }

private:
    // Map a profile's stored File ("mappings.txt" or "profiles/<slug>.txt") to
    // the loader path relative to the addon config dir. Empty File defaults to
    // the legacy Standard mappings.txt.
    static std::string profileRelPath(const std::string &file) {
        const std::string base =
            std::string(schnelle_umlaute::kConfigSubdir) + "/";
        // Guard against a hand-edited profiles.conf with a traversal/absolute
        // File=: fall back to the Standard mappings rather than read outside
        // the config dir. Single choke point for every engine path build.
        if (file.empty() || !schnelle_umlaute::isSafeProfileFile(file))
            return base + schnelle_umlaute::kMappingsFile;
        return base + file;
    }

    // Relative mappings path of the active profile. Falls back to the first
    // profile, then to the legacy mappings.txt, so a missing/unknown active
    // name never leaves the engine without mappings. An empty profile list is
    // the pre-profiles / fresh-install state and maps to mappings.txt, keeping
    // behavior unchanged until the editor seeds profiles.conf.
    std::string activeProfileFile() const {
        const auto &profs = *profiles_.profiles;
        if (profs.empty())
            return profileRelPath(schnelle_umlaute::kMappingsFile);
        const std::string &active = *profiles_.active;
        for (const auto &p : profs) {
            if (*p.name == active)
                return profileRelPath(*p.file);
        }
        return profileRelPath(*profs.front().file);
    }

    // Apply in-memory config: rebuild mappings, sanitize custom key, log.
    // Shared by setConfig (values already loaded) and reloadConfig (read from
    // disk).
    void applyConfig() {
        umlautMap_ = schnelle_umlaute::loadMappingsFromFile(activeProfileFile());

        // Sanitize custom leader key: trim whitespace, keep only first
        // UTF-8 character.  Cached for runtime use — the config file
        // stores the original value so the UI round-trips correctly.
        cachedCustomKey_ =
            *config_.leader->custom->customKeyEnabled
                ? sanitizeCustomKey(*config_.leader->custom->customKey)
                : "";
        cachedCustomKey2_ =
            *config_.leader->custom->customKey2Enabled
                ? sanitizeCustomKey(*config_.leader->custom->customKey2)
                : "";

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
                FCITX_WARN()
                    << "Schnelle: CustomKey and CustomKey2 are identical"
                    << " — dual split disabled, both trigger all mappings";
            } else if (handClassifier_.isLeftHand(cachedCustomKey_) ==
                       handClassifier_.isLeftHand(cachedCustomKey2_)) {
                FCITX_WARN()
                    << "Schnelle: CustomKey '" << cachedCustomKey_
                    << "' and CustomKey2 '" << cachedCustomKey2_
                    << "' are on the same keyboard half"
                    << " — dual split disabled, both trigger all mappings";
            }
        }

        std::string leaders;
        if (*config_.leader->space)
            leaders += "Space ";
        if (*config_.leader->left)
            leaders += "Left ";
        if (*config_.leader->right)
            leaders += "Right ";
        if (*config_.leader->up)
            leaders += "Up ";
        if (*config_.leader->down)
            leaders += "Down ";
        if (*config_.leader->alt)
            leaders += "Alt/AltGr ";
        if (!cachedCustomKey_.empty())
            leaders += "Custom1('" + cachedCustomKey_ + "') ";
        if (!cachedCustomKey2_.empty())
            leaders += "Custom2('" + cachedCustomKey2_ + "') ";
        if (leaders.empty())
            leaders = "None ";

        // App filter: push config values into the filter
        appFilter_.configure(*config_.appFilter->mode,
                             *config_.appFilter->blacklist,
                             *config_.appFilter->whitelist);

        FCITX_INFO() << "Schnelle: Config loaded - DelayLowercase=["
                     << *config_.delay->lowercaseMin << ","
                     << *config_.delay->lowercase << "]ms, DelayUppercase=["
                     << *config_.delay->uppercaseMin << ","
                     << *config_.delay->uppercase << "]ms, Leaders=" << leaders
                     << ", Profile=" << *profiles_.active
                     << ", Mappings=" << umlautMap_.size();

        // Daemon lifecycle follows the enable flag only, not the placement. On
        // enable it is DBus-activated once (a no-op where it can't run, e.g.
        // X11/GNOME); in caret mode it is never told to show, so it just sits
        // idle and invisible. Tying it to !overlayAtCaret() instead caused
        // start/quit churn on placement switches that could race the DBus
        // activation and leave the daemon down after switching back.
        overlayClient_.applyEnabledTransition(*config_.overlay->enabled);
    }

    void updateClientPreedit(InputContext *ic, const std::string &text) {
        Text preedit(text);
        preedit.setCursor(static_cast<int>(preedit.textLength()));
        ic->inputPanel().setClientPreedit(preedit);
        ic->updatePreedit();
    }

    // Deferred cycling commit for Alt-led gestures on KWin Wayland.
    // Auto-repeat sends release-press pairs; committing on release would
    // destroy cycling state. Instead, wait 5ms — if a re-press arrives,
    // it cancels this timer and cycling continues. If not (real release),
    // the timer fires and commits the cycling value.
    void scheduleDeferredCyclingCommit(InputContext *ic,
                                       SchnelleUmlauteState *state) {
        state->cancelTimeout();

        auto savedRef = ic->watch();
        auto *eventLoop = &instance_->eventLoop();
        uint64_t now = SchnelleUmlauteState::nowUsec();
        uint64_t target =
            now + kDeferredCommitDelayMs * kMicrosecondsPerMillisecond;

        state->timeoutEvent_ = eventLoop->addTimeEvent(
            CLOCK_MONOTONIC, target, 0,
            [state, savedRef, this](EventSourceTime *, uint64_t) {
                // Safety: see scheduleTimeout — single-threaded event loop
                // guarantees state outlives savedRef.get() != nullptr.
                auto *ctx = savedRef.get();
                if (!ctx)
                    return false;

                if (state->cyclingInput_) {
                    auto it = umlautMap_.find(*state->cyclingInput_);
                    if (it != umlautMap_.end() &&
                        state->cyclingIndex_ < it->second.size()) {
                        ctx->inputPanel().reset();
                        ctx->commitString(it->second[state->cyclingIndex_]);
                        ctx->updatePreedit();
                        state->recentlyCommitted_ = true;
                    }
                    state->resetCycling();
                    overlayHide();
                    state->waitingKeyCode_ = 0;
                }
                state->altGestureSession_ = false;
                state->consumedAltCode_ = 0;
                return false;
            });
    }

    void commitPendingKey(InputContext *ic, SchnelleUmlauteState *state) {
        if (!state->waitingKey_)
            return;
        hideTriggerOverlay(state);
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
        if (!state->cyclingInput_)
            return;
        const auto cyclingInput = *state->cyclingInput_;
        state->cancelTimeout(); // Cancel any deferred commit timer
        auto it = umlautMap_.find(cyclingInput);
        if (it != umlautMap_.end() &&
            state->cyclingIndex_ < it->second.size()) {
            ic->inputPanel().reset();
            ic->commitString(it->second[state->cyclingIndex_]);
            ic->updatePreedit();
            state->recentlyCommitted_ = true;
        }
        state->inputKeyPressed_ = false;
        state->resetCycling();
        overlayHide();
    }

    // Intentionally no whitespace trimming: leading/trailing spaces in outputs
    // are valid (e.g. mapping a key to " " so terminal commands skip history).
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
    static bool matchCustomKey(const std::string &keyChar,
                               const std::string &customKey) {
        if (keyChar == customKey)
            return true;
        if (keyChar.size() == 1 && customKey.size() == 1) {
            char a = keyChar[0], b = customKey[0];
            if (a >= 'A' && a <= 'Z')
                a += 32;
            if (b >= 'A' && b <= 'Z')
                b += 32;
            if (a >= 'a' && a <= 'z')
                return a == b;
        }
        return false;
    }

    // Resolve the base (unshifted, Level 0) character for a physical key
    // using the XKB keymap.  Returns empty string if unavailable.
    std::string getBaseChar(int rawCode) const {
        if (!xkbKeymap_)
            return "";
        auto code = static_cast<xkb_keycode_t>(rawCode);
        const xkb_keysym_t *syms;
        int n = xkb_keymap_key_get_syms_by_level(xkbKeymap_, code, 0, 0, &syms);
        if (n > 0) {
            uint32_t uc = xkb_keysym_to_utf32(syms[0]);
            if (uc > 0 && uc <= kMaxUnicodeCodepoint) {
                return utf8::UCS4ToUTF8(uc);
            }
        }
        return "";
    }

    // Match a keypress against a custom leader key.  First tries the
    // character directly (handles letters via case-folding).  If that
    // fails (Shift turned '/' into '?'), resolves the physical key's
    // base character via the XKB keymap and retries.
    bool matchCustomKeyOrBase(const std::string &keyChar,
                              const std::string &customKey, int rawCode) const {
        if (!keyChar.empty() && matchCustomKey(keyChar, customKey))
            return true;
        std::string base = getBaseChar(rawCode);
        return !base.empty() && matchCustomKey(base, customKey);
    }

    // Leader classification for dual custom leader support.
    // Built-in leaders (Space, Arrows, Alt) are unrestricted.
    // Custom1/Custom2 may be restricted by dual-split logic.
    enum class LeaderType { None, BuiltIn, Custom1, Custom2 };

    LeaderType classifyLeader(const Key &key, const std::string &keyChar,
                              int rawCode) const {
        KeySym sym = key.sym();

        // Alt/AltGr — built-in, unrestricted
        if (*config_.leader->alt && isAltLeaderSym(sym))
            return LeaderType::BuiltIn;

        // Custom Key 1 (sanitized at config load, case-insensitive for
        // letters). When Shift changes the character (e.g. Shift+/ → ?), fall
        // back to the XKB keymap to resolve the physical key's base character.
        if (!cachedCustomKey_.empty() &&
            matchCustomKeyOrBase(keyChar, cachedCustomKey_, rawCode))
            return LeaderType::Custom1;

        // Custom Key 2
        if (!cachedCustomKey2_.empty() &&
            matchCustomKeyOrBase(keyChar, cachedCustomKey2_, rawCode))
            return LeaderType::Custom2;

        // Built-in leader toggles
        if (*config_.leader->space && sym == FcitxKey_space)
            return LeaderType::BuiltIn;
        if (*config_.leader->left && sym == FcitxKey_Left)
            return LeaderType::BuiltIn;
        if (*config_.leader->right && sym == FcitxKey_Right)
            return LeaderType::BuiltIn;
        if (*config_.leader->up && sym == FcitxKey_Up)
            return LeaderType::BuiltIn;
        if (*config_.leader->down && sym == FcitxKey_Down)
            return LeaderType::BuiltIn;

        return LeaderType::None;
    }

    // Dual custom leader split: when BOTH custom keys are set and on
    // opposite hands, each only triggers inputs on the OTHER hand.
    // Single custom key or same-hand keys → no restriction.
    // Built-in leaders always unrestricted.
    // inputKeyCode: physical keycode of the input key (from waitingKeyCode_).
    // When available, uses the physical key position directly — this correctly
    // classifies shifted characters (e.g. ! = Shift+1 → left hand) that
    // charToKeycode_ cannot resolve (it only has level 0 / unshifted chars).
    bool isDualCustomAllowed(LeaderType leader, const std::string &inputKey,
                             int inputKeyCode = 0) const {
        if (leader == LeaderType::BuiltIn || leader == LeaderType::None)
            return true;

        // Dual mode only when BOTH custom keys are set
        if (cachedCustomKey_.empty() || cachedCustomKey2_.empty())
            return true;

        // Identical keys → no split
        if (cachedCustomKey_ == cachedCustomKey2_)
            return true;

        bool key1Left = handClassifier_.isLeftHand(cachedCustomKey_);
        bool key2Left = handClassifier_.isLeftHand(cachedCustomKey2_);

        // Both keys on same hand → no split possible, allow all
        if (key1Left == key2Left)
            return true;

        bool inputLeft = (inputKeyCode > 0)
                             ? HandClassifier::isLeftHandKeycode(inputKeyCode)
                             : handClassifier_.isLeftHand(inputKey);

        // Left-hand leader triggers RIGHT-hand inputs (and vice versa)
        if (leader == LeaderType::Custom1)
            return key1Left ? !inputLeft : inputLeft;
        else // Custom2
            return key2Left ? !inputLeft : inputLeft;
    }

    // ASCII-only uppercase check — sufficient because input keys are
    // physical keyboard keys which are always single ASCII bytes.
    int getEffectiveDelay(const SchnelleUmlauteState *state) const {
        if (!state->waitingKey_)
            return *config_.delay->lowercase;
        bool isUpper = state->waitingKey_->length() == 1 &&
                       (*state->waitingKey_)[0] >= 'A' &&
                       (*state->waitingKey_)[0] <= 'Z';
        return isUpper ? *config_.delay->uppercase : *config_.delay->lowercase;
    }

    // Lower bound (minimum hold) of the accent window for the waiting key.
    // Mirrors getEffectiveDelay's lowercase/uppercase split. The editor clamps
    // min < max, but a hand-edited config could set min >= max, which would
    // make the window unreachable and silently kill every accent. Guard against
    // that by ignoring a degenerate lower bound, so the window falls back to
    // [0, max] instead of going dead.
    int getEffectiveMinHold(const SchnelleUmlauteState *state) const {
        bool isUpper =
            state->waitingKey_ && state->waitingKey_->length() == 1 &&
            (*state->waitingKey_)[0] >= 'A' && (*state->waitingKey_)[0] <= 'Z';
        int minHold = isUpper ? *config_.delay->uppercaseMin
                              : *config_.delay->lowercaseMin;
        int maxDelay =
            isUpper ? *config_.delay->uppercase : *config_.delay->lowercase;
        if (minHold >= maxDelay)
            return 0;
        return minHold;
    }

    void scheduleTimeout(InputContext *ic, SchnelleUmlauteState *state) {
        if (!state->waitingKey_)
            return;
        auto savedKey = *state->waitingKey_;

        state->cancelTimeout();

        int effectiveDelay = getEffectiveDelay(state);
        auto *eventLoop = &instance_->eventLoop();

        uint64_t now_usec = SchnelleUmlauteState::nowUsec();
        uint64_t target_usec =
            now_usec +
            static_cast<uint64_t>(effectiveDelay) * kMicrosecondsPerMillisecond;

        auto savedRef = ic->watch();
        state->timeoutEvent_ = eventLoop->addTimeEvent(
            CLOCK_MONOTONIC, target_usec, 0,
            [this, state, savedKey, savedRef](EventSourceTime *, uint64_t) {
                // Safety: state is owned by IC (InputContextProperty). If IC
                // is destroyed, savedRef.get() returns nullptr and we bail
                // before touching state. No race: fcitx5's event loop is
                // single-threaded, so IC can't be destroyed between the
                // check and the access.
                auto *ctx = savedRef.get();
                if (!ctx)
                    return false;

                if (state->waitingKey_ && *state->waitingKey_ == savedKey) {
                    ctx->inputPanel().reset();
                    ctx->commitString(*state->waitingKey_);
                    ctx->updatePreedit();
                    state->recentlyCommitted_ = true;
                    state->waitingKey_.reset();
                    state->waitingKeyCode_ = 0;
                    state->inputKeyPressed_ = false;
                    // Window elapsed without a leader: clear the preview.
                    hideTriggerOverlay(state);
                }
                // Don't reset timeoutEvent_ here — destroying the EventSource
                // inside its own callback is a use-after-free risk. Returning
                // false disables the timer; the unique_ptr is cleaned up by
                // the next scheduleTimeout() or cancelTimeout() call.
                return false;
            });
    }

    // Shared by setConfig (values already loaded) and reloadConfig (read
    // from disk). Writes the normalized values back into config_ so that
    // the next safeSaveAsIni emits clean entries and applyConfig finds
    // consistent cached values.
    void normalizeCustomLeaders() {
        config_.leader.mutableValue()
            ->custom.mutableValue()
            ->customKey.setValue(
                sanitizeCustomKey(*config_.leader->custom->customKey));
        config_.leader.mutableValue()
            ->custom.mutableValue()
            ->customKey2.setValue(
                sanitizeCustomKey(*config_.leader->custom->customKey2));
    }

    // Sanitize custom leader key: trim whitespace, keep only first UTF-8
    // character, lowercase ASCII letters.  Only a single key is valid —
    // spaces would silently shadow the Space toggle, and multi-char strings
    // would never match a keypress.
    static std::string sanitizeCustomKey(const std::string &raw) {
        size_t start = raw.find_first_not_of(" \t\n\r");
        if (start == std::string::npos)
            return "";
        size_t end = raw.find_last_not_of(" \t\n\r");
        std::string trimmed = raw.substr(start, end - start + 1);
        if (!utf8::validate(trimmed))
            return "";
        size_t firstCharBytes = utf8::ncharByteLength(trimmed.begin(), 1);
        std::string result = trimmed.substr(0, firstCharBytes);
        if (result.size() == 1 && result[0] >= 'A' && result[0] <= 'Z')
            result[0] = static_cast<char>(result[0] - 'A' + 'a');
        return result;
    }

    Instance *instance_;
    SchnelleUmlauteConfig config_;
    // What getConfig() returns to fcitx5-config-qt / KDE KCM. Holds a
    // single ExternalOption so the configtool fast-paths past the dialog
    // and launches schnelle-umlaute-editor. The real settings live in
    // config_ and are written by the editor directly.
    ExternalEditorConfig externalConfig_;
    FactoryFor<SchnelleUmlauteState> factory_;

    // Mapping profiles, read from schnelle-umlaute/profiles.conf (separate
    // from config_; the editor owns that file). The active profile's mappings
    // file feeds umlautMap_ via activeProfileFile(). An empty list is the
    // pre-profiles / fresh-install state and falls back to mappings.txt.
    ProfilesConfig profiles_;

    // Mappings (shared across all InputContexts, read-only after config load)
    std::unordered_map<std::string, std::vector<std::string>> umlautMap_;
    // Sanitized custom leader keys (trimmed, single UTF-8 char each)
    std::string cachedCustomKey_;
    std::string cachedCustomKey2_;
    // XKB keymap for resolving the base (unshifted) character of a physical
    // key.  Lets Shift+symbol custom leaders work (e.g. Shift+/ → '?' is
    // resolved back to '/' so the leader still matches).
    struct xkb_context *xkbCtx_ = nullptr;
    struct xkb_keymap *xkbKeymap_ = nullptr;
    // Layout-independent hand classifier. Built from xkbKeymap_ after the
    // keymap is ready; used by isDualCustomAllowed() for the dual
    // custom-leader split feature.
    HandClassifier handClassifier_;
    // App filter (cached from config). When set to Blacklist/Whitelist,
    // processing is skipped for matching apps based on ic->program().
    AppFilter appFilter_;
    // DBus client for the standalone overlay daemon. Tracks its own
    // lifecycle — calling applyEnabledTransition() on every config reload
    // starts or stops the daemon in response to the Enabled flag.
    OverlayClient overlayClient_;
    // Best-effort mirror of the daemon's visibility so overlayHide() can skip
    // a redundant DBus Hide when nothing is showing. Plain typing with
    // ShowOnTrigger on otherwise emits two spurious Hides per mapped keystroke
    // (commitPendingKey + the app's follow-up reset). A daemon restart can
    // briefly desync this, but the next real show() corrects it.
    bool overlayVisible_ = false;
    // In TextCaret placement the overlay is fcitx5's own input-panel
    // candidate window, owned by the focused InputContext rather than the
    // daemon. Track that context so overlayHide() can clear its candidate
    // list. Watched (not a raw pointer) so a destroyed context resolves to
    // null instead of dangling.
    TrackableObjectReference<InputContext> caretOverlayIc_;

    // TextCaret placement renders through fcitx5's input-panel candidate
    // window (compositor-anchored at the caret) instead of the layer-shell
    // daemon. See OverlayPlacement in config.h.
    bool overlayAtCaret() const {
        return *config_.overlay->placement == OverlayPlacement::TextCaret;
    }

    void overlayShow(InputContext *ic, const std::vector<std::string> &variants,
                     int index) {
        if (!*config_.overlay->enabled)
            return;
        if (overlayAtCaret()) {
            showCaretOverlay(ic, variants, index);
            return;
        }
        // Combine the two enum halves into the single "<Row><Col>" string
        // the overlay daemon expects (e.g. "TopCol4", "CenterCol7"). In
        // MouseCursor placement, prefix the shared cursor marker so the daemon
        // anchors the overlay's lower-left corner at the pointer; the grid
        // string that follows is the fallback the daemon uses when the
        // compositor can't report the cursor position. The prefix is the same
        // constant the daemon parses with, so writer and reader can't drift.
        std::string position;
        if (*config_.overlay->placement == OverlayPlacement::MouseCursor)
            position = schnelle_umlaute::cursorPositionPrefix();
        position += OverlayRowToString(*config_.overlay->row);
        position += OverlayColumnToString(*config_.overlay->column);
        overlayClient_.show(variants, index, position);
        overlayVisible_ = true;
    }
    void overlayHide() {
        if (!*config_.overlay->enabled || !overlayVisible_)
            return;
        if (overlayAtCaret())
            hideCaretOverlay();
        else
            overlayClient_.hide();
        overlayVisible_ = false;
    }

    // Show the variants as a horizontal candidate list on the focused
    // context. fcitx5's active UI (classic-ui) wraps it in a
    // zwp_input_popup_surface_v2 the compositor anchors at the text caret; on
    // X11 it is placed via the client cursor rect. index < 0 means "no
    // highlight" (the trigger/progress preview before a leader is pressed).
    void showCaretOverlay(InputContext *ic,
                          const std::vector<std::string> &variants, int index) {
        auto list = std::make_unique<DisplayOnlyCandidateList>();
        list->setContent(variants);
        list->setLayoutHint(CandidateLayoutHint::Horizontal);
        list->setCursorIndex(index >= 0 ? index : -1);
        ic->inputPanel().setCandidateList(std::move(list));
        ic->updateUserInterface(UserInterfaceComponent::InputPanel);
        caretOverlayIc_ = ic->watch();
        overlayVisible_ = true;
    }
    void hideCaretOverlay() {
        if (auto *ic = caretOverlayIc_.get()) {
            ic->inputPanel().setCandidateList(nullptr);
            ic->updateUserInterface(UserInterfaceComponent::InputPanel);
        }
        caretOverlayIc_.unwatch();
    }

    // Progress bar ([Overlay]/ProgressBar). Unlike the trigger preview, it shows
    // the overlay from the accent key press (t=0) so the lead-in (min-hold) is
    // visible, then a window segment counts down across [min, max].
    // startProgressOverlay sends the durations and shows the variant preview;
    // freezeProgressOverlay holds the bar once a leader catches the window and
    // cycling begins.
    void startProgressOverlay(InputContext *ic, SchnelleUmlauteState *state,
                              const std::string &keyChar) {
        if (!*config_.overlay->enabled || !*config_.overlay->progressBar)
            return;
        auto it = umlautMap_.find(keyChar);
        if (it == umlautMap_.end() || it->second.empty())
            return;
        const int lead = getEffectiveMinHold(state);
        // getEffectiveMinHold caps lead below the delay (returns 0 when min >=
        // max), so window is >= 0 today. Clamp anyway so a future config/logic
        // change can never feed a negative duration to the QML bar animation.
        const int window = std::max(0, getEffectiveDelay(state) - lead);
        overlayClient_.setProgress(lead, window);
        overlayShow(ic, it->second, kPreviewNoHighlight);
    }
    void freezeProgressOverlay() {
        if (!*config_.overlay->enabled || !*config_.overlay->progressBar)
            return;
        if (overlayAtCaret())
            return;
        overlayClient_.freezeProgress();
    }

    // Index sent to the overlay for the trigger-window preview. No cell
    // matches it, so the picker shows the variants without a green highlight —
    // the active cell only lights up once a leader press starts cycling.
    static constexpr int kPreviewNoHighlight = -1;

    // Trigger-window preview ([Overlay]/ShowOnTrigger). Show the mapping's
    // variants as soon as the accent window opens — for EVERY mapped key,
    // including single-variant ones that never enter the cycling picker. The
    // preview waits for the minimum hold to elapse (shows immediately when
    // min == 0) and shows the variants with no cell highlighted. Once a leader
    // is pressed the cycling logic takes over: it keeps the overlay (now
    // highlighting the active variant) for multi-variant keys and hides it for
    // single-output keys, so cycling behaves exactly as before.
    void scheduleTriggerOverlay(InputContext *ic, SchnelleUmlauteState *state,
                                const std::string &keyChar) {
        if (!*config_.overlay->enabled || !*config_.overlay->showOnTrigger)
            return;
        auto it = umlautMap_.find(keyChar);
        if (it == umlautMap_.end() || it->second.empty())
            return;

        // A fresh preview supersedes a pending commit-flash hide from a
        // previous single-mapping commit, so that stale timer can't blank
        // this overlay mid-flash.
        state->cancelOverlayHide();

        int minHold = getEffectiveMinHold(state);
        if (minHold <= 0) {
            overlayShow(ic, it->second, kPreviewNoHighlight);
            return;
        }

        auto variants = it->second;
        auto savedRef = ic->watch();
        auto *eventLoop = &instance_->eventLoop();
        uint64_t target =
            SchnelleUmlauteState::nowUsec() +
            static_cast<uint64_t>(minHold) * kMicrosecondsPerMillisecond;

        // keyChar is captured by value so the copy outlives this call; the
        // deferred lambda compares it against the still-held waiting key.
        state->overlayShowEvent_ = eventLoop->addTimeEvent(
            CLOCK_MONOTONIC, target, 0,
            [this, state, keyChar, variants, savedRef](EventSourceTime *,
                                                       uint64_t) {
                // Safety mirrors scheduleTimeout: the single-threaded event
                // loop guarantees state outlives a non-null savedRef.get().
                auto *ctx = savedRef.get();
                if (!ctx)
                    return false;
                // Only preview if still holding the same key and a leader has
                // not started cycling in the meantime.
                if (state->waitingKey_ && *state->waitingKey_ == keyChar &&
                    !state->cyclingInput_)
                    overlayShow(ctx, variants, kPreviewNoHighlight);
                return false;
            });
    }

    // Tear down the trigger-window preview: cancel a pending show timer and
    // hide the overlay. The DBus hide is suppressed unless a trigger-time
    // feature is on (the preview or the progress bar, both of which show the
    // overlay before cycling), so plain commits don't emit a Hide on every
    // keystroke.
    void hideTriggerOverlay(SchnelleUmlauteState *state) {
        state->cancelOverlayShow();
        if (*config_.overlay->enabled && (*config_.overlay->showOnTrigger ||
                                          *config_.overlay->progressBar))
            overlayHide();
    }

    // How long the single cell stays highlighted after a single-mapping
    // commit before the overlay fades. Long enough for the 120 ms cell color
    // animation to land, short enough to feel like a confirmation blip.
    static constexpr int kCommitFlashMs = 150;

    // Confirm a single-mapping commit visually: highlight the (only) cell so
    // it lights up in the accent color, then hide after a short flash. Only
    // meaningful when a preview is already on screen — callers gate on
    // overlayVisible_ so fast typing below the min-hold doesn't pop a blip.
    void flashCommitOverlay(InputContext *ic, SchnelleUmlauteState *state,
                            const std::vector<std::string> &variants) {
        state->cancelOverlayShow();
        // The post-commit flash is a daemon-only confirmation: it survives the
        // commit because the daemon overlay is independent of the input panel.
        // In TextCaret placement the overlay IS the candidate list, which the
        // commit's inputPanel().reset() clears at once, so a flash would only
        // flicker. Just tear the preview down cleanly instead.
        if (overlayAtCaret()) {
            overlayHide();
            return;
        }
        overlayShow(ic, variants, 0);
        auto savedRef = ic->watch();
        auto *eventLoop = &instance_->eventLoop();
        uint64_t target = SchnelleUmlauteState::nowUsec() +
                          static_cast<uint64_t>(kCommitFlashMs) *
                              kMicrosecondsPerMillisecond;
        state->overlayHideEvent_ = eventLoop->addTimeEvent(
            CLOCK_MONOTONIC, target, 0,
            [this, savedRef](EventSourceTime *, uint64_t) {
                // Hide once the flash elapses. An orphaned timer can't blank a
                // newer overlay: every fresh gesture cancels it first via
                // scheduleTriggerOverlay's cancelOverlayHide(). The
                // overlayVisible_ guard in overlayHide() is just a backstop for
                // the case where an unrelated hide already ran (then it no-ops).
                if (savedRef.get())
                    overlayHide();
                return false;
            });
    }
};

class SchnelleUmlauteEngineFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        return new SchnelleUmlauteEngine(manager->instance());
    }
};

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::SchnelleUmlauteEngineFactory)
