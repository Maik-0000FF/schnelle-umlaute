// Unit tests for decideRenderAction().
// Pure function, no Qt/QML/layer-shell runtime — verifies the table that drives
// the overlay daemon's renderer: when to hide, when to re-anchor, and above all
// when to do NOTHING, because a rebuild per cycling keystroke is the cost this
// table exists to avoid.

#include "overlay/overlay_render.h"

#include "test_expect.h"

#include <cstdio>

using schnelle_umlaute::render::decideRenderAction;
using schnelle_umlaute::render::isEpochCurrent;
using schnelle_umlaute::render::kFirstEpoch;
using schnelle_umlaute::render::nextEpoch;
using schnelle_umlaute::render::RenderAction;
using schnelle_umlaute::render::RenderEpoch;
using schnelle_umlaute::render::RenderRequest;
using schnelle_umlaute::render::RenderState;
using schnelle_umlaute::render::showSnapsTransitions;

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

// The cursor fetch is async, so its reply can land after the gesture that asked
// for it. Work that started in an epoch may only touch the window while that
// epoch is still the current one.
void testDeferredWorkOfTheLivePlacementRuns() {
    const RenderEpoch started = kFirstEpoch;
    EXPECT(isEpochCurrent(started, started));
}

// Hidden in the meantime: the reply carries a pointer for an overlay that is
// gone, so it must not re-show the window.
void testDeferredWorkAfterHideIsStale() {
    const RenderEpoch started = kFirstEpoch;
    const RenderEpoch afterHide = nextEpoch(started);
    EXPECT(!isEpochCurrent(started, afterHide));
}

// The case the destroyed window used to catch for free: gesture 1's reply lands
// while gesture 2 is already on screen. The window is alive and visible, so only
// the epoch tells them apart. Applying it would place gesture 2's overlay at
// gesture 1's position.
void testDeferredWorkOfSupersededPlacementIsStale() {
    const RenderEpoch gesture1 = kFirstEpoch;
    // A new placement hides first, then shows: two bumps.
    const RenderEpoch gesture2 = nextEpoch(nextEpoch(gesture1));
    EXPECT(!isEpochCurrent(gesture1, gesture2));
}

// Epochs only ever move forward, so a stale one can never come back around and
// be mistaken for the live placement.
void testEpochsAdvance() {
    RenderEpoch e = kFirstEpoch;
    for (int i = 0; i < 100; ++i) {
        const RenderEpoch previous = e;
        e = nextEpoch(e);
        EXPECT(e > previous);
        EXPECT(!isEpochCurrent(previous, e));
    }
}

// Cycling: the overlay is up, the same variants come back with the highlight on
// another cell. That handover is what the animation is for.
void testCyclingAnimates() {
    EXPECT(!showSnapsTransitions(/*wasVisible=*/true, /*variantsChanged=*/false));
}

// A new gesture while the previous overlay is still on screen (the engine hides
// with a commit flash, so this is reachable). Its cells still hold the old
// gesture's colours, so animating from them is the flash.
void testNewVariantsWhileVisibleSnap() {
    EXPECT(showSnapsTransitions(/*wasVisible=*/true, /*variantsChanged=*/true));
}

// Re-triggering the SAME key after a commit: the variants are identical, so the
// content alone cannot tell this from cycling. Coming from hidden is what does.
// This is the reported bug: the previously active cell faded out green.
void testSameVariantsFromHiddenSnap() {
    EXPECT(showSnapsTransitions(/*wasVisible=*/false, /*variantsChanged=*/false));
}

void testNewVariantsFromHiddenSnap() {
    EXPECT(showSnapsTransitions(/*wasVisible=*/false, /*variantsChanged=*/true));
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
    testDeferredWorkOfTheLivePlacementRuns();
    testDeferredWorkAfterHideIsStale();
    testDeferredWorkOfSupersededPlacementIsStale();
    testEpochsAdvance();
    testCyclingAnimates();
    testNewVariantsWhileVisibleSnap();
    testSameVariantsFromHiddenSnap();
    testNewVariantsFromHiddenSnap();
    std::printf("testoverlayrender: all passed\n");
    return 0;
}
