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
// timestamp. That makes the event time an exact, non-heuristic discriminator:
// a release whose time equals the starting press's time is synthetic and must
// not commit the pending character; any other release is genuine.
//
// The time==0 guard is essential: KeyEvent::time() defaults to 0 and some
// frontends (and the test frontend) never stamp events, which would make every
// release match a zero-initialised press time. Returning false there falls back
// to the historic "release commits immediately" behavior, so nothing outside a
// timestamp-stamping compositor is affected. Equality (not ordering) is used so
// the check stays correct even if time() wraps past INT_MAX on long uptimes.

namespace fcitx {

inline bool isSyntheticAutoRepeatRelease(int releaseTime, int pressTime) {
    return releaseTime != 0 && releaseTime == pressTime;
}

} // namespace fcitx

#endif
