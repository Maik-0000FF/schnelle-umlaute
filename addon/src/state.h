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

    // Track if input key is physically pressed
    bool inputKeyPressed_ = false;
    int waitingKeyCode_ = 0;

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
        cancelOverlayShow();
        cancelOverlayHide();
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

    void cancelTimeout() { timeoutEvent_.reset(); }

    void cancelOverlayShow() { overlayShowEvent_.reset(); }

    void cancelOverlayHide() { overlayHideEvent_.reset(); }

    bool isTimeoutExpired(int effectiveDelay) const {
        if (!waitingKey_)
            return false;
        uint64_t now_usec = nowUsec();
        uint64_t elapsed_ms =
            (now_usec - startTimeUsec_) / kMicrosecondsPerMillisecond;
        return elapsed_ms > static_cast<uint64_t>(effectiveDelay);
    }

    // Lower bound of the accent window: true while the input key has been
    // held for less than minHoldMs, i.e. a leader arriving now is too early
    // to trigger the accent. minHoldMs <= 0 disables the lower bound, which
    // is the historic default.
    bool isBeforeMinHold(int minHoldMs) const {
        if (!waitingKey_ || minHoldMs <= 0)
            return false;
        uint64_t now_usec = nowUsec();
        uint64_t elapsed_ms =
            (now_usec - startTimeUsec_) / kMicrosecondsPerMillisecond;
        return elapsed_ms < static_cast<uint64_t>(minHoldMs);
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
