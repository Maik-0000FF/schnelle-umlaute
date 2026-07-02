#ifndef SCHNELLE_UMLAUTE_OVERLAY_LIFECYCLE_H
#define SCHNELLE_UMLAUTE_OVERLAY_LIFECYCLE_H

// Pure decision logic for the overlay daemon lifecycle. Deliberately
// free of DBus/fcitx5/Qt dependencies so it can be unit-tested with
// nothing but libc++.

#include <optional>

namespace fcitx {

enum class OverlayLifecycleAction { None, Start, Quit };

// Given the overlay's Enabled flag in the previous and current config
// states, decide what the daemon lifecycle should do.
//
//   previous    current    → action
//   nullopt     true       → Start (eager start after fcitx5 boot so the
//                                   daemon is ready for the first cycling
//                                   event instead of racing with DBus
//                                   activation latency on the first Show())
//   nullopt     false      → None  (overlay disabled, nothing to do)
//   false       true       → Start (user enabled the overlay)
//   true        false      → Quit  (user disabled the overlay)
//   same        same       → None  (no transition)
inline OverlayLifecycleAction
decideOverlayLifecycleAction(std::optional<bool> previous, bool current) {
    if (!previous.has_value()) {
        return current ? OverlayLifecycleAction::Start
                       : OverlayLifecycleAction::None;
    }
    if (!*previous && current)
        return OverlayLifecycleAction::Start;
    if (*previous && !current)
        return OverlayLifecycleAction::Quit;
    return OverlayLifecycleAction::None;
}

// Decide whether an already-running overlay daemon must be quit because its wire
// protocol no longer matches ours (a stale in-place-upgrade leftover). Only
// meaningful when a daemon owns the bus name.
//
//   hasOwner=false                  → false (nobody running; normal DBus
//                                            activation starts the fresh binary
//                                            on the next call)
//   hasOwner, version query failed  → true  (a daemon predating the version
//                                            handshake replies with an error;
//                                            treat it as stale)
//   hasOwner, reported != expected  → true  (signatures changed since it
//                                            started; its calls are dropped)
//   hasOwner, reported == expected  → false (compatible; leave it running)
inline bool overlayDaemonIsStale(bool hasOwner, bool gotVersion,
                                 int reportedVersion, int expectedVersion) {
    if (!hasOwner)
        return false;
    if (!gotVersion)
        return true;
    return reportedVersion != expectedVersion;
}

} // namespace fcitx

#endif
