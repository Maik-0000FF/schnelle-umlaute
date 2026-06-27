// Unit test for the pure cursor-overlay geometry helpers
// (addon/overlay/cursor_overlay_geometry.h): the "Cursor:" wire-format split
// and the lower-left-corner-at-cursor margin math with edge clamping. No Qt,
// DBus, or layer-shell runtime needed.

#include "overlay/cursor_overlay_geometry.h"

#include "test_expect.h"

using schnelle_umlaute::cursorMargins;
using schnelle_umlaute::parseCursorPosition;

int main() {
    // ── parseCursorPosition ──────────────────────────────────────────────
    {
        const auto plain = parseCursorPosition("TopCol4");
        EXPECT(!plain.atCursor);
        EXPECT(plain.grid == "TopCol4");

        const auto cursor = parseCursorPosition("Cursor:TopCol4");
        EXPECT(cursor.atCursor);
        EXPECT(cursor.grid == "TopCol4");

        // Prefix only → cursor mode with an empty grid fallback.
        const auto bare = parseCursorPosition("Cursor:");
        EXPECT(bare.atCursor);
        EXPECT(bare.grid.empty());

        // "Cursor" without the colon is a plain (if unusual) grid string, not
        // the cursor marker — the prefix must match exactly.
        const auto noColon = parseCursorPosition("Cursor");
        EXPECT(!noColon.atCursor);
        EXPECT(noColon.grid == "Cursor");

        // A different column survives the split unchanged.
        const auto other = parseCursorPosition("Cursor:BottomCol7");
        EXPECT(other.atCursor);
        EXPECT(other.grid == "BottomCol7");
    }

    // ── cursorMargins: lower-left corner at the cursor ───────────────────
    {
        // Comfortably inside a 1920×1080 screen at the origin. The overlay's
        // left edge lands on the cursor x; its bottom edge (top + height) on
        // the cursor y.
        const int ow = 200, oh = 64;
        const auto m = cursorMargins(500, 500, 0, 0, 1920, 1080, ow, oh);
        EXPECT(m.left == 500);
        EXPECT(m.top == 500 - oh); // 436 → bottom edge at 500
        EXPECT(m.top + oh == 500);
    }

    // ── cursorMargins: clamp at the right edge ───────────────────────────
    {
        const int ow = 200, oh = 64;
        // Cursor near the right edge: left would be 1900 but the surface is
        // 200 wide, so it clamps to screenW - ow = 1720. The overlay extends
        // upward from the cursor, so the bottom never needs clamping for an
        // on-screen cursor — top is just cursorY - oh.
        const auto m = cursorMargins(1900, 1060, 0, 0, 1920, 1080, ow, oh);
        EXPECT(m.left == 1920 - ow); // 1720
        EXPECT(m.top == 1060 - oh);  // 996
    }

    // ── cursorMargins: clamp at the top edge ─────────────────────────────
    {
        const int ow = 200, oh = 64;
        // Cursor 10 px from the top: top = 10 - 64 = -54 → clamp to 0 so the
        // surface stays on-screen (corner drifts off the cursor).
        const auto m = cursorMargins(300, 10, 0, 0, 1920, 1080, ow, oh);
        EXPECT(m.left == 300);
        EXPECT(m.top == 0);
    }

    // ── cursorMargins: second monitor (non-zero screen origin) ───────────
    {
        const int ow = 200, oh = 64;
        // The cursor's output starts at x=1920; margins are output-local, so a
        // global cursor at (2000, 300) maps to left=80, top=236.
        const auto m = cursorMargins(2000, 300, 1920, 0, 1920, 1080, ow, oh);
        EXPECT(m.left == 80);
        EXPECT(m.top == 300 - oh); // 236
    }

    return 0;
}
