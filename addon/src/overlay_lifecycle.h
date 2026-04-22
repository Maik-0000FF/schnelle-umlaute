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
//   nullopt     any        → None  (first call after boot — lazy start)
//   false       true       → Start (user enabled the overlay)
//   true        false      → Quit  (user disabled the overlay)
//   same        same       → None  (no transition)
inline OverlayLifecycleAction
decideOverlayLifecycleAction(std::optional<bool> previous, bool current) {
    if (!previous.has_value()) return OverlayLifecycleAction::None;
    if (!*previous && current) return OverlayLifecycleAction::Start;
    if (*previous && !current) return OverlayLifecycleAction::Quit;
    return OverlayLifecycleAction::None;
}

} // namespace fcitx

#endif
