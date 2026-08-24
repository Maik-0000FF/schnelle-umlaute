#ifndef SCHNELLE_UMLAUTE_STATE_H
#define SCHNELLE_UMLAUTE_STATE_H

// Per-InputContext state for the Schnelle Umlaute addon. Each application
// window gets its own instance via InputContextProperty so focus switches
// between windows cannot corrupt one another's gesture state.

#include <fcitx-utils/event.h>
#include <fcitx/inputcontextproperty.h>

#include <cstdint>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

#include "synthetic_autorepeat.h"

namespace fcitx {

constexpr uint64_t kMicrosecondsPerSecond = 1'000'000;
constexpr uint64_t kNanosecondsPerMicrosecond = 1'000;
constexpr uint64_t kMicrosecondsPerMillisecond = 1'000;

class SchnelleUmlauteState : public InputContextProperty {
public:
    // Waiting state (before first Space)
    std::optional<std::string> waitingKey_;
    uint64_t startTimeUsec_ = 0;
    std::unique_ptr<EventSourceTime> timeoutEvent_;

    // Separate timer for the trigger-window preview overlay. Runs concurrently
    // with timeoutEvent_ (the accent window's upper bound), so it cannot share
    // that single slot — it fires once the minimum hold elapses to preview the
    // mapping's variants before any leader is pressed.
    std::unique_ptr<EventSourceTime> overlayShowEvent_;

    // One-shot timer that hides the overlay a short moment after a single-
    // mapping commit, so the accent cell can flash to confirm the commit
    // instead of vanishing in the same frame. Lives in its own slot because
    // it can be pending while a fresh preview is being scheduled.
    std::unique_ptr<EventSourceTime> overlayHideEvent_;

    // Zero-delay timer that delivers the trailing space of a char+space
    // commit in its own event-loop turn (see scheduleSpaceCommit()). Own
    // slot: sharing timeoutEvent_ would let the next gesture's
    // scheduleTimeout() cancel a still-pending space. pendingSpaceCommit_
    // is the source of truth (the EventSourceTime stays non-null after
    // firing, and a callback must not destroy its own source).
    std::unique_ptr<EventSourceTime> spaceCommitEvent_;
    bool pendingSpaceCommit_ = false;

    // Backstop for the cycling phase, which has no timer of its own: the leader
    // press cancels the accent window, and from then on only the input key's
    // release ends the gesture. A compositor grab can swallow that release
    // (KWin's window operations menu on Alt+Space, issue #147) without moving
    // the focus, so no release and no FocusOut ever arrive and the gesture
    // would stay live for the rest of the session. This timer ends it the way
    // the release would have, by committing the variant on screen. Every step
    // of the gesture re-arms it, so cycling is only ever cut short by doing
    // nothing at all. Own slot: timeoutEvent_ carries the Alt deferred commit
    // during the same phase.
    std::unique_ptr<EventSourceTime> cyclingWatchdogEvent_;
    // True only while the watchdog callback runs. Destroying an EventSourceTime
    // from inside its own callback is a use-after-free, and the callback
    // commits through the shared path, which tears the gesture down and would
    // cancel the timer. The guard makes that cancel a no-op; returning false
    // from the callback disables the source, and the next arming replaces it.
    bool cyclingWatchdogFiring_ = false;

    // Monotonic stamp of the last VISIBLE progress of the cycling phase, i.e.
    // the last time the picker actually moved. The ceiling in
    // armCyclingWatchdog() is measured from here rather than from the gesture's
    // start, so deliberate cycling pushes it along while invisible liveness
    // does not (see kCyclingWatchdogCapFactor). Zero while nothing is cycling.
    uint64_t lastCyclingProgressUsec_ = 0;

    // Track if input key is physically pressed
    bool inputKeyPressed_ = false;
    int waitingKeyCode_ = 0;
    // Frontend event time (KeyEvent::time(), ms) of the press that started the
    // current waiting gesture. On KWin/Wayland the compositor freezes the
    // event time across a held key's whole auto-repeat burst, so a release
    // carrying this exact (nonzero) time is a synthetic auto-repeat, not a real
    // release. See isSyntheticAutoRepeatRelease() / issue #73.
    int waitingKeyTime_ = 0;
    // True once a synthetic auto-repeat release has been observed on this input
    // context, i.e. the platform delivers auto-repeat as release-press pairs
    // (Wayland). Persists across gestures but is reset on every focus change
    // (clearAllState runs in activate/deactivate), so it effectively marks the
    // current focus session: within one, every hold past the first suppression
    // is armed, while the first over-window hold after a (re)focus can still
    // leak a single unpaired key-up before the marker latches. Gates the
    // window-timeout committed_ arming so press-only auto-repeat (classic X11)
    // is left untouched. See isSyntheticAutoRepeatRelease() / issue #73.
    bool sawSyntheticRelease_ = false;

    // Set after commit to route next Space through commitString (ordering
    // guard). Intentionally NOT cleared in clearAllState() — apps like WezTerm
    // and Chromium call reset() after every commit, which would destroy the
    // ordering guard before Space arrives.
    bool recentlyCommitted_ = false;

    // Cycling state (after first Space, while input key held)
    std::optional<std::string> cyclingInput_;
    size_t cyclingIndex_ = 0;

    // Track physically held keys to distinguish fresh presses from repeats
    std::unordered_set<int> heldRawCodes_;

    // Repeat-suppression arming for a held accent key after a single-output
    // commit: while the key stays physically down, its auto-repeat is consumed
    // instead of starting a fresh gesture that would duplicate the character
    // (e.g. "üu"). The three values are bundled so they always move together;
    // a bare field trio drifts out of sync across the arming, clear,
    // reset-preserve and profile-switch sites. Coverage is now uniform: full on
    // X11 (press-only repeat) AND on synthetic release-press platforms
    // (KWin/Wayland), where the frozen press timestamp lets the release branch
    // keep the arming across the whole burst (issue #92 hole 2).
    //
    //  - code:      raw keycode of the committed, still-held key (0 == not armed).
    //  - time:      frozen frontend event time (KeyEvent::time(), ms) of its
    //               press. A synthetic KWin/Wayland auto-repeat release carries
    //               this exact time and must NOT drop the arming. The window-
    //               timeout arming leaves this 0 on purpose: a 0 press time never
    //               equals a real release time, so its release clears the code
    //               per window, keeping the intended one-char-per-window repeat.
    //  - startUsec: monotonic press time of THIS committed gesture, the elapsed
    //               reference for the synthetic-release predicate. Must not use
    //               the global startTimeUsec_, which a later gesture overwrites.
    struct CommittedKey {
        int code = 0;
        int time = 0;
        uint64_t startUsec = 0;
    };
    CommittedKey committed_;

    // Track consumed Alt/AltGr leader press to also consume the release.
    // Prevents compositor state confusion from an orphan modifier release
    // and TUI side effects from stray Alt release events.
    int consumedAltCode_ = 0;

    // Active Alt-led cycling session. Set when Alt starts cycling,
    // cleared only by deferred commit timer or clearAllState().
    // Survives auto-repeat release-press gaps on KWin Wayland where
    // cycling is temporarily reset between pairs.
    bool altGestureSession_ = false;

    // Tear down the waiting-gesture bundle in one place so no commit or
    // cancel site can forget a field (reset symmetry). Deliberately excludes
    // sawSyntheticRelease_ (persistent platform marker, see its comment) and
    // the timers (some callers must not touch a timer from inside its own
    // callback, see the window-timeout commit).
    void resetWaitingGesture() {
        waitingKey_.reset();
        waitingKeyCode_ = 0;
        waitingKeyTime_ = 0;
        inputKeyPressed_ = false;
    }

    // Arm/clear the committed-key repeat suppression as one unit, so code, its
    // frozen press timestamp and its monotonic start never drift apart. Pass
    // time=0/startUsec=0 to opt a site out of synthetic-release keeping (the
    // window-timeout path), which then clears on the next release as before.
    void armCommittedKey(int code, int time, uint64_t startUsec) {
        committed_ = {code, time, startUsec};
    }
    // Arm committed-key suppression from the current waiting gesture's fields.
    // Call BEFORE resetWaitingGesture()/commitPendingKey() clears them; carries
    // the "capture waiting code/time/start together, in this order" invariant
    // that every single-output commit site shares (issue #92 hole 2).
    void armCommittedFromWaiting() {
        armCommittedKey(waitingKeyCode_, waitingKeyTime_, startTimeUsec_);
    }
    void clearCommittedKey() { committed_ = {}; }

    // True once no gesture and no Alt-led session is live any more: the
    // state in which a still-armed consumedAltCode_ is either the awaited,
    // symmetric leader release or provably stale. Gates both the release
    // eater's one-shot disarm and the press-side stale disarm as the ONE
    // definition of "session over", so the two sites cannot drift apart.
    // During KWin Wayland auto-repeat gaps cycling keeps cyclingInput_ and
    // altGestureSession_ set, so this stays false there and the arming
    // survives the gap.
    bool altSessionOver() const {
        return !waitingKey_ && !cyclingInput_ && !altGestureSession_;
    }

    // Classify a key release as a synthetic auto-repeat for the waiting or the
    // committed gesture. Each binds isSyntheticAutoRepeatRelease() to its own
    // bundle's (frozen press time, monotonic start) pair and owns the nowUsec()
    // math, so a call site cannot pair one gesture's timestamp with the other's
    // start. releaseTime is the release's frontend event time. See
    // synthetic_autorepeat.h / issue #73.
    bool isSyntheticWaitingRelease(int releaseTime) const {
        return isSyntheticAutoRepeatRelease(releaseTime, waitingKeyTime_,
                                            nowUsec() - startTimeUsec_);
    }
    bool isSyntheticCommittedRelease(int releaseTime) const {
        return isSyntheticAutoRepeatRelease(releaseTime, committed_.time,
                                            nowUsec() - committed_.startUsec);
    }

    void clearAllState() {
        resetWaitingGesture();
        sawSyntheticRelease_ = false;
        // Note: recentlyCommitted_ is intentionally NOT cleared here.
        cancelTimeout();
        cancelOverlayShow();
        cancelOverlayHide();
        cancelSpaceCommit();
        resetCycling();
        heldRawCodes_.clear();
        clearCommittedKey();
        consumedAltCode_ = 0;
        altGestureSession_ = false;
    }

    // The one teardown every end of a cycling phase runs through (commit,
    // cancel, wipe), so the watchdog cannot outlive the gesture it guards.
    void resetCycling() {
        cyclingInput_.reset();
        cyclingIndex_ = 0;
        cancelCyclingWatchdog();
        lastCyclingProgressUsec_ = 0;
    }

    void cancelTimeout() { timeoutEvent_.reset(); }

    void cancelOverlayShow() { overlayShowEvent_.reset(); }

    void cancelOverlayHide() { overlayHideEvent_.reset(); }

    void cancelSpaceCommit() {
        spaceCommitEvent_.reset();
        pendingSpaceCommit_ = false;
    }

    // No-op while the watchdog is firing: see cyclingWatchdogFiring_.
    void cancelCyclingWatchdog() {
        if (cyclingWatchdogFiring_)
            return;
        cyclingWatchdogEvent_.reset();
    }

    // Milliseconds between a monotonic stamp and now. The one place the
    // microsecond clock is converted, so the two time predicates below cannot
    // drift apart on the unit.
    static uint64_t elapsedMsSince(uint64_t stampUsec) {
        return (nowUsec() - stampUsec) / kMicrosecondsPerMillisecond;
    }

    bool isTimeoutExpired(int effectiveDelay) const {
        if (!waitingKey_)
            return false;
        return elapsedMsSince(startTimeUsec_) >
               static_cast<uint64_t>(effectiveDelay);
    }

    // Lower bound of the accent window: true while the input key has been
    // held for less than minHoldMs, i.e. a leader arriving now is too early
    // to trigger the accent. minHoldMs <= 0 disables the lower bound, which
    // is the historic default.
    bool isBeforeMinHold(int minHoldMs) const {
        if (!waitingKey_ || minHoldMs <= 0)
            return false;
        return elapsedMsSince(startTimeUsec_) <
               static_cast<uint64_t>(minHoldMs);
    }

    static uint64_t nowUsec() {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * kMicrosecondsPerSecond +
               ts.tv_nsec / kNanosecondsPerMicrosecond;
    }
};

} // namespace fcitx

#endif
