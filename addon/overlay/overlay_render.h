#ifndef SCHNELLE_UMLAUTE_OVERLAY_RENDER_H
#define SCHNELLE_UMLAUTE_OVERLAY_RENDER_H

// Renderer-level constants shared by the overlay daemon and the pure geometry
// headers next to it. Free of Qt/QML so it stays unit-testable and can be
// included from both sides without dragging in the layer-shell stack.

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

} // namespace render
} // namespace schnelle_umlaute

#endif
