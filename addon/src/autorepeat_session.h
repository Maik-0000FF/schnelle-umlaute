#ifndef SCHNELLE_UMLAUTE_AUTOREPEAT_SESSION_H
#define SCHNELLE_UMLAUTE_AUTOREPEAT_SESSION_H

// Pure detection of whether the current session delivers a held key's
// auto-repeat as synthetic release-press pairs. Deliberately free of
// Qt/DBus/fcitx5 deps so it is unit-testable in isolation, mirroring
// layer_shell_capability.h and session_env.h.
//
// Background (issue #73): on a Wayland session, KWin (and Wayland
// compositors generally) surface keyboard auto-repeat through wl_keyboard as
// a release immediately followed by a re-press of the same physical key,
// rather than as repeat-presses with no release. That fools the pre-leader
// waiting-key release path into committing the held character prematurely,
// before the accent window elapses. X11 sessions deliver auto-repeat as
// repeat-presses (no synthetic release), so the release is always genuine and
// the historic immediate-commit behavior is correct there.
//
// The engine reads this once and only enables the release-commit deferral in
// sessions that send synthetic pairs, leaving X11/terminal behavior — and its
// "release = synchronous commit" invariant — byte-for-byte unchanged.

#include <cstdlib>
#include <cstring>

namespace fcitx {

// True when the session delivers auto-repeat as synthetic release-press
// pairs, i.e. any Wayland session. Detected from the presence of a Wayland
// display (`waylandDisplay`, the most direct signal that a Wayland compositor
// is driving input) or an explicit `XDG_SESSION_TYPE=wayland`. Both arguments
// may be nullptr/empty when the variable is unset.
//
// Over-inclusiveness is deliberate and harmless: an XWayland (X11-protocol)
// app inside a Wayland session would also be treated as Wayland, but it does
// not send synthetic pairs, so the deferral there merely commits after the
// short timer with no re-press to bridge — an imperceptible delay, no
// behavior change. Only a genuine, fully non-Wayland session (no Wayland
// display, session type not "wayland") disables the deferral.
inline bool sessionSendsSyntheticRepeats(const char *sessionType,
                                         const char *waylandDisplay) {
    if (waylandDisplay && *waylandDisplay) {
        return true;
    }
    return sessionType && std::strcmp(sessionType, "wayland") == 0;
}

// Convenience wrapper that reads the environment.
inline bool detectSyntheticRepeats() {
    return sessionSendsSyntheticRepeats(std::getenv("XDG_SESSION_TYPE"),
                                        std::getenv("WAYLAND_DISPLAY"));
}

} // namespace fcitx

#endif
