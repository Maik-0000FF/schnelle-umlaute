// Unit test for the pure progress-bar geometry helpers
// (addon/overlay/progress_overlay_geometry.h): bar length clamp/scale, the
// lead/window split, and the grid placement that centres the panel (not the
// panel+bar surface) on a column. No Qt or QML runtime needed.

#include "overlay/progress_overlay_geometry.h"

#include "test_expect.h"

namespace pg = schnelle_umlaute::progress;

int main() {
    // ── barLength: scale + clamp ─────────────────────────────────────────
    {
        const int screen = 1920;
        const int maxWidth = static_cast<int>(screen * pg::kScreenFraction); // 1152

        // Normal: 1250 ms * 0.22 = 275, within [min, max].
        EXPECT(pg::barLength(1250, screen) == 275);
        // Floor: a tiny total clamps up to kMinWidth.
        EXPECT(pg::barLength(10, screen) == pg::kMinWidth);
        // Ceiling: a long timeout clamps down to the screen fraction.
        EXPECT(pg::barLength(100000, screen) == maxWidth);
        // screenWidth <= 0 falls back to render::kFallbackScreenWidth (1920
        // here), so the result matches the explicit-1920 call.
        EXPECT(pg::barLength(1250, 0)
               == pg::barLength(1250, schnelle_umlaute::render::kFallbackScreenWidth));
    }

    // ── leadLength: proportional split ───────────────────────────────────
    {
        // 500 : 1250 of a 275 px bar -> 110.
        EXPECT(pg::leadLength(275, 500, 1250) == 110);
        // No lead -> no lead segment.
        EXPECT(pg::leadLength(275, 0, 1250) == 0);
        // Whole bar is lead when lead == total.
        EXPECT(pg::leadLength(275, 1250, 1250) == 275);
        // Degenerate total -> 0, no division by zero.
        EXPECT(pg::leadLength(275, 0, 0) == 0);
    }

    // ── gridPanelLeftMargin: centre the PANEL on the column ──────────────
    {
        const int screen = 1920, frame = 200, window = 400;
        const int edge = schnelle_umlaute::render::kEdgeMargin; // 24
        // Centre column (col 3): centre = 960, panel left = 960 - 100 = 860,
        // and the panel's own centre lands back exactly on 960.
        const int leftCol4 = pg::gridPanelLeftMargin(3, screen, frame, window, edge);
        EXPECT(leftCol4 == 860);
        EXPECT(leftCol4 + frame / 2 == 960);

        // Left column (col 0): centre = 240, panel left = 140.
        EXPECT(pg::gridPanelLeftMargin(0, screen, frame, window, edge) == 140);

        // Left column with a wide panel would push past the left edge -> clamp
        // to edgeMargin.
        EXPECT(pg::gridPanelLeftMargin(0, screen, 600, 800, edge) == edge);

        // Right column (col 6): centre = 1680, panel left = 1580, but the whole
        // window must stay on screen -> clamp to screen - window - edge = 1496.
        EXPECT(pg::gridPanelLeftMargin(6, screen, frame, window, edge) == 1496);
    }

    return 0;
}
