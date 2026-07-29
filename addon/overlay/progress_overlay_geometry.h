#ifndef SCHNELLE_UMLAUTE_PROGRESS_OVERLAY_GEOMETRY_H
#define SCHNELLE_UMLAUTE_PROGRESS_OVERLAY_GEOMETRY_H

// Pure geometry for the overlay timing progress bar. Single source of truth for
// the bar's pixel sizing and the grid placement that centres the panel (not the
// whole panel+bar surface) on a column and, for the Center row, on the
// output's vertical midline. Free of Qt/QML so it is unit-testable
// (tests/testprogressgeometry.cpp) and shared by the daemon's renderer (grid
// placement) and QML (bar sizing, via OverlayController Q_INVOKABLE wrappers),
// so writer and reader can't drift.

#include <algorithm>
#include <cmath>

#include "overlay_render.h"

namespace schnelle_umlaute {
namespace progress {

// Pixels per millisecond the bar length encodes, the screen fraction it is
// clamped to, and the floor length. The fallback screen width lives in
// overlay_render.h: the renderer's grid placement needs the same assumption, so
// it is defined once for both.
constexpr double kPxPerMs = 0.22;
constexpr double kScreenFraction = 0.6;
constexpr int kMinWidth = 80;

// Bar pixel length: the total gesture time (lead + window) scaled by kPxPerMs,
// clamped to [kMinWidth, screenWidth * kScreenFraction] so a long timeout can't
// run off-screen. screenWidth <= 0 (surface not yet on an output) falls back to
// render::kFallbackScreenWidth.
inline int barLength(int totalMs, int screenWidth) {
    const int sw =
        screenWidth > 0 ? screenWidth : render::kFallbackScreenWidth;
    const int maxWidth = static_cast<int>(std::lround(sw * kScreenFraction));
    const int raw = static_cast<int>(std::lround(totalMs * kPxPerMs));
    return std::clamp(raw, kMinWidth, std::max(kMinWidth, maxWidth));
}

// Lead-segment length, proportional to lead : total within the bar. A
// non-positive total yields 0 (no lead segment).
inline int leadLength(int barLen, int leadMs, int totalMs) {
    if (totalMs <= 0)
        return 0;
    return static_cast<int>(
        std::lround(static_cast<double>(barLen) * leadMs / totalMs));
}

// Left margin (from the output's left edge) that centres a frameWidth-wide panel
// on grid column `col` (0..6), while keeping the whole windowWidth-wide surface
// (panel plus the bar overhang to its right) on screen by `edgeMargin`. The
// column centre comes from render::columnCenter, the same one anchorsFor uses,
// but here the PANEL is centred rather than the surface, so the bar no longer
// shifts the panel.
inline int gridPanelLeftMargin(int col, int screenWidth, int frameWidth,
                               int windowWidth, int edgeMargin) {
    const int center = render::columnCenter(col, screenWidth);
    const int left = center - frameWidth / 2;
    const int maxLeft =
        std::max(edgeMargin, screenWidth - windowWidth - edgeMargin);
    return std::clamp(left, edgeMargin, maxLeft);
}

// Top margin (from the output's top edge, for a Top-anchored surface) that
// vertically centres a frameHeight-tall PANEL on the output. In progress mode
// the panel sits at the bottom of a taller windowHeight surface (the bar plus
// its gap overhang ABOVE the panel), so a plain compositor-centred surface
// would drop the panel by half that overhang. This anchors the panel's centred
// position instead, mirroring gridPanelLeftMargin on the vertical axis,
// clamped so the whole surface stays on screen by `edgeMargin`.
inline int gridPanelTopMargin(int screenHeight, int frameHeight,
                              int windowHeight, int edgeMargin) {
    // Bar plus its gap, sitting above the panel.
    const int overhang = windowHeight - frameHeight;
    const int top = (screenHeight - frameHeight) / 2 - overhang;
    const int maxTop =
        std::max(edgeMargin, screenHeight - windowHeight - edgeMargin);
    return std::clamp(top, edgeMargin, maxTop);
}

} // namespace progress
} // namespace schnelle_umlaute

#endif
