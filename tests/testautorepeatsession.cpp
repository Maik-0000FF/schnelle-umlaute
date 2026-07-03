// Unit tests for sessionSendsSyntheticRepeats().
// Pure header-only logic — verifies the rule that decides whether the
// pre-leader commit deferral (issue #73) is enabled: true only in sessions
// that deliver auto-repeat as synthetic release-press pairs (Wayland).

#include "src/autorepeat_session.h"

#include <cstdio>
#include <cstdlib>

using fcitx::sessionSendsSyntheticRepeats;

#define EXPECT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,       \
                         #cond);                                               \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

// XDG_SESSION_TYPE=wayland with no display variable still counts as Wayland.
void testSessionTypeWayland() {
    EXPECT(sessionSendsSyntheticRepeats("wayland", nullptr));
    EXPECT(sessionSendsSyntheticRepeats("wayland", ""));
}

// A present WAYLAND_DISPLAY is the most direct signal, independent of
// XDG_SESSION_TYPE (which is often unset under bare compositors).
void testWaylandDisplayPresent() {
    EXPECT(sessionSendsSyntheticRepeats(nullptr, "wayland-0"));
    EXPECT(sessionSendsSyntheticRepeats("", "wayland-1"));
}

// X11 session with no Wayland display: deferral disabled, historic behavior.
void testX11IsDisabled() {
    EXPECT(!sessionSendsSyntheticRepeats("x11", nullptr));
    EXPECT(!sessionSendsSyntheticRepeats("x11", ""));
}

// XWayland app inside a Wayland session (session type x11 but a Wayland
// display is present): treated as Wayland by design. Over-inclusive but
// harmless — the deferral just commits after the short timer with no
// re-press to bridge.
void testXWaylandInWaylandSession() {
    EXPECT(sessionSendsSyntheticRepeats("x11", "wayland-0"));
}

// No env at all (nullptr / empty): disabled. This is the CI/build-sandbox
// case, so the addon's own test suite runs the non-deferred path by default.
void testUnknownIsDisabled() {
    EXPECT(!sessionSendsSyntheticRepeats(nullptr, nullptr));
    EXPECT(!sessionSendsSyntheticRepeats("", ""));
}

// A non-wayland, non-x11 label (e.g. "tty") with no display is disabled.
void testTtyIsDisabled() {
    EXPECT(!sessionSendsSyntheticRepeats("tty", nullptr));
}

int main() {
    testSessionTypeWayland();
    testWaylandDisplayPresent();
    testX11IsDisabled();
    testXWaylandInWaylandSession();
    testUnknownIsDisabled();
    testTtyIsDisabled();
    std::fprintf(stderr, "testautorepeatsession: all tests passed\n");
    return 0;
}
