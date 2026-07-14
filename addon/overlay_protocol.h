#ifndef SCHNELLE_UMLAUTE_OVERLAY_PROTOCOL_H
#define SCHNELLE_UMLAUTE_OVERLAY_PROTOCOL_H

// Wire-protocol version for the de.schnelle_umlaute.Overlay1 D-Bus interface,
// the single source both the engine client and the overlay daemon consume.
//
// After an in-place package upgrade the previously running overlay daemon keeps
// owning the bus name (D-Bus activation only starts one when the name is
// unowned), so a fresh engine can end up talking to an old daemon. If a method
// signature changed in between, the old daemon rejects the new call and, since
// the engine sends fire-and-forget, the overlay silently goes dark. Exposing
// this version over D-Bus lets the engine detect that mismatch and restart the
// stale daemon.
//
// Bump on EVERY change to a method signature on that interface (a new, removed,
// reordered or retyped argument). Kept free of fcitx5/Qt dependencies so both
// build targets can include it directly.
namespace schnelle_umlaute {
// 2 (1.5.0): SendCursor gained a request id, so its signature changed from
// (x, y) to (requestId, x, y). The bump is what makes the engine notice a daemon
// still running from the previous install and restart it; without it the user
// keeps the old daemon (and none of this cycle's overlay work) until logout.
constexpr int kOverlayProtocolVersion = 2;

// The currentIndex a Show carries when NO cell is highlighted. The engine sends
// it while a gesture's accent window is still open (its preview shows the
// variants without a choice yet) and for a standalone profile-name pill; cycling
// and the post-commit flash always name a real cell.
//
// The daemon reads it as "a gesture just opened", which is the only thing that
// can tell a re-triggered key apart from a cycling step: the commit flash leaves
// the same variants on a still-visible overlay for a moment, so neither
// visibility nor content distinguishes the two (see render::opensGesture).
// That makes it a cross-process contract, so it lives here, next to the version
// it travels with, rather than once on each side.
constexpr int kNoHighlightIndex = -1;
}

#endif
