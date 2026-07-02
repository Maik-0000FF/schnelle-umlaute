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
constexpr int kOverlayProtocolVersion = 1;
}

#endif
