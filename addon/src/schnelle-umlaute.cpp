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
#include "app_filter.h"
#include "config.h"
#include "hand_classifier.h"
#include "mappings_loader.h"
#include "overlay_protocol.h"
#include "usage_sort.h"
#include "profile_cycle.h"
#include "profile_paths.h"
#include <fcitx-utils/key.h>
#include "overlay/cursor_overlay_geometry.h"
#include "overlay_client.h"
#include "state.h"
#include "synthetic_autorepeat.h"

namespace fcitx {

constexpr uint32_t kMaxUnicodeCodepoint = 0x10FFFF;

// =============================================================================
// VelocityAccents-Style Implementation
// =============================================================================
// Key insight: Track whether input key is PHYSICALLY PRESSED
// - Cycling only works while input key is held down
// - Cycle as long as you want: every step postpones the backstop that ends a
//   gesture whose release never arrives, bounded only by its ceiling
//   (armCyclingWatchdog, issue #147)
// - When input key is released, cycling ends
// =============================================================================

class SchnelleUmlauteEngine final : public InputMethodEngineV2 {
public:
    SchnelleUmlauteEngine(Instance *instance)
        : instance_(instance),
          factory_([](InputContext &) { return new SchnelleUmlauteState; }) {
        instance_->inputContextManager().registerProperty(
            "schnelle-umlaute-state", &factory_);

        // Load the usage counters once, before reloadConfig() builds the first
        // runtime map (which frequency-sorts from them). Never re-read after:
        // the in-memory table is the source of truth for the rest of the
        // session, so a later reloadConfig() cannot discard unflushed counts.
        usageCounts_ = schnelle_umlaute::loadUsage();
        reloadConfig();
    }

    // Persist any pending usage counts on shutdown (addon unload), the last
    // flush after the periodic timer and focus-out flushes during the session.
    ~SchnelleUmlauteEngine() override { flushUsage(); }

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
            wipeAllGestureState();
            // If RawConfig contains mapping data, use it directly (the editor
            // pushing the active profile's mappings live). Otherwise reload
            // from file (normal configtool path). Either way the result still
            // goes through finish/buildRuntimeMap so the merge and the
            // frequency sort apply to the pushed base too.
            UmlautMap pushed;
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
                    pushed[*input] = std::move(outputs);
                }
            }
            umlautMap_ = pushed.empty() ? buildRuntimeMap()
                                        : finishRuntimeMap(std::move(pushed));
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

        // Deliver a still-pending deferred space before this key is processed,
        // so its text can never land behind this key's output if the key event
        // wins the race against the zero-delay timer. See scheduleSpaceCommit().
        flushPendingSpaceCommit(ic, state);

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

        // A press of the gesture's own input key that cannot be that key's
        // auto-repeat proves the release was swallowed (issue #147). The
        // platform freezes the frontend event time across a whole repeat burst,
        // so every repeat carries the timestamp of the press that started it,
        // and only a genuinely new press carries a new one. heldRawCodes_
        // cannot answer this: the lost release never erased the code, so the
        // fresh press reads as a repeat there.
        //
        // Cycling only. The waiting phase has the accent window as its own
        // upper bound, and inputKeyPressed_ keeps the Alt deferred commit's own
        // 5 ms window out of this. A frontend that reports no event time at all
        // leaves waitingKeyTime_ at 0 and simply never triggers here, which
        // costs the detection but never fires it wrongly.
        //
        // The gesture ends the way its release would have ended it, by
        // committing the variant on screen. The press itself is deliberately
        // not consumed: it falls through and starts its own gesture, so the
        // keystroke the user just made is not swallowed.
        if (isPress && state->cyclingInput_ && state->inputKeyPressed_ &&
            state->waitingKeyCode_ != 0 && rawCode == state->waitingKeyCode_ &&
            state->waitingKeyTime_ != 0 &&
            keyEvent.time() != state->waitingKeyTime_) {
            commitCyclingValue(ic, state);
        }
        // Get character from key
        uint32_t unicode = Key::keySymToUnicode(key.sym());
        std::string keyChar;
        if (unicode > 0 && unicode <= kMaxUnicodeCodepoint) {
            keyChar = utf8::UCS4ToUTF8(unicode);
        }

        if (isPress)
            learnBaseChar(keyEvent.rawKey(), rawCode);

        // Pure modifier key presses (Shift, Ctrl, Alt, Super, etc.)
        // pass through without affecting gesture state.
        // Only Modifier+Key combinations trigger the modifier check below.
        if (isPress && key.sym() >= FcitxKey_Shift_L &&
            key.sym() <= FcitxKey_Hyper_R) {
            // Allow Alt_L/Alt_R through as leader when configured and gesture
            // active. ISO_Level3_Shift (AltGr on EU layouts, 0xfe03) is outside
            // this range and passes through naturally.
            if (((*config_.leader->alt && key.sym() == FcitxKey_Alt_L) ||
                 (*config_.leader->altGr && key.sym() == FcitxKey_Alt_R)) &&
                (state->waitingKey_ || state->cyclingInput_)) {
                // Fall through to leader key handling
            } else {
                // A fresh press of the very key whose consumed-leader release
                // is still awaited proves that release was lost (a compositor
                // grab swallowed it, issue #147 class): disarm, so the release
                // eater cannot consume this press's REAL release and leave the
                // application with a stuck modifier. Only outside an active
                // gesture and Alt session; during KWin Wayland auto-repeat
                // gaps the session is still ongoing and the arming must
                // survive (see the release-side comment). Any other pure
                // modifier press keeps the arming: its own release is not the
                // awaited one.
                if (state->consumedAltCode_ != 0 &&
                    rawCode == state->consumedAltCode_ &&
                    state->altSessionOver()) {
                    state->consumedAltCode_ = 0;
                }
                return;
            }
        }

        // Profile-switch shortcuts (user-configurable, e.g. Ctrl+Alt+1 or
        // Ctrl+Alt+J/K). Matched on a fresh press only (no auto-repeat) and
        // before gesture handling so the combo is intercepted rather than
        // leaking to the app. switchToProfileName/cycleProfile clear gesture
        // state, swap the active profile's mappings, and flash the name.
        if (isPress && isNewKeyPress) {
            // Normalize the event key to match the normalized stored combos
            // (case-insensitive letters, see parseShortcut).
            Key nkey = key.normalize();
            if (cycleNextKey_.isValid() && nkey.check(cycleNextKey_)) {
                cycleProfile(ic, +1);
                keyEvent.filterAndAccept();
                return;
            }
            if (cyclePrevKey_.isValid() && nkey.check(cyclePrevKey_)) {
                cycleProfile(ic, -1);
                keyEvent.filterAndAccept();
                return;
            }
            for (const auto &s : profileSelectShortcuts_) {
                if (nkey.check(s.key)) {
                    switchToProfileName(ic, s.name);
                    keyEvent.filterAndAccept();
                    return;
                }
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
                // While a gesture or Alt session is active the arming must
                // survive this release: on KWin Wayland, Alt auto-repeat
                // sends release-press pairs, and clearing here would leave a
                // gap where input key events leak through hasModifiers. Once
                // the session is over, this release is the final, symmetric
                // counterpart of the consumed leader press: consume it and
                // disarm (one-shot), so a stale arming can never eat a later,
                // unrelated Alt release, which would leave the application
                // with a stuck modifier (issue #147 class).
                if (state->altSessionOver()) {
                    state->consumedAltCode_ = 0;
                }
                keyEvent.filterAndAccept();
                return;
            }

            // Consume release of key that was committed via single-output.
            // The press was filterAndAccepted, so the release is an orphan.
            if (state->committed_.code != 0 &&
                rawCode == state->committed_.code) {
                // On KWin/Wayland auto-repeat arrives as release-press pairs with
                // a frozen event time. A synthetic release carries the committed
                // key's press time, and must NOT drop the arming — otherwise the
                // paired re-press restarts a gesture and one raw char leaks
                // (issue #92 hole 2). Keep the arming and re-insert the raw code
                // that the top-of-handler erase removed, so the repeat guard
                // below (keyed on heldRawCodes_) swallows the re-press. Uses the
                // committed gesture's own press time/start (committed_.time /
                // .startUsec), mirroring the waiting-release branch's #73 check.
                // The window-timeout arming leaves committed_.time == 0, so this
                // never matches there and that path clears per window as before.
                if (state->isSyntheticCommittedRelease(keyEvent.time())) {
                    state->heldRawCodes_.insert(rawCode);
                    keyEvent.filterAndAccept();
                    return;
                }
                // Genuine release (advanced or absent event time): clear it.
                state->clearCommittedKey();
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

                // A synthetic auto-repeat release of the held key must not close
                // the variant picker. Same frozen-timestamp check as the
                // pre-leader path (issue #73): waitingKeyTime_ is still the
                // original press time during cycling, and startTimeUsec_ still
                // marks the gesture's press for the elapsed guard. Suppress it
                // and keep cycling; the paired synthetic re-press is ignored by
                // the repeat guard further down (cyclingInput_ == keyChar).
                if (state->isSyntheticWaitingRelease(keyEvent.time())) {
                    state->sawSyntheticRelease_ = true;
                    keyEvent.filterAndAccept();
                    return;
                }

                // Non-Alt leader: commit immediately
                auto it = umlautMap_.find(*state->cyclingInput_);
                if (it != umlautMap_.end() &&
                    state->cyclingIndex_ < it->second.size()) {
                    ic->inputPanel().reset();
                    ic->commitString(it->second[state->cyclingIndex_]);
                    recordUsage(*state->cyclingInput_,
                                it->second[state->cyclingIndex_]);
                    ic->updatePreedit();
                    state->recentlyCommitted_ = true;
                }

                state->resetWaitingGesture();
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
                if (state->isSyntheticWaitingRelease(keyEvent.time())) {
                    // Held-key auto-repeat (issue #73): KWin freezes the
                    // frontend event time across the whole repeat burst, so
                    // this release carries the starting press's timestamp and
                    // is synthetic, not a real release. Suppress it and keep
                    // the gesture waiting so the held char is not committed
                    // prematurely; the paired synthetic re-press is swallowed
                    // by the accent-key repeat guard below (waitingKey_ ==
                    // keyChar). The window timer keeps running untouched, so a
                    // leader can still convert and the genuine final release
                    // (advanced timestamp) falls through to the commit below.
                    // Record that this gesture is on a synthetic-release
                    // platform, so the window-timeout commit knows a trailing
                    // synthetic release will follow and needs consuming. On
                    // press-only auto-repeat (classic X11) this is never set, so
                    // that path stays byte-for-byte historic.
                    state->sawSyntheticRelease_ = true;
                    keyEvent.filterAndAccept();
                    return;
                }
                // Genuine release (advanced or absent event time): commit
                // immediately, the historic behavior.
                commitPendingKey(ic, state);
                keyEvent.filterAndAccept();
                return;
            }
            return;
        }

        // =========================================
        // ORDERING GUARD: Ensure correct character order after timeout
        // =========================================
        // Window elapsed but a key arrived before the timeout timer fired:
        // commit the pending char now (this also clears the trigger preview,
        // mirroring the timeout callback's teardown). A Space is committed
        // separately in its own event-loop turn so per-event apps receive two
        // single-character inserts instead of one "a " (issue #90); see
        // scheduleSpaceCommit(). Modifier combinations (Ctrl+Space) are
        // excluded so shortcuts are not swallowed; any other key falls
        // through and continues as a normal key.
        if (state->waitingKey_ &&
            state->isTimeoutExpired(getEffectiveDelay(state))) {
            commitPendingKey(ic, state);

            if (key.sym() == FcitxKey_space && !hasModifiers(key)) {
                scheduleSpaceCommit(ic, state);
                keyEvent.filterAndAccept();
                return;
            }
        }

        // =========================================
        // HANDLE MODIFIER COMBINATIONS (Ctrl+C, Alt+F4, etc.)
        // Unlike the pure modifier early-return above (Shift/Ctrl alone),
        // this handles a modifier HELD + another key pressed.
        // =========================================
        bool didAltBypass = false;
        if (hasModifiers(key)) {
            // When Alt is the leader key and a gesture or Alt-led session is
            // live, ignore Alt-only modifier state: input key repeats with Alt
            // held must not commit the gesture and leak through. Gated on
            // altSessionOver(), the same predicate as the release eater.
            // consumedAltCode_ deliberately does NOT widen this: after a
            // shortcut abort it stays armed for the still-owed leader release,
            // and honoring it here would keep the bypass alive with no gesture
            // left, turning the next Alt+key application shortcut into
            // committed text while that Alt is still held (issue #147 class).
            bool altLeaderBypass =
                (*config_.leader->alt || *config_.leader->altGr) &&
                !state->altSessionOver();
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
                // The shortcut also ends any Alt-led session: a stale
                // altGestureSession_ would keep the Alt-leader bypass armed
                // forever, turning later Alt+key application shortcuts into
                // committed text (issue #147 class). consumedAltCode_ stays
                // armed on purpose, but no longer feeds the bypass (see
                // altSessionOver() above): it only marks the release the
                // consumed leader press still owes, which the one-shot
                // release eater consumes. Alt+key keeps working as a normal
                // application shortcut in the meantime, while that Alt is
                // still physically held. The awaited release, or a fresh Alt
                // press after a lost one, disarms it.
                state->altGestureSession_ = false;
                return; // Let the shortcut through
            }
            // Alt-only during gesture: resolve the physical key's base
            // character.  Some layouts hand us a different keysym when Alt is
            // held (AltGr+q → '@', number keys → symbols), which would stop the
            // accent key handler from finding the mapping.  The base character
            // comes from what this key produced unmodified earlier in the
            // session (learnBaseChar), i.e. the real layout, not a compiled guess.
            // Unknown key → keep the keysym's character, as before.
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
            // The backstop needs no arming here: this key is the gesture's own,
            // so the liveness re-arm at the top of the handler already ran for
            // this very event.
            keyEvent.filterAndAccept();
            return;
        }

        // =========================================
        // HANDLE LEADER KEY (Space/Arrows/Alt/Custom)
        // =========================================
        auto leaderType = classifyLeader(key, rawCode);

        // Dual custom leader split: downgrade to None if this leader is
        // not allowed for the currently active input key's keyboard half.
        // The gesture must be live (waiting or cycling) for a half to exist;
        // waitingKeyCode_ carries the input key's physical position through
        // both phases.
        if (leaderType != LeaderType::None &&
            (state->cyclingInput_ || state->waitingKey_) &&
            !isDualCustomAllowed(leaderType, state->waitingKeyCode_)) {
            leaderType = LeaderType::None;
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
            // the plain pending char now, then let the leader act as a normal
            // key. With min == 0 this never fires, so the historic behavior
            // is unchanged.
            if (!state->cyclingInput_ && state->waitingKey_ &&
                state->isBeforeMinHold(getEffectiveMinHold(state))) {
                // Arm auto-repeat suppression for the still-held input key.
                // Without this, the next auto-repeat of the held key would
                // start a fresh gesture and duplicate the character (the
                // "üu"-class bug guarded at the committed_ check). Suppresses on
                // X11 and KWin/Wayland alike: committed_.time is the frozen press
                // time, so a synthetic release keeps the arming (issue #92 hole
                // 2). Capture waitingKeyTime_/startTimeUsec_ now, before
                // commitPendingKey() clears the waiting gesture.
                state->armCommittedFromWaiting();
                commitPendingKey(ic, state);
                if (key.sym() == FcitxKey_space && !hasModifiers(key)) {
                    // The space is committed separately in its own event-loop
                    // turn so per-event apps receive two single-character
                    // inserts instead of one "a " (issue #90). See
                    // scheduleSpaceCommit() for the transport rationale.
                    scheduleSpaceCommit(ic, state);
                    keyEvent.filterAndAccept();
                    return;
                }
                // Non-Space leader (arrow): the plain char is committed, the
                // leader passes through as a normal key. The committed char
                // and the raw leader travel on separate channels, so in
                // theory they could reorder in terminals like WezTerm (the
                // #6 pattern), but arrows cannot be delivered per
                // commitString, and arrow leaders combined with a minimum
                // hold are rare.
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
                        // Cycle to the next variant. Forward leaders step +1,
                        // the reverse leader steps -1; +n keeps the modulo
                        // positive for the -1 case.
                        const int n = static_cast<int>(it->second.size());
                        const int next =
                            (static_cast<int>(state->cyclingIndex_) +
                             leaderStep(key, rawCode) + n) %
                            n;
                        state->cyclingIndex_ = static_cast<size_t>(next);
                        updateClientPreedit(ic, state,
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
                        recordUsage(*state->cyclingInput_, it->second[0]);
                        ic->updatePreedit();
                        state->recentlyCommitted_ = true;
                        // Arm auto-repeat suppression for the held input key.
                        // Without this, releasing Alt while the input key is
                        // still down would let the next repeat start a fresh
                        // gesture (üu-class duplicate). Suppresses on X11 and
                        // KWin/Wayland alike (issue #92 hole 2); the frozen press
                        // time in committed_ keeps the arming across a synthetic
                        // release burst.
                        state->armCommittedFromWaiting();
                        state->resetWaitingGesture();
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
                        // A reverse leader that starts a fresh multi-variant
                        // session lands on the LAST variant, so a preset whose
                        // rarer variants sit at the end is reached without
                        // stepping past the common ones first. Any reverse
                        // leader (arrow, Alt / AltGr or custom) triggers this
                        // via leaderStep; a forward start, and any single-output
                        // path (nothing to reverse), stays at 0.
                        const size_t startIdx =
                            (it->second.size() > 1 &&
                             leaderStep(key, rawCode) < 0)
                                ? it->second.size() - 1
                                : 0;
                        state->cyclingInput_ = *state->waitingKey_;
                        state->cyclingIndex_ = startIdx;
                        if (isAlt)
                            state->altGestureSession_ = true;

                        // Update preedit with the starting variant
                        updateClientPreedit(ic, state, it->second[startIdx]);
                        // Overlay is for choosing among variants; suppress it
                        // when there's nothing to cycle (single-output Alt
                        // still needs the cycling state above for the deferred
                        // commit / re-press machinery).
                        if (it->second.size() > 1) {
                            overlayShow(ic, it->second,
                                        static_cast<int>(startIdx));
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

                        // Cycling owns the gesture now: only waitingKey_ ends;
                        // waitingKeyCode_/waitingKeyTime_ stay valid for the
                        // cycling release checks.
                        state->waitingKey_.reset();
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
                        recordUsage(*state->waitingKey_, it->second[0]);
                        // Arm auto-repeat suppression for the still-held key.
                        // Suppresses on X11 and KWin/Wayland alike (issue #92
                        // hole 2); the frozen press time in committed_ keeps the
                        // arming across a synthetic release burst. Capture before
                        // resetWaitingGesture() clears the waiting gesture.
                        state->armCommittedFromWaiting();
                        state->resetWaitingGesture();
                        state->recentlyCommitted_ = true;
                    }

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
            if (!isNewKeyPress && state->committed_.code != 0 &&
                rawCode == state->committed_.code) {
                keyEvent.filterAndAccept();
                return;
            }

            // New accent key - commit any pending state first
            commitPendingKey(ic, state);
            commitCyclingValue(ic, state);

            // Show character in PREEDIT (not committed yet - can be changed!)
            state->waitingKey_ = keyChar;
            state->waitingKeyCode_ = keyEvent.rawKey().code();
            // Remember the frontend event time of this press so a later release
            // carrying the same (frozen) timestamp can be recognised as a
            // synthetic auto-repeat and suppressed. See isSyntheticAutoRepeatRelease().
            state->waitingKeyTime_ = keyEvent.time();
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
            updateClientPreedit(ic, state, keyChar);

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
        auto *ic = event.inputContext();
        auto *state = ic->propertyFor(&factory_);
        // A deferred space already consumed from the user must be delivered
        // before clearAllState() cancels it, mirroring deactivate()/reset()/
        // wipeAllGestureState() (issue #90). Narrow: an activate() landing on an
        // IC that still holds a pending space without a flushing deactivate.
        flushPendingSpaceCommit(ic, state);
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

        // A deferred space was already consumed from the user; deliver it
        // instead of letting clearAllState() cancel it. Reachable only in the
        // sub-millisecond window where a FocusOut queued right behind the
        // Space press dispatches before the zero-delay timer.
        flushPendingSpaceCommit(ic, state);

        state->clearAllState();
        state->recentlyCommitted_ = false;
        // Focus left this context: drop any visible overlay (cycling picker or
        // trigger preview) so it doesn't linger over another window.
        overlayHide();
        // Focus-out is the main batched flush point for the usage counters, so
        // switching windows persists what was typed without a write per commit.
        flushUsage();
    }

    void reset(const InputMethodEntry &, InputContextEvent &event) override {
        // Don't clear state if input key is still pressed!
        // Some apps (Chromium, Neovide) call reset() after every commit.
        auto *ic = event.inputContext();
        auto *state = ic->propertyFor(&factory_);

        if (state->inputKeyPressed_) {
            return; // Keep all state intact
        }

        // A deferred trailing space was already consumed from the user; deliver
        // it before clearAllState() cancels it, mirroring deactivate(). The char
        // committed synchronously and cleared inputKeyPressed_, so the early
        // return above does not cover it; without this flush a reset() landing
        // in the sub-millisecond window before the zero-delay space timer
        // (Chromium and Neovide fire reset() after every commit) drops the
        // trailing space (issue #90).
        flushPendingSpaceCommit(ic, state);

        // A running commit-flash must survive the post-commit reset that
        // Chromium and Neovide fire, otherwise the confirmation overlay would
        // vanish in the same frame as the commit (single-output commits set
        // inputKeyPressed_ = false, so the early return above doesn't catch
        // them). Pull the timer out so clearAllState()'s cancelOverlayHide()
        // becomes a no-op, then restore it and leave the overlay up. The
        // overlayVisible_ check distinguishes a live flash from a spent timer
        // (overlayHideEvent_ stays non-null after firing, like overlayShowEvent_).
        // A still-held key whose char was already committed via single-output
        // keeps committed_ armed so its auto-repeat is consumed instead of
        // starting a fresh gesture (the "üu"-class guard). clearAllState() drops
        // committed_ and its heldRawCodes_ entry, so the app-reset() Chromium and
        // Neovide fire after every commit lets the next auto-repeat re-enter as a
        // fresh press (isNewKeyPress == true, so the repeat guard below the arming
        // sites no longer matches) and start a duplicate gesture (issue #92).
        // Preserve the whole committed_ bundle across the wipe; the focus-change
        // path (deactivate/activate) keeps clearing everything. Self-guarding:
        // code == 0 means nothing was armed (a release already cleared it via the
        // committed-key release branch).
        const auto heldCommitted = state->committed_;

        auto flash = std::move(state->overlayHideEvent_);
        state->clearAllState();
        if (heldCommitted.code != 0) {
            state->committed_ = heldCommitted;
            state->heldRawCodes_.insert(heldCommitted.code);
        }
        if (flash && overlayVisible_) {
            state->overlayHideEvent_ = std::move(flash);
            return;
        }
        // A profile-name flash lives on the engine-level profileFlashHideEvent_
        // (kept off the per-IC timer so a later gesture's cancelOverlayHide
        // can't kill it), so the commit-flash restore above never sees it. Leave
        // it up for the same reason and let its own timer hide it after the
        // readable delay. clearAllState() is per-IC and leaves that engine timer
        // running; a gesture overlay resets it (overlayShow), so a non-null
        // timer with a visible overlay uniquely means a live flash, not a spent
        // one (the flash callback clears overlayVisible_ when it fires).
        if (profileFlashHideEvent_ && overlayVisible_)
            return;
        // Route through hideTriggerOverlay (not a bare overlayHide) so the
        // DBus Hide is suppressed when ShowOnTrigger is off. Apps like Chromium
        // and Neovide call reset() after every commit; cycling holds
        // inputKeyPressed_ and returned above, so only a trigger preview can
        // still be showing here.
        hideTriggerOverlay(state);
    }

private:
    // The engine builds and rewrites this runtime table type in several places
    // (merge compose, frequency sort, live pushes); alias the loader's type so
    // those member signatures stay readable in this fcitx namespace.
    using UmlautMap = schnelle_umlaute::UmlautMap;

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
    // The active profile's bare File field ("mappings.txt" or
    // "profiles/<slug>.txt"), before the config-dir prefix. This is the form
    // the merge manifest stores its base/source refs in, so the merge check
    // compares against it directly.
    std::string activeBareFile() const {
        const auto &profs = *profiles_.profiles;
        if (profs.empty())
            return schnelle_umlaute::kMappingsFile;
        const std::string &active = *profiles_.active;
        for (const auto &p : profs) {
            if (*p.name == active)
                return *p.file;
        }
        return *profs.front().file;
    }

    std::string activeProfileFile() const {
        return profileRelPath(activeBareFile());
    }

    // The runtime mapping table for the active profile: its own mappings,
    // composed with the merge (only when the active profile is the merge base),
    // then frequency-sorted (only when the toggle is on). The single place the
    // engine turns a profile into what keyEvent cycles through.
    UmlautMap buildRuntimeMap() {
        return finishRuntimeMap(
            schnelle_umlaute::loadMappingsFromFile(activeProfileFile()));
    }

    // Apply the merge and the frequency sort to an already-loaded base map.
    // Shared by buildRuntimeMap (loads from file) and setSubConfig (uses the
    // mappings the editor just pushed for the active profile).
    UmlautMap finishRuntimeMap(UmlautMap base) {
        UmlautMap map = applyMergeIfBaseActive(std::move(base));
        storedMap_ = map; // stored order, before the frequency sort
        applyFrequencySort(map);
        return map;
    }

    // Compose the base map with the appended source profiles, but ONLY when the
    // active profile is the manifest's chosen base. Any other active profile
    // (or no manifest) is returned untouched, so the merge never "wanders" onto
    // a profile that is not its base. Duplicates are kept (projectValues), so
    // the cycle matches the composed editor view exactly; a repeated value is a
    // dead slot the editor flags, never silently removed here.
    UmlautMap applyMergeIfBaseActive(UmlautMap active) {
        const schnelle_umlaute::MergeManifest m =
            schnelle_umlaute::loadMergeManifest();
        if (m.base.empty() || m.base != activeBareFile())
            return active;
        // Load each appended source once; keep the maps alive for compose().
        std::vector<UmlautMap> extra;
        std::vector<std::string> extraRefs;
        extra.reserve(m.sources.size());
        extraRefs.reserve(m.sources.size());
        for (const auto &src : m.sources) {
            if (src == m.base || !schnelle_umlaute::isSafeProfileFile(src))
                continue; // base is source 0; unsafe/dangling refs are dropped
            extra.push_back(
                schnelle_umlaute::loadMappingsFromFile(profileRelPath(src)));
            extraRefs.push_back(src);
        }
        std::vector<schnelle_umlaute::ComposeSource> sources;
        sources.reserve(extra.size() + 1);
        sources.push_back({m.base, &active});
        for (size_t i = 0; i < extra.size(); ++i)
            sources.push_back({extraRefs[i], &extra[i]});
        return schnelle_umlaute::projectValues(
            schnelle_umlaute::compose(sources, m.order));
    }

    // Reorder each base's variants by usage (most-used first) when the toggle
    // is on. Non-destructive: the stored order is the input, the sort only
    // rearranges the in-memory runtime copy, and with no counts for a base the
    // stored order is kept (sortVariantsByUsage is a stable no-op on zero
    // counts). Uses the one shared comparator so the editor preview matches.
    void applyFrequencySort(UmlautMap &map) {
        if (!*config_.behavior->sortByFrequency)
            return;
        for (auto &kv : map) {
            auto it = usageCounts_.find(kv.first);
            if (it == usageCounts_.end())
                continue;
            kv.second =
                schnelle_umlaute::sortVariantsByUsage(kv.second, it->second);
        }
    }

    // Count one committed variant. Cheap in-memory increment; the table is
    // flushed to disk in batches (focus-out, periodic timer, shutdown), never
    // per keystroke, so a fast typist never triggers a write per commit.
    void recordUsage(const std::string &base, const std::string &variant) {
        if (base.empty() || variant.empty())
            return;
        // Count only while the sort is on. Off means no new counting, no timer
        // and no writes; the accumulated counts stay in memory and in usage.conf
        // (loaded once at startup), so turning the sort off then on resumes from
        // the same counts instead of losing them, and a reboot preserves them.
        if (!*config_.behavior->sortByFrequency)
            return;
        ++usageCounts_[base][variant];
        usageDirty_ = true;
        // Re-sort this key's cycle live from its stored order, so the next
        // trigger reflects the new count without waiting for a map rebuild. The
        // committing gesture has just ended, so its own cycle is not disturbed.
        // Re-sorting from storedMap_ (not the already-sorted umlautMap_) keeps
        // the tie-break on stored order, so runtime and editor preview agree.
        auto sit = storedMap_.find(base);
        auto it = umlautMap_.find(base);
        if (sit != storedMap_.end() && it != umlautMap_.end())
            it->second = schnelle_umlaute::sortVariantsByUsage(
                sit->second, usageCounts_[base]);
    }

    // Persist the usage table if it changed since the last write.
    void flushUsage() {
        if (!usageDirty_)
            return;
        if (schnelle_umlaute::saveUsage(usageCounts_))
            usageDirty_ = false;
    }

    // Re-arming timer that flushes the usage table periodically, so a long
    // session in a single window (which never fires deactivate) still persists
    // its counts. setTime + setOneShot re-arms the same source; recreating it
    // inside its own callback would free the running source.
    static constexpr int kUsageFlushIntervalMs = 60'000;
    void schedulePeriodicUsageFlush() {
        const uint64_t interval = static_cast<uint64_t>(kUsageFlushIntervalMs) *
                                  kMicrosecondsPerMillisecond;
        usageFlushEvent_ = instance_->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, SchnelleUmlauteState::nowUsec() + interval, 0,
            [this, interval](EventSourceTime *source, uint64_t) {
                flushUsage();
                source->setTime(SchnelleUmlauteState::nowUsec() + interval);
                source->setOneShot();
                return true;
            });
    }

    // Start or stop usage tracking to match the SortByFrequency toggle, called
    // from applyConfig on every config (re)load. On: arm the periodic flush
    // timer (recordUsage counts). Off: flush what accumulated, then drop the
    // timer and stop counting. The in-memory counts and usage.conf are kept
    // either way (usage.conf is loaded once at startup and never deleted here),
    // so toggling off is a pause: turning it back on resumes from the same
    // counts, and a reboot preserves them. A user who never enables the sort
    // gets no timer, no writes and no usage.conf at all.
    void applyUsageTracking() {
        if (*config_.behavior->sortByFrequency) {
            if (!usageFlushEvent_)
                schedulePeriodicUsageFlush();
        } else {
            flushUsage();
            usageFlushEvent_.reset();
        }
    }

    // Parse a combo string to a Key, or an invalid Key if it is empty or
    // carries no real (non-Shift) modifier. The modifier requirement stops a
    // bare key like "1" from matching and swallowing every plain press of it.
    static Key parseShortcut(const std::string &combo) {
        if (combo.empty())
            return Key();
        Key k(combo);
        if (!k.isValid() || !hasModifiers(k))
            return Key();
        // Normalize so matching is case-insensitive for letters: a
        // "Control+Alt+J" binding and a Ctrl+Alt+j press normalize to the same
        // sym/states (Key::check does no case folding). The event key is
        // normalized too at the match site.
        return k.normalize();
    }

    // Parse the configured combo strings into fcitx Keys once per config load,
    // so keyEvent only does Key::check (no per-keystroke string parsing).
    void rebuildProfileShortcuts() {
        profileSelectShortcuts_.clear();
        for (const auto &p : *profiles_.profiles) {
            Key k = parseShortcut(*p.selectKey);
            if (k.isValid())
                profileSelectShortcuts_.push_back({k, *p.name});
        }
        cycleNextKey_ = parseShortcut(*profiles_.cycleNext);
        cyclePrevKey_ = parseShortcut(*profiles_.cyclePrev);
    }

    // Cancel any active gesture on every IC before swapping umlautMap_, so no
    // cycling state references a now-removed entry. Mirrors setSubConfig.
    void wipeAllGestureState() {
        instance_->inputContextManager().foreach([this](InputContext *c) {
            auto *s = c->propertyFor(&factory_);
            // Deliver a consumed deferred space before wiping, same guard as
            // reset()/deactivate(): a config or profile reload can land between
            // the Space press and the zero-delay timer. Only the focused IC can
            // hold a pending space, so this is a no-op for every other context.
            flushPendingSpaceCommit(c, s);
            s->clearAllState();
            s->recentlyCommitted_ = false;
            c->inputPanel().reset();
            c->updatePreedit();
            return true;
        });
    }

    // Switch the active profile by name: wipe gesture state, load its mappings,
    // persist the choice, flash the name. No-op if already active or unknown.
    void switchToProfileName(InputContext *ic, const std::string &name) {
        if (name.empty() || name == *profiles_.active)
            return;
        bool known = false;
        for (const auto &p : *profiles_.profiles) {
            if (*p.name == name) {
                known = true;
                break;
            }
        }
        if (!known)
            return;
        auto *st = ic->propertyFor(&factory_);
        // Commit any in-flight character on this IC before wiping, so switching
        // mid-input commits the pending char (like a release would) instead of
        // dropping it. cyclingInput_ (an accent variant) and waitingKey_ (the
        // pre-leader base char) are mutually exclusive: cycling resets
        // waitingKey_. Cycling uses the OLD umlautMap_, since the pending char
        // belongs to the profile being left.
        if (st->cyclingInput_) {
            auto it = umlautMap_.find(*st->cyclingInput_);
            if (it != umlautMap_.end() &&
                st->cyclingIndex_ < it->second.size()) {
                ic->inputPanel().reset();
                ic->updatePreedit();
                ic->commitString(it->second[st->cyclingIndex_]);
                recordUsage(*st->cyclingInput_, it->second[st->cyclingIndex_]);
            }
        } else if (st->waitingKey_) {
            commitPendingKey(ic, st);
        }
        // Preserve this IC's held-key set across the wipe: clearAllState() would
        // clear it, so the still-held switch combo's next auto-repeat would
        // re-qualify as a fresh press and cycle again per repeat tick (each a
        // disk write). Keeping it marks the combo as held, suppressing repeats
        // until real release.
        auto heldKeys = st->heldRawCodes_;
        // Symmetry with the heldRawCodes_ preserve above: keep the single-output
        // repeat-suppression arming (the whole committed_ bundle) across the wipe
        // too, so a still-held combo doesn't lose its guard on a profile switch
        // (issue #92). Rarely armed on the switch combo, purely state-preserving.
        const auto heldCommitted = st->committed_;
        wipeAllGestureState();
        st->heldRawCodes_ = std::move(heldKeys);
        st->committed_ = heldCommitted;
        profiles_.active.setValue(name);
        umlautMap_ = buildRuntimeMap();
        safeSaveAsIni(profiles_, std::string(schnelle_umlaute::kConfigSubdir) +
                                     "/" + schnelle_umlaute::kProfilesConf);
        flashProfileName(ic, name);
        FCITX_INFO() << "Schnelle: Switched to profile '" << name
                     << "', Mappings=" << umlautMap_.size();
    }

    // Cycle the active profile by delta (+1 next, -1 previous) through the
    // favorites (or all profiles when none are marked favorite).
    void cycleProfile(InputContext *ic, int delta) {
        std::vector<schnelle_umlaute::CycleEntry> entries;
        for (const auto &p : *profiles_.profiles)
            entries.push_back({*p.name, *p.favorite});
        switchToProfileName(
            ic, schnelle_umlaute::cycleTarget(
                    schnelle_umlaute::cycleNames(entries), *profiles_.active,
                    delta));
    }

    // Brief on-switch feedback: show the new profile name where output is
    // visible (the daemon overlay when enabled and not in caret placement,
    // otherwise the caret candidate window), then auto-hide after a readable
    // delay (longer than the commit flash), like the input-method-switch popup.
    static constexpr int kProfileFlashMs = 1500;
    void flashProfileName(InputContext *ic, const std::string &name) {
        profileFlashHideEvent_.reset(); // supersede any in-flight flash
        const bool useDaemon = *config_.overlay->enabled && !overlayAtCaret();
        if (useDaemon) {
            overlayClient_.show({name}, kPreviewNoHighlight,
                                overlayPositionString(), /*label=*/true);
            overlayVisible_ = true;
        } else {
            showCaretOverlay(ic, {name}, kPreviewNoHighlight);
        }
        auto savedRef = ic->watch();
        uint64_t target = SchnelleUmlauteState::nowUsec() +
                          static_cast<uint64_t>(kProfileFlashMs) *
                              kMicrosecondsPerMillisecond;
        profileFlashHideEvent_ = instance_->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, target, 0,
            [this, savedRef, useDaemon](EventSourceTime *, uint64_t) {
                if (savedRef.get()) {
                    if (useDaemon)
                        overlayClient_.hide();
                    else
                        hideCaretOverlay();
                    overlayVisible_ = false;
                }
                return false;
            });
    }

    // Apply in-memory config: rebuild mappings, sanitize custom key, log.
    // Shared by setConfig (values already loaded) and reloadConfig (read from
    // disk).
    void applyConfig() {
        // Consume a pending usage-reset request (the editor's sidecar marker)
        // before building the runtime map, so it sorts on empty counts (=
        // stored order) and the runtime cycle matches the editor preview at
        // once. Not gated on the sort toggle: a reset is valid while off too.
        if (schnelle_umlaute::takeUsageResetMarker()) {
            usageCounts_.clear();
            usageDirty_ = false;
            schnelle_umlaute::deleteUsage();
        }
        umlautMap_ = buildRuntimeMap();
        rebuildProfileShortcuts();
        applyUsageTracking();

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
        // The physical key behind each leader, i.e. what actually triggers it.
        cachedCustomKeyCode_ =
            *config_.leader->custom->customKeyEnabled
                ? sanitizeKeyCode(*config_.leader->custom->customKeyCode)
                : kNoKeyCode;
        cachedCustomKey2Code_ =
            *config_.leader->custom->customKey2Enabled
                ? sanitizeKeyCode(*config_.leader->custom->customKey2Code)
                : kNoKeyCode;

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

        // An enabled leader with no captured key cannot trigger anything.
        if (*config_.leader->custom->customKeyEnabled &&
            cachedCustomKeyCode_ == kNoKeyCode) {
            FCITX_WARN() << "Schnelle: CustomKey has no key assigned"
                         << ", press the key in the editor to set it";
        }
        if (*config_.leader->custom->customKey2Enabled &&
            cachedCustomKey2Code_ == kNoKeyCode) {
            FCITX_WARN() << "Schnelle: CustomKey2 has no key assigned"
                         << ", press the key in the editor to set it";
        }

        // Warn when the split cannot apply. Mirrors isDualCustomAllowed(), so
        // the log explains why both leaders trigger everything.
        if (cachedCustomKeyCode_ != kNoKeyCode &&
            cachedCustomKey2Code_ != kNoKeyCode) {
            if (cachedCustomKeyCode_ == cachedCustomKey2Code_) {
                FCITX_WARN()
                    << "Schnelle: CustomKey and CustomKey2 are the same key"
                    << " — dual split disabled, both trigger all mappings";
            } else if (isLeftHandKeycode(cachedCustomKeyCode_) ==
                       isLeftHandKeycode(cachedCustomKey2Code_)) {
                FCITX_WARN()
                    << "Schnelle: CustomKey '"
                    << customLeaderLabel(cachedCustomKey_, cachedCustomKeyCode_)
                    << "' and CustomKey2 '"
                    << customLeaderLabel(cachedCustomKey2_, cachedCustomKey2Code_)
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
            leaders += "Alt ";
        if (*config_.leader->altGr)
            leaders += "AltGr ";
        // Gate on the keycode, not the character: a keycode-only leader (a
        // navigation key like Home) has an empty character but is still active,
        // so show its keycode instead of dropping it from the summary.
        if (cachedCustomKeyCode_ != kNoKeyCode)
            leaders += "Custom1('" +
                       customLeaderLabel(cachedCustomKey_, cachedCustomKeyCode_) +
                       "') ";
        if (cachedCustomKey2Code_ != kNoKeyCode)
            leaders +=
                "Custom2('" +
                customLeaderLabel(cachedCustomKey2_, cachedCustomKey2Code_) +
                "') ";
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

    // Every gesture advance ends here: starting the waiting phase, starting to
    // cycle, and each step to another variant all rewrite the preview.
    void updateClientPreedit(InputContext *ic, SchnelleUmlauteState *state,
                             const std::string &text) {
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
                        recordUsage(*state->cyclingInput_,
                                    it->second[state->cyclingIndex_]);
                        ctx->updatePreedit();
                        state->recentlyCommitted_ = true;
                    }
                    state->resetCycling();
                    overlayHide();
                    state->resetWaitingGesture();
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
        state->resetWaitingGesture();
        state->cancelTimeout();
        state->recentlyCommitted_ = true;
    }

    // Deliver the trailing space of a char+space commit in its own event-loop
    // turn (issue #90). The XIM frontend (always) and the DBus frontend (for
    // clients with the KeyEventOrderFix capability, e.g. the GTK module)
    // buffer every commit issued while a key event is being processed and
    // deliver them merged as ONE string (deliverBlockedEvents()), so two
    // back-to-back commitString calls still reach the app as a combined
    // "a ". Apps that evaluate text inserts per event (monkeytype.com) then
    // drop the letter and only register the space. A zero-delay timer moves
    // the space into its own delivery, giving every frontend two
    // single-character inserts. Order is safe twice over: both commits stay
    // on the IME channel, and a key event that wins the race against the
    // timer flushes the space first (flushPendingSpaceCommit() at the top of
    // keyEvent()). This is the same transport the window-timeout commit has
    // shipped since fa13cf7: timer commit, space routed as its own commit.
    void scheduleSpaceCommit(InputContext *ic, SchnelleUmlauteState *state) {
        auto savedRef = ic->watch();
        auto *eventLoop = &instance_->eventLoop();
        state->pendingSpaceCommit_ = true;
        state->spaceCommitEvent_ = eventLoop->addTimeEvent(
            CLOCK_MONOTONIC, SchnelleUmlauteState::nowUsec(), 0,
            [this, state, savedRef](EventSourceTime *, uint64_t) {
                // Safety: see scheduleTimeout (single-threaded event loop
                // guarantees state outlives savedRef.get() != nullptr).
                auto *ctx = savedRef.get();
                if (!ctx)
                    return false;
                flushPendingSpaceCommit(ctx, state);
                // Don't reset spaceCommitEvent_ here: destroying the
                // EventSource inside its own callback is a use-after-free
                // risk. pendingSpaceCommit_ is the source of truth; the
                // unique_ptr is cleaned up by the next scheduleSpaceCommit()
                // or cancelSpaceCommit() call.
                return false;
            });
    }

    void flushPendingSpaceCommit(InputContext *ic,
                                 SchnelleUmlauteState *state) {
        if (!state->pendingSpaceCommit_)
            return;
        state->pendingSpaceCommit_ = false;
        ic->commitString(" ");
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
            recordUsage(cyclingInput, it->second[state->cyclingIndex_]);
            ic->updatePreedit();
            state->recentlyCommitted_ = true;
        }
        state->resetWaitingGesture();
        state->resetCycling();
        // Committing the cycling value ends the gesture, and with it any
        // Alt-led session that drove it. Leaving altGestureSession_ set here
        // would keep the Alt-leader bypass armed with nothing live behind it,
        // so the next Alt+key application shortcut would be committed as text
        // while that Alt is still held (issue #147 class). The deferred-commit
        // KWin Wayland auto-repeat gap it guards is untouched.
        state->altGestureSession_ = false;
        overlayHide();
    }

    // are valid (e.g. mapping a key to " " so terminal commands skip history).
    // Check for Ctrl/Alt/Super in key state. Shift is intentionally
    // excluded — it is needed for uppercase accent mappings (Shift+A → Ä).
    static bool hasModifiers(const Key &key) {
        KeyStates mods = key.states();
        return mods.test(KeyState::Ctrl) || mods.test(KeyState::Alt) ||
               mods.test(KeyState::Super);
    }

    // Every modifier that can change which character a key produces. AltGr is
    // the one that is easy to miss: it is the level-3 shift and reports as
    // Mod5, NOT as Alt (which is Mod1), so a guard testing Alt alone would let
    // AltGr through and learn the level-3 character as if it were the base one.
    // NumLock is deliberately absent: it only switches the keypad, never a
    // letter.
    static KeyStates charChangingModifiers() {
        return KeyStates(KeyState::Shift) | KeyState::CapsLock |
               KeyState::Ctrl | KeyState::Alt | KeyState::Super |
               KeyState::Hyper | KeyState::Meta | KeyState::Mod5;
    }

    // Record what a physical key produces when pressed with no modifiers at
    // all. This is the user's real layout, observed rather than assumed: fcitx5
    // resolved the keysym through the XKB state that actually governs the
    // session, so the pair is correct on every layout.
    //
    // Takes the RAW key, never KeyEvent::key(). The normalized key is unusable
    // for deciding "unmodified": Key::normalize() keeps only Ctrl/Alt/Shift/
    // Super, and drops Shift outright for a-z/A-Z. AltGr (Mod5) and CapsLock
    // would therefore be invisible here, and Shift+a would arrive looking like
    // an unmodified 'A'. Each of those would teach a key a wrong base character.
    //
    // A key's entry is refreshed the next time it is pressed unmodified, so
    // after a layout switch every key corrects itself on its first plain press.
    // Until then its old character is served, which the sole consumer tolerates:
    // it only ever falls back to the keysym's own character. That consumer is
    // the Alt-leader bypass (see getBaseChar). Keys never pressed unmodified
    // stay unknown.
    void learnBaseChar(const Key &rawKey, int rawCode) {
        if (rawCode == kNoKeyCode)
            return;
        if (rawKey.states().testAny(charChangingModifiers()))
            return;
        const uint32_t unicode = Key::keySymToUnicode(rawKey.sym());
        if (unicode == 0 || unicode > kMaxUnicodeCodepoint)
            return;
        baseCharByCode_[rawCode] = utf8::UCS4ToUTF8(unicode);
    }

    // The unmodified character of a physical key, or empty when that key has
    // not been pressed unmodified yet. Never guesses.
    std::string getBaseChar(int rawCode) const {
        const auto it = baseCharByCode_.find(rawCode);
        return it == baseCharByCode_.end() ? std::string() : it->second;
    }

    // Alt vs AltGr, split only for enabling and direction. The left Alt is
    // "Alt"; AltGr is ISO_Level3_Shift plus the right Alt (Alt_R), which on EU
    // layouts is the AltGr key. isAltLeaderSym stays the union of both and keeps
    // driving the shared Alt-gesture machinery (deferred commit, gesture
    // session, repeat suppression) for either key.
    static bool isAltSym(KeySym sym) { return sym == FcitxKey_Alt_L; }
    static bool isAltGrSym(KeySym sym) {
        return sym == FcitxKey_Alt_R || sym == FcitxKey_ISO_Level3_Shift;
    }
    static bool isAltLeaderSym(KeySym sym) {
        return isAltSym(sym) || isAltGrSym(sym);
    }

    // Cycle step for a leader press: -1 when that key's reverse flag is set,
    // +1 otherwise. Space, each arrow, each of Alt / AltGr, and each custom
    // leader carries its own flag, so any of them can go forward or backward
    // independently. Space, the arrows and Alt / AltGr are matched by keysym; a
    // custom leader IS its physical key, so it is matched by keycode. The
    // precedence (Alt / AltGr, then custom, then Space, then arrows) mirrors
    // classifyLeader, so the step direction always comes from the same flag that
    // classified the press. The step sign only moves the index; it is orthogonal
    // to the Alt-gesture machinery (which keys off isAltLeaderSym, unchanged).
    // Forward and reverse presses act on the same cyclingIndex_, so both can be
    // mixed freely inside one session.
    int leaderStep(const Key &key, int rawCode) const {
        KeySym sym = key.sym();
        bool reverse = false;
        if (isAltSym(sym))
            reverse = *config_.leader->altReverse;
        else if (isAltGrSym(sym))
            reverse = *config_.leader->altGrReverse;
        else if (matchCustomLeader(cachedCustomKeyCode_, rawCode))
            reverse = *config_.leader->custom->customKeyReverse;
        else if (matchCustomLeader(cachedCustomKey2Code_, rawCode))
            reverse = *config_.leader->custom->customKey2Reverse;
        else if (sym == FcitxKey_space)
            reverse = *config_.leader->spaceReverse;
        else if (sym == FcitxKey_Left)
            reverse = *config_.leader->leftReverse;
        else if (sym == FcitxKey_Right)
            reverse = *config_.leader->rightReverse;
        else if (sym == FcitxKey_Up)
            reverse = *config_.leader->upReverse;
        else if (sym == FcitxKey_Down)
            reverse = *config_.leader->downReverse;
        return reverse ? -1 : +1;
    }

    // A custom leader IS its physical key, so matching is a keycode comparison
    // and nothing else. The leader fires whatever character the key currently
    // produces: through Shift, through any layout, through any script. An
    // unconfigured leader (kNoKeyCode) matches nothing.
    static bool matchCustomLeader(int customKeyCode, int rawCode) {
        return customKeyCode != kNoKeyCode && rawCode == customKeyCode;
    }

    // A custom leader's label for diagnostics: its character, or "#<keycode>"
    // for a keycode-only leader (a navigation key with no character). Keyed on
    // the keycode, like matchCustomLeader, so logs name exactly what can fire.
    static std::string customLeaderLabel(const std::string &ch, int keyCode) {
        return ch.empty() ? "#" + std::to_string(keyCode) : ch;
    }

    // Leader classification for dual custom leader support.
    // Built-in leaders (Space, Arrows, Alt) are unrestricted.
    // Custom1/Custom2 may be restricted by dual-split logic.
    enum class LeaderType { None, BuiltIn, Custom1, Custom2 };

    LeaderType classifyLeader(const Key &key, int rawCode) const {
        KeySym sym = key.sym();

        // Alt and AltGr — built-in, unrestricted, each enabled independently.
        if (*config_.leader->alt && isAltSym(sym))
            return LeaderType::BuiltIn;
        if (*config_.leader->altGr && isAltGrSym(sym))
            return LeaderType::BuiltIn;

        // Custom leaders: the captured physical keys
        if (matchCustomLeader(cachedCustomKeyCode_, rawCode))
            return LeaderType::Custom1;
        if (matchCustomLeader(cachedCustomKey2Code_, rawCode))
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

    // Dual custom leader split: when BOTH custom leaders are configured and sit
    // on opposite keyboard halves, each one only triggers inputs on the OTHER
    // half. Built-in leaders are always unrestricted.
    //
    // Every hand comes from a physical keycode: the leaders' from the config
    // (captured in the editor), the input key's from the key event via
    // waitingKeyCode_, which stays valid through cycling. Nothing is resolved
    // from a character, so the rule holds on every layout and across a layout
    // switch.
    //
    // The split switches off, and every leader then triggers everything, when it
    // no meaning: only one leader configured, or both on the same key or the
    // same half.
    bool isDualCustomAllowed(LeaderType leader, int inputKeyCode) const {
        if (leader == LeaderType::BuiltIn || leader == LeaderType::None)
            return true;

        // Dual mode only when BOTH custom leaders are configured
        if (cachedCustomKeyCode_ == kNoKeyCode ||
            cachedCustomKey2Code_ == kNoKeyCode)
            return true;

        // Same physical key → no split
        if (cachedCustomKeyCode_ == cachedCustomKey2Code_)
            return true;

        const bool key1Left = isLeftHandKeycode(cachedCustomKeyCode_);
        const bool key2Left = isLeftHandKeycode(cachedCustomKey2Code_);

        // Both leaders on the same half → no split possible, allow all
        if (key1Left == key2Left)
            return true;

        // The input key always carries its keycode (waitingKeyCode_ is set on
        // the press and outlives the switch to cycling), so this guard should
        // never fire. Allow rather than classify a missing code as right-hand.
        if (inputKeyCode == kNoKeyCode)
            return true;

        const bool inputLeft = isLeftHandKeycode(inputKeyCode);

        // Left-hand leader triggers RIGHT-hand inputs (and vice versa)
        if (leader == LeaderType::Custom1)
            return key1Left ? !inputLeft : inputLeft;
        else // Custom2
            return key2Left ? !inputLeft : inputLeft;
    }

    // The accent window one gesture key gets. A key of nullptr reads as
    // lowercase: that is the window a gesture without a key yet would use.
    // ASCII-only uppercase check, sufficient because input keys are physical
    // keyboard keys which are always single ASCII bytes.
    int delayForKey(const std::string *key) const {
        const bool isUpper =
            key && key->length() == 1 && (*key)[0] >= 'A' && (*key)[0] <= 'Z';
        return isUpper ? *config_.delay->uppercase : *config_.delay->lowercase;
    }

    int getEffectiveDelay(const SchnelleUmlauteState *state) const {
        return delayForKey(state->waitingKey_ ? &*state->waitingKey_ : nullptr);
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
                    // If the key is still physically held past the accent
                    // window, its auto-repeat keeps arriving after this commit.
                    // On a synthetic-release platform (Wayland) a trailing
                    // release will follow, so arm committed_ to consume it via
                    // the committed-key release branch instead of leaking an
                    // unpaired key-up to the app (issue #73 robustness). Arm with
                    // time == 0 (and startUsec == 0) so the release branch does
                    // NOT keep the arming for a synthetic release (unlike the
                    // single-output sites, issue #92 hole 2): the release clears
                    // the code again, and each window cycle re-arms it, so the
                    // held key still restarts a gesture and repeats as intended.
                    // Explicit 0 is an invariant, not a "happens to be 0" bet — a
                    // prior single-output commit could otherwise leave a stale
                    // time. Gated on sawSyntheticRelease_ so press-only auto-
                    // repeat (classic X11) keeps its historic per-window behavior.
                    if (state->sawSyntheticRelease_)
                        state->armCommittedKey(state->waitingKeyCode_, 0, 0);
                    state->resetWaitingGesture();
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

    // A keycode is only meaningful if it can name a real key. The config is a
    // plain text file, so a hand-edited value outside the pressable range has to
    // collapse to kNoKeyCode here. Left as-is, it would count as "leader
    // configured" (arming the hand-split and silencing the no-key warning) while
    // matching no key that can ever be pressed. That holds at both ends: -1 and
    // 99999 are equally unreachable.
    static int sanitizeKeyCode(int raw) {
        return isUsableKeyCode(raw) ? raw : kNoKeyCode;
    }

    // Normalise a custom leader's stored character: trim whitespace, keep only
    // the first UTF-8 character, lowercase ASCII letters. The character does not
    // trigger the leader (its keycode does), so this only shapes what is shown
    // and what the mapped-input collision check compares. A hand-edited config
    // cannot smuggle a multi-character or padded string into either.
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
    // Parsed profile-switch shortcuts, cached from profiles_ in applyConfig so
    // keyEvent only does Key::check, no per-keystroke string parsing.
    struct ProfileShortcut {
        Key key;
        std::string name;
    };
    std::vector<ProfileShortcut> profileSelectShortcuts_;
    Key cycleNextKey_;
    Key cyclePrevKey_;
    // Dedicated timer for the profile-name flash auto-hide, kept off the per-IC
    // overlayHideEvent_ so a following gesture's cancelOverlayHide() cannot
    // cancel it and leave the name stuck on screen.
    std::unique_ptr<EventSourceTime> profileFlashHideEvent_;

    // Mappings (shared across all InputContexts, read-only after config load)
    std::unordered_map<std::string, std::vector<std::string>> umlautMap_;
    // The same mappings in their STORED order (merge-composed, before the
    // frequency sort). Kept so a commit can re-sort a single key's cycle live
    // from the stored order (tie-break = stored), matching the build-time pass
    // and the editor preview exactly instead of drifting on equal counts.
    std::unordered_map<std::string, std::vector<std::string>> storedMap_;

    // Per-(base char, committed variant) usage counters. Loaded once at
    // startup, incremented in memory on every variant commit, and flushed to
    // disk in batches (focus-out, periodic timer, shutdown). The frequency sort
    // reads them; the editor reads the flushed file to preview the same order.
    schnelle_umlaute::UsageCounts usageCounts_;
    bool usageDirty_ = false;
    std::unique_ptr<EventSourceTime> usageFlushEvent_;
    // The character each custom leader printed when it was captured. Not used
    // for matching, only for log messages and the mapped-input collision
    // warning.
    std::string cachedCustomKey_;
    std::string cachedCustomKey2_;
    // The physical key (evdev+8) behind each custom leader, captured in the
    // editor. This is what triggers the leader and what the hand-split
    // classifies. kNoKeyCode → the leader is off or has no key assigned.
    int cachedCustomKeyCode_ = kNoKeyCode;
    int cachedCustomKey2Code_ = kNoKeyCode;
    // Physical key (evdev+8) → the character it produces unmodified, learned
    // from the user's own keystrokes. See learnBaseChar(). Bounded by the
    // keyboard's key count, so it needs no eviction.
    std::unordered_map<int, std::string> baseCharByCode_;
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

    // The single "<Row><Col>" position string the overlay daemon expects (e.g.
    // "TopCol4"). In MouseCursor placement the shared cursor marker is
    // prefixed so the daemon anchors at the pointer, with the grid string as
    // the fallback when the compositor can't report it. Shared by overlayShow
    // and flashProfileName so the assembly lives in one place.
    std::string overlayPositionString() const {
        std::string position;
        if (*config_.overlay->placement == OverlayPlacement::MouseCursor)
            position = schnelle_umlaute::cursorPositionPrefix();
        position += OverlayRowToString(*config_.overlay->row);
        position += OverlayColumnToString(*config_.overlay->column);
        return position;
    }

    void overlayShow(InputContext *ic, const std::vector<std::string> &variants,
                     int index) {
        if (!*config_.overlay->enabled)
            return;
        // A real overlay (a gesture preview) supersedes a profile-name flash:
        // cancel the pending flash auto-hide so it can't later hide this
        // overlay. The gesture's own overlayHideEvent_ takes over hiding. All
        // gesture overlays (daemon and caret) route through here.
        profileFlashHideEvent_.reset();
        if (overlayAtCaret()) {
            showCaretOverlay(ic, variants, index);
            return;
        }
        overlayClient_.show(variants, index, overlayPositionString());
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
        list->setCursorIndex(
            index >= 0 ? index : schnelle_umlaute::kNoHighlightIndex);
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
        // Pass the gesture start so the daemon can pre-advance the bar by the
        // D-Bus delivery latency and stay in step with the real accent window.
        overlayClient_.setProgress(lead, window, state->startTimeUsec_);
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
    //
    // The daemon also reads it as "a gesture opened" to decide whether its
    // (persistent) QML may animate into the new state or has to snap, so the
    // value is a contract between the two processes and is defined once, in
    // overlay_protocol.h, rather than here and there.
    static constexpr int kPreviewNoHighlight =
        schnelle_umlaute::kNoHighlightIndex;

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
