#ifndef SCHNELLE_UMLAUTE_PROGRESS_OVERLAY_GEOMETRY_H
#define SCHNELLE_UMLAUTE_PROGRESS_OVERLAY_GEOMETRY_H

// Pure geometry for the overlay timing progress bar. Single source of truth for
// the bar's pixel sizing and the grid placement that centres the panel (not the
// whole panel+bar surface) on a column. Free of Qt/QML so it is unit-testable
// (tests/testprogressgeometry.cpp) and shared by the daemon's renderer (grid
// placement) and QML (bar sizing, via OverlayController Q_INVOKABLE wrappers),
// so writer and reader can't drift.

#include <algorithm>
#include <cmath>

namespace schnelle_umlaute {
namespace progress {

// Pixels per millisecond the bar length encodes, the screen fraction it is
// clamped to, the floor length, and the fallback screen width before the
// surface is bound to an output.
constexpr double kPxPerMs = 0.22;
constexpr double kScreenFraction = 0.6;
constexpr int kMinWidth = 80;
constexpr int kFallbackScreenWidth = 1920;

// Bar pixel length: the total gesture time (lead + window) scaled by kPxPerMs,
// clamped to [kMinWidth, screenWidth * kScreenFraction] so a long timeout can't
// run off-screen. screenWidth <= 0 (surface not yet on an output) falls back to
// kFallbackScreenWidth.
inline int barLength(int totalMs, int screenWidth) {
    const int sw = screenWidth > 0 ? screenWidth : kFallbackScreenWidth;
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
// column centres mirror anchorsFor's screenWidth*(col+1)/8, but here the PANEL
// is centred rather than the surface, so the bar no longer shifts the panel.
inline int gridPanelLeftMargin(int col, int screenWidth, int frameWidth,
                               int windowWidth, int edgeMargin) {
    const int center = screenWidth * (col + 1) / 8;
    const int left = center - frameWidth / 2;
    const int maxLeft =
        std::max(edgeMargin, screenWidth - windowWidth - edgeMargin);
    return std::clamp(left, edgeMargin, maxLeft);
}

} // namespace progress
} // namespace schnelle_umlaute

#endif
