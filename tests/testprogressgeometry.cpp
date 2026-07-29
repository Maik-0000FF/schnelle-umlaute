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

    // ── gridPanelTopMargin: centre the PANEL on the Center row ───────────
    {
        const int edge = schnelle_umlaute::render::kEdgeMargin; // 24
        const int screen = 1080, frame = 64;
        // Overhang above the panel: progressBarHeight (6) + progressBarGap (8)
        // from Overlay.qml.
        const int overhang = 14;
        const int window = frame + overhang; // 78

        // The panel sits `overhang` below the surface top, so its own centre
        // must land back on the screen centre: (1080 - 64) / 2 - 14 = 494.
        const int top = pg::gridPanelTopMargin(screen, frame, window, edge);
        EXPECT(top == 494);
        EXPECT(top + overhang + frame / 2 == screen / 2);

        // Without a bar (window == frame) it is the plain centred panel top,
        // so the non-progress path is unchanged.
        EXPECT(pg::gridPanelTopMargin(screen, frame, frame, edge)
               == (screen - frame) / 2);

        // A surface taller than the output would want a negative top -> clamp
        // to edgeMargin. (The maxTop clamp mirrors gridPanelLeftMargin but
        // cannot bind here: a taller surface means a larger overhang, which
        // only lowers the top, so it never pushes the bottom off-screen.)
        EXPECT(pg::gridPanelTopMargin(100, frame, 200, edge) == edge);
    }

    return 0;
}
