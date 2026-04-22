// Unit tests for decideOverlayLifecycleAction().
// Pure function, no DBus runtime — verifies the daemon-lifecycle decision
// table that drives OverlayClient::applyEnabledTransition().

#include "src/overlay_lifecycle.h"

#include <cstdio>
#include <cstdlib>
#include <optional>

using fcitx::decideOverlayLifecycleAction;
using fcitx::OverlayLifecycleAction;

#define EXPECT(cond) do {                                                   \
    if (!(cond)) {                                                          \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);\
        std::abort();                                                       \
    }                                                                       \
} while (0)

// First call after fcitx5 startup: we don't know the previous state, so
// we never emit start/quit. The daemon remains lazy (DBus activation on
// the first Show()) if enabled, and stays absent if disabled.
void testFirstCallIsNoOpWhenDisabled() {
    auto action = decideOverlayLifecycleAction(std::nullopt, false);
    EXPECT(action == OverlayLifecycleAction::None);
}

void testFirstCallIsNoOpWhenEnabled() {
    auto action = decideOverlayLifecycleAction(std::nullopt, true);
    EXPECT(action == OverlayLifecycleAction::None);
}

// User toggles the overlay on in the editor: previous=false, current=true.
void testDisabledToEnabledStarts() {
    auto action = decideOverlayLifecycleAction(false, true);
    EXPECT(action == OverlayLifecycleAction::Start);
}

// User toggles the overlay off: previous=true, current=false.
void testEnabledToDisabledQuits() {
    auto action = decideOverlayLifecycleAction(true, false);
    EXPECT(action == OverlayLifecycleAction::Quit);
}

// Config reload without a change (e.g. editing mappings) must not churn
// the daemon — a lot of user actions trigger reloadConfig.
void testEnabledStaysEnabledNoOp() {
    auto action = decideOverlayLifecycleAction(true, true);
    EXPECT(action == OverlayLifecycleAction::None);
}

void testDisabledStaysDisabledNoOp() {
    auto action = decideOverlayLifecycleAction(false, false);
    EXPECT(action == OverlayLifecycleAction::None);
}

int main() {
    testFirstCallIsNoOpWhenDisabled();
    testFirstCallIsNoOpWhenEnabled();
    testDisabledToEnabledStarts();
    testEnabledToDisabledQuits();
    testEnabledStaysEnabledNoOp();
    testDisabledStaysDisabledNoOp();
    std::fprintf(stderr, "testoverlaylifecycle: all tests passed\n");
    return 0;
}
