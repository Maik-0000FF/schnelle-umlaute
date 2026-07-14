#ifndef SCHNELLE_UMLAUTE_OVERLAY_RENDER_H
#define SCHNELLE_UMLAUTE_OVERLAY_RENDER_H

// Renderer-level constants and the daemon's show/hide decision table, shared
// with the pure geometry headers next to it. Free of Qt/QML so it stays
// unit-testable (tests/testoverlayrender.cpp) and can be included from both
// sides without dragging in the layer-shell stack.

#include <string>

namespace schnelle_umlaute {
namespace render {

// Assumed output width before the surface is bound to one, i.e. when no QScreen
// is known yet. Only ever used to keep the placement math on a sane scale until
// the real geometry arrives.
constexpr int kFallbackScreenWidth = 1920;

// Assumed overlay size before the QML window has been laid out (width()/height()
// still report 0). The anchor margins for the fractional columns and the
// at-cursor placement are derived from the overlay's own size, so a plausible
// stand-in beats a zero that would pin the surface to the wrong spot.
constexpr int kFallbackOverlayWidth = 200;
constexpr int kFallbackOverlayHeight = 64;

enum class RenderAction {
    // Leave the window as it is: the content changed (variants, current index,
    // theme, progress), and the QML bindings render that on the surface that is
    // already up. This is the cycling case, and the reason the daemon must not
    // rebuild anything per keystroke.
    None,
    // Hide the window. The engine and its QML stay alive.
    Hide,
    // Anchor for `position` and show. Qt destroys the wl_surface when a window
    // is hidden and builds a fresh layer surface when it is shown again, so the
    // anchors of a new position always take effect.
    Show,
};

// What the controller is asking for.
struct RenderRequest {
    bool visible;
    bool hasVariants;
    std::string position;
    // Label mode renders one full-width name instead of glyph cells, so it has
    // a very different width and must not reuse a grid-mode surface.
    bool label;
};

// What the renderer has already committed to. `active` is true from the moment
// the renderer decides to show at (position, label) until the window is hidden
// again, INCLUDING the window in which an async cursor query is still in flight.
struct RenderState {
    bool active;
    std::string position;
    bool label;
};

// The renderer's one decision. Nothing to show means hide; an already-active
// window at the same position and mode needs no work at all (its bindings do it);
// anything else needs a re-anchored surface.
inline RenderAction decideRenderAction(const RenderRequest &req,
                                       const RenderState &state) {
    if (!req.visible || !req.hasVariants)
        return RenderAction::Hide;
    if (state.active && state.position == req.position &&
        state.label == req.label)
        return RenderAction::None;
    return RenderAction::Show;
}

// The engine highlights no cell while a gesture's accent window is still open:
// it sends a negative index until a leader press starts the cycling. Cycling and
// the post-commit flash always name a real cell. A negative index is therefore
// the engine saying "a gesture just opened", and it is the ONLY thing that can
// say so: the flash leaves the committed variants on screen for 150 ms, so
// pressing that same key again inside that window arrives with the same variant
// list, on a still-visible overlay. Nothing about the content distinguishes it
// from cycling.
inline bool opensGesture(int currentIndex) { return currentIndex < 0; }

// Does this Show have to snap the QML's transitions, or may it animate them?
//
// The engine outlives a gesture, so its properties still hold the last one's
// values: the cell that was active is green, the panel is faded in. A Show that
// starts a NEW gesture must snap, or those values animate to the new ones on a
// surface that is already on screen, which is the flash the user sees. A Show
// that merely moves the highlight within the gesture on screen is the handover
// the animation exists for.
inline bool showSnapsTransitions(bool wasVisible, bool variantsChanged,
                                 int currentIndex) {
    return !wasVisible || variantsChanged || opensGesture(currentIndex);
}

// Placement epoch. Cursor mode fetches the pointer asynchronously, so a reply
// can land after the gesture that asked for it is over, carrying that gesture's
// position and cursor. It must not be applied to whatever is on screen by then.
//
// The window used to be destroyed on hide, so a stale reply found a dangling
// QPointer and gave up on its own. With the window alive across gestures that
// accidental guard is gone, and "is the overlay visible?" is no answer either:
// it is visible again as soon as the NEXT gesture opens. So the placement gets
// an explicit epoch: bumped on every hide and every show, captured by value in
// the deferred work, and compared before that work touches the window.
using RenderEpoch = unsigned long long;
constexpr RenderEpoch kFirstEpoch = 1;

inline RenderEpoch nextEpoch(RenderEpoch epoch) { return epoch + 1; }

// True when deferred work still belongs to the placement that started it.
inline bool isEpochCurrent(RenderEpoch captured, RenderEpoch current) {
    return captured == current;
}

} // namespace render
} // namespace schnelle_umlaute

#endif
