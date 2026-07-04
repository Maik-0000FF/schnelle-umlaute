#ifndef SCHNELLE_UMLAUTE_SYNTHETIC_AUTOREPEAT_H
#define SCHNELLE_UMLAUTE_SYNTHETIC_AUTOREPEAT_H

// Pure predicate for detecting a synthetic auto-repeat key release, kept
// header-only and dependency-free so it can be unit-tested in isolation
// (mirrors layer_shell_capability.h / session_env.h).
//
// Background (issue #73): on a KWin/Wayland session a held key's auto-repeat
// is delivered as release-press pairs, and the compositor freezes the frontend
// event time (KeyEvent::time()) across the entire repeat burst: every
// synthetic release and re-press carries the timestamp of the original press,
// while the real, physical release that ends the burst carries an advanced
// timestamp. A release whose time equals the starting press's time is
// therefore synthetic and must not commit the pending character; any other
// release is genuine.
//
// The time==0 guard is essential: KeyEvent::time() defaults to 0 and some
// frontends (and the test frontend) never stamp events, which would make every
// release match a zero-initialised press time. Returning false there falls back
// to the historic "release commits immediately" behavior, so nothing outside a
// timestamp-stamping compositor is affected. Equality (not ordering) is used so
// the check stays correct even if time() wraps past INT_MAX on long uptimes.
//
// The elapsed-time guard closes the one hole in the equality check: timestamp
// equality alone cannot distinguish a frozen repeat burst from a GENUINE
// press+release pair that was generated within the same millisecond, which
// human fingers cannot produce but injected input can (ydotool/wtype,
// password-manager autotype, test automation). The two cases differ sharply in
// wall-clock terms: a real synthetic release arrives at least one auto-repeat
// period after its gesture's press (>= 10 ms even at aggressive repeat rates,
// ~600 ms initial delay for the first one), while an injected equal-timestamp
// pair is delivered back to back within well under a millisecond. Requiring a
// minimum monotonic gap between the gesture's press and the release keeps
// injected pairs on the historic immediate-commit path.

#include <cstdint>

namespace fcitx {

// Minimum monotonic time between a gesture's press and a release for the
// release to qualify as a synthetic auto-repeat. Chosen below the shortest
// realistic auto-repeat period (10 ms at a 100 Hz repeat rate) and far above
// the sub-millisecond delivery gap of an injected equal-timestamp pair.
constexpr uint64_t kSyntheticReleaseMinElapsedUsec = 5'000;

inline bool isSyntheticAutoRepeatRelease(int releaseTime, int pressTime,
                                         uint64_t elapsedUsec) {
    return releaseTime != 0 && releaseTime == pressTime &&
           elapsedUsec >= kSyntheticReleaseMinElapsedUsec;
}

} // namespace fcitx

#endif
