// Unit tests for decideOverlayLifecycleAction().
// Pure function, no DBus runtime — verifies the daemon-lifecycle decision
// table that drives OverlayClient::applyEnabledTransition().

#include "src/overlay_lifecycle.h"

#include <cstdio>
#include <cstdlib>
#include <optional>

using fcitx::decideOverlayLifecycleAction;
using fcitx::overlayDaemonIsStale;
using fcitx::OverlayLifecycleAction;

#define EXPECT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,       \
                         #cond);                                               \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

// First call after fcitx5 startup: eager-start when enabled so the
// daemon is ready for the first cycling event. When disabled there's
// nothing to do.
void testFirstCallIsNoOpWhenDisabled() {
    auto action = decideOverlayLifecycleAction(std::nullopt, false);
    EXPECT(action == OverlayLifecycleAction::None);
}

void testFirstCallStartsWhenEnabled() {
    auto action = decideOverlayLifecycleAction(std::nullopt, true);
    EXPECT(action == OverlayLifecycleAction::Start);
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

// -- overlayDaemonIsStale(): the protocol-version restart decision -----------

// Nobody running: nothing to restart, DBus activation brings up the fresh
// binary on the next call. The version args are irrelevant here.
void testNoOwnerIsNeverStale() {
    EXPECT(!overlayDaemonIsStale(/*hasOwner=*/false, /*gotVersion=*/false,
                                 /*reported=*/-1, /*expected=*/1));
    EXPECT(!overlayDaemonIsStale(false, true, 1, 1));
}

// A running daemon that predates the handshake replies with an error, so the
// version query fails: treat it as stale and restart it.
void testOwnerWithoutVersionIsStale() {
    EXPECT(overlayDaemonIsStale(/*hasOwner=*/true, /*gotVersion=*/false,
                                /*reported=*/-1, /*expected=*/1));
}

// A running daemon reporting a different protocol version is a stale in-place
// upgrade leftover whose calls would be dropped.
void testOwnerVersionMismatchIsStale() {
    EXPECT(overlayDaemonIsStale(true, true, /*reported=*/1, /*expected=*/2));
    EXPECT(overlayDaemonIsStale(true, true, /*reported=*/3, /*expected=*/2));
}

// A running daemon on the same protocol version is fine: leave it alone.
void testOwnerVersionMatchIsNotStale() {
    EXPECT(!overlayDaemonIsStale(true, true, /*reported=*/2, /*expected=*/2));
}

int main() {
    testFirstCallIsNoOpWhenDisabled();
    testFirstCallStartsWhenEnabled();
    testDisabledToEnabledStarts();
    testEnabledToDisabledQuits();
    testEnabledStaysEnabledNoOp();
    testDisabledStaysDisabledNoOp();
    testNoOwnerIsNeverStale();
    testOwnerWithoutVersionIsStale();
    testOwnerVersionMismatchIsStale();
    testOwnerVersionMatchIsNotStale();
    std::fprintf(stderr, "testoverlaylifecycle: all tests passed\n");
    return 0;
}
