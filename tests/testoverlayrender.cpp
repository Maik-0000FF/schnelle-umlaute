// Unit tests for decideRenderAction().
// Pure function, no Qt/QML/layer-shell runtime — verifies the table that drives
// the overlay daemon's renderer: when to hide, when to re-anchor, and above all
// when to do NOTHING, because a rebuild per cycling keystroke is the cost this
// table exists to avoid.

#include "overlay/overlay_render.h"

#include "test_expect.h"

#include <cstdio>

using schnelle_umlaute::render::decideRenderAction;
using schnelle_umlaute::render::RenderAction;
using schnelle_umlaute::render::RenderRequest;
using schnelle_umlaute::render::RenderState;

namespace {
RenderRequest shown(const char *pos, bool label = false) {
    return RenderRequest{true, true, pos, label};
}
RenderState idle() { return RenderState{false, "", false}; }
RenderState activeAt(const char *pos, bool label = false) {
    return RenderState{true, pos, label};
}
} // namespace

// Nothing to show: the controller hid the overlay, or a gesture ended and left
// no variants behind.
void testHiddenControllerHides() {
    EXPECT(decideRenderAction(RenderRequest{false, true, "TopCol4", false},
                              activeAt("TopCol4")) == RenderAction::Hide);
}

void testEmptyVariantsHide() {
    EXPECT(decideRenderAction(RenderRequest{true, false, "TopCol4", false},
                              activeAt("TopCol4")) == RenderAction::Hide);
}

// Hiding an already-hidden window is still Hide (a no-op at the call site), not
// an accidental Show.
void testHiddenWhileIdleStaysHidden() {
    EXPECT(decideRenderAction(RenderRequest{false, false, "TopCol4", false},
                              idle()) == RenderAction::Hide);
}

// First open: nothing is up yet, so anchor and show.
void testFirstOpenShows() {
    EXPECT(decideRenderAction(shown("TopCol4"), idle()) == RenderAction::Show);
}

// THE hot path: cycling through variants at the same position. The surface is
// up and the QML bindings render the new variant, so the renderer must not
// touch the window. Anything but None here means a rebuild per keystroke.
void testSamePositionWhileActiveIsNoOp() {
    EXPECT(decideRenderAction(shown("TopCol4"), activeAt("TopCol4")) ==
           RenderAction::None);
}

// Re-showing at the position we last used, but from a hidden window (a new
// gesture after the previous one committed): the surface is gone, so it must be
// shown again. This is the case a naive "same position, do nothing" check gets
// wrong, leaving the overlay invisible for the rest of the session.
void testSamePositionWhileInactiveShows() {
    EXPECT(decideRenderAction(shown("TopCol4"),
                              RenderState{false, "TopCol4", false}) ==
           RenderAction::Show);
}

// A different position needs a fresh surface: layer-shell anchors are fixed at
// the first commit.
void testPositionChangeShows() {
    EXPECT(decideRenderAction(shown("BottomCol7"), activeAt("TopCol4")) ==
           RenderAction::Show);
}

// Cursor mode carries its own position string, so switching to or from it is a
// position change like any other.
void testCursorModeChangeShows() {
    EXPECT(decideRenderAction(shown("Cursor:TopCol4"), activeAt("TopCol4")) ==
           RenderAction::Show);
}

// Label mode (a profile name) is far wider than the glyph cells, and the anchor
// margin is baked from the width at surface-build time, so a mode switch must
// re-anchor even though the position string is unchanged.
void testLabelModeChangeShows() {
    EXPECT(decideRenderAction(shown("TopCol4", true), activeAt("TopCol4", false)) ==
           RenderAction::Show);
    EXPECT(decideRenderAction(shown("TopCol4", false), activeAt("TopCol4", true)) ==
           RenderAction::Show);
}

int main() {
    testHiddenControllerHides();
    testEmptyVariantsHide();
    testHiddenWhileIdleStaysHidden();
    testFirstOpenShows();
    testSamePositionWhileActiveIsNoOp();
    testSamePositionWhileInactiveShows();
    testPositionChangeShows();
    testCursorModeChangeShows();
    testLabelModeChangeShows();
    std::printf("testoverlayrender: all passed\n");
    return 0;
}
