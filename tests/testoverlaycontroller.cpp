// Unit tests for OverlayController.
// Verifies the show/hide/quit state machine that the DBus adapter drives
// on the standalone overlay daemon — independent of LayerShellQt or QML.

#include "overlay/OverlayController.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QStringList>
#include <QTimer>

#include <cstdio>
#include <cstdlib>
#include <ctime>

#define EXPECT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,       \
                         #cond);                                               \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

// show() with variants populates every field and flips visible to true.
void testShowPopulatesStateAndEmits() {
    OverlayController ctrl;
    QSignalSpy spy(&ctrl, &OverlayController::stateChanged);

    ctrl.show({"ä", "Ä"}, 1, QStringLiteral("BottomRight"));

    EXPECT(ctrl.visible());
    EXPECT(ctrl.currentIndex() == 1);
    EXPECT(ctrl.position() == QStringLiteral("BottomRight"));
    EXPECT(ctrl.variants().size() == 2);
    EXPECT(ctrl.variants().at(0) == QStringLiteral("ä"));
    EXPECT(spy.count() == 1);
}

// show() with an empty variants list must not display the overlay — the
// addon calls hide() via an empty show() payload in some paths and the
// QML engine teardown relies on visible() being false for that case.
void testShowEmptyVariantsHides() {
    OverlayController ctrl;
    ctrl.show({"x"}, 0, QStringLiteral("TopCenter"));
    EXPECT(ctrl.visible());

    ctrl.show({}, 0, QStringLiteral("TopCenter"));
    EXPECT(!ctrl.visible());
    EXPECT(ctrl.variants().isEmpty());
}

// hide() clears visibility but keeps the last-known position so the
// renderer doesn't snap back to the default anchor between cycles.
void testHideClearsVisibleKeepsPosition() {
    OverlayController ctrl;
    ctrl.show({"ö", "Ö"}, 0, QStringLiteral("TopLeft"));
    EXPECT(ctrl.visible());
    QSignalSpy spy(&ctrl, &OverlayController::stateChanged);

    ctrl.hide();

    EXPECT(!ctrl.visible());
    EXPECT(ctrl.position() == QStringLiteral("TopLeft"));
    EXPECT(spy.count() == 1);
}

// An empty position string on show() must not clobber the previously
// configured position. Guards against the addon (theoretically) sending
// "" when OverlayConfig is mid-reload.
void testShowWithEmptyPositionPreservesPrevious() {
    OverlayController ctrl;
    ctrl.show({"ü"}, 0, QStringLiteral("Center"));
    EXPECT(ctrl.position() == QStringLiteral("Center"));

    ctrl.show({"ü"}, 0, QString());
    EXPECT(ctrl.position() == QStringLiteral("Center"));
}

// label defaults off for accent cycling and turns on for a profile-name flash,
// so the renderer knows to draw one full-width label instead of glyph cells.
void testLabelModeFlag() {
    OverlayController ctrl;
    ctrl.show({"ä", "Ä"}, 0, QStringLiteral("TopCenter"));
    EXPECT(!ctrl.label());

    ctrl.show({"Mathematik"}, -1, QStringLiteral("TopCenter"), true);
    EXPECT(ctrl.label());
    EXPECT(ctrl.variants().at(0) == QStringLiteral("Mathematik"));

    // Falls back to cell mode on the next accent show.
    ctrl.show({"ö"}, 0, QStringLiteral("TopCenter"));
    EXPECT(!ctrl.label());
}

// The DBus adaptor's Show slot forwards every arg, including the new label
// bool, to the controller. Pins the receiver-side signature the engine
// marshals against (asisb); exercised directly, without a bus.
void testAdaptorShowForwardsLabel() {
    OverlayController ctrl;
    OverlayDBusAdaptor adaptor(&ctrl);

    adaptor.Show({"Mathematik"}, -1, QStringLiteral("TopCenter"), true);
    EXPECT(ctrl.visible());
    EXPECT(ctrl.label());
    EXPECT(ctrl.variants().at(0) == QStringLiteral("Mathematik"));

    adaptor.Show({"ä", "Ä"}, 0, QStringLiteral("TopCenter"), false);
    EXPECT(!ctrl.label());
}

// quit() schedules a QCoreApplication::quit via QueuedConnection. Verify
// that running exec() after calling quit() returns cleanly — a missing
// or broken schedule would hang the event loop (caught by the timeout).
void testQuitSchedulesAppExit() {
    OverlayController ctrl;
    ctrl.quit();

    bool timedOut = false;
    QTimer safety;
    safety.setSingleShot(true);
    QObject::connect(&safety, &QTimer::timeout, [&]() {
        timedOut = true;
        QCoreApplication::exit(1);
    });
    safety.start(1000);

    int rc = QCoreApplication::exec();
    EXPECT(!timedOut);
    EXPECT(rc == 0);
}

// setProgress stores lead/window and computes the elapsed offset against the
// engine's start on the shared monotonic clock, clamped to [0, total], so the
// daemon can pre-advance the bar past D-Bus delivery latency.
void testSetProgressElapsedCompensation() {
    auto nowUsec = []() {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<qint64>(ts.tv_sec) * 1'000'000 + ts.tv_nsec / 1'000;
    };

    OverlayController ctrl;

    // A start 100 ms ago yields ~100 ms elapsed (the daemon reads the clock
    // just after, so it is >= 100 plus only microseconds of call overhead).
    ctrl.setProgress(50, 350, nowUsec() - 100'000);
    EXPECT(ctrl.progressLeadMs() == 50);
    EXPECT(ctrl.progressWindowMs() == 350);
    EXPECT(ctrl.progressActive());
    EXPECT(ctrl.progressElapsedMs() >= 100 && ctrl.progressElapsedMs() <= 150);

    // Elapsed beyond the total clamps to total (lead + window).
    ctrl.setProgress(50, 350, nowUsec() - 10'000'000);
    EXPECT(ctrl.progressElapsedMs() == 400);

    // A future start (or a clock that ran backwards) clamps to 0.
    ctrl.setProgress(50, 350, nowUsec() + 1'000'000);
    EXPECT(ctrl.progressElapsedMs() == 0);

    // startUsec <= 0 disables the compensation entirely.
    ctrl.setProgress(50, 350, 0);
    EXPECT(ctrl.progressElapsedMs() == 0);
}

// A profile-name (label) show clears a running or frozen progress bar so the
// name pill can't render over a leftover bar (hold a mapped key, then trigger a
// profile switch before releasing).
void testLabelShowClearsProgressBar() {
    OverlayController ctrl;
    ctrl.setProgress(100, 200, 0);
    EXPECT(ctrl.progressActive());
    ctrl.show({"French"}, 0, QStringLiteral("BottomCenter"), true);
    EXPECT(!ctrl.progressActive());
    EXPECT(!ctrl.progressFrozen());
}

// A plain accent show (label off) leaves the bar alone, so the frozen cycling
// bar keeps showing while the user steps through variants.
void testPlainShowKeepsProgressBar() {
    OverlayController ctrl;
    ctrl.setProgress(100, 200, 0);
    ctrl.freezeProgress();
    EXPECT(ctrl.progressActive());
    EXPECT(ctrl.progressFrozen());
    ctrl.show({"ä", "Ä"}, 0, QStringLiteral("BottomCenter"), false);
    EXPECT(ctrl.progressActive());
    EXPECT(ctrl.progressFrozen());
}

// ── Transition gate (the anti-flash sequencing) ───────────────────────────────
//
// The daemon keeps its QML engine across gestures, so its properties still hold
// the last gesture's values when the next one opens. These verify WHEN the gate
// closes, which is what the pure predicate alone cannot show: it has to happen
// on the call that overwrites the values, before it writes them.

// Cycling: the overlay is up and the highlight moves within the same variants.
// That handover is the one thing that must keep its animation.
void testCyclingKeepsTransitions() {
    OverlayController ctrl;
    ctrl.show({"ä", "à", "á"}, 0, QStringLiteral("TopCol4"));
    ctrl.setAnimate(true); // as the renderer does once the first frame is drawn

    ctrl.show({"ä", "à", "á"}, 1, QStringLiteral("TopCol4"));
    EXPECT(ctrl.animate());
    ctrl.show({"ä", "à", "á"}, 2, QStringLiteral("TopCol4"));
    EXPECT(ctrl.animate());
}

// Re-triggering a key after its overlay was hidden. Same variants, so only the
// hidden-to-visible step tells this apart from cycling.
void testShowFromHiddenSnaps() {
    OverlayController ctrl;
    ctrl.show({"ä", "à"}, 1, QStringLiteral("TopCol4"));
    ctrl.hide();
    ctrl.setAnimate(true);

    ctrl.show({"ä", "à"}, 0, QStringLiteral("TopCol4"));
    EXPECT(!ctrl.animate());
}

// THE reported bug. The commit flash leaves the committed variants on screen for
// 150 ms; pressing the same key again inside that window sends the SAME variants
// on a still-visible overlay, so neither visibility nor content can tell it from
// cycling. The engine's no-highlight index does: it only ever opens a gesture.
void testRetriggerDuringCommitFlashSnaps() {
    OverlayController ctrl;
    ctrl.show({"ä", "à"}, 1, QStringLiteral("TopCol4")); // cycled to à
    ctrl.show({"ä", "à"}, 0, QStringLiteral("TopCol4")); // commit flash, stays up
    ctrl.setAnimate(true);

    // Same key pressed again while the flash is still on screen.
    ctrl.show({"ä", "à"}, -1, QStringLiteral("TopCol4"));
    EXPECT(!ctrl.animate());
}

// A different key while the previous overlay is still up: different variants.
void testNewVariantsWhileVisibleSnaps() {
    OverlayController ctrl;
    ctrl.show({"ä", "à"}, 0, QStringLiteral("TopCol4"));
    ctrl.setAnimate(true);

    ctrl.show({"ö", "ô", "ó"}, -1, QStringLiteral("TopCol4"));
    EXPECT(!ctrl.animate());
}

// SetProgress opens a gesture and arrives BEFORE its Show, while the previous
// overlay can still be on screen. Without the gate the panel would animate down
// from its revealed state: a flash during the lead-in.
void testSetProgressSnaps() {
    OverlayController ctrl;
    ctrl.show({"ä", "à"}, 0, QStringLiteral("TopCol4"));
    ctrl.setAnimate(true);

    ctrl.setProgress(300, 700, 0);
    EXPECT(!ctrl.animate());
}

// The gate must close BEFORE the new values land, or the property write has
// already started the animation it was meant to prevent.
void testGateClosesBeforeStateChanges() {
    OverlayController ctrl;
    ctrl.show({"ä", "à"}, 0, QStringLiteral("TopCol4"));
    ctrl.setAnimate(true);

    bool animateWasOffWhenStateChanged = false;
    QObject::connect(&ctrl, &OverlayController::stateChanged, &ctrl,
                     [&]() { animateWasOffWhenStateChanged = !ctrl.animate(); });

    ctrl.show({"ö", "ô"}, -1, QStringLiteral("TopCol4"));
    EXPECT(animateWasOffWhenStateChanged);
}

// setAnimate is idempotent and stays silent when the value does not change. That
// is not a detail: it is why the renderer must NOT arm its "reopen the gate on
// the next drawn frame" off animateChanged. SetProgress closes the gate, and the
// Show that follows closes it again, which emits nothing, so an arming hung off
// that signal would never be renewed for the surface that is actually about to
// draw. Every progress-mode gesture takes that path.
void testSetAnimateIsIdempotentAndSilent() {
    OverlayController ctrl;
    EXPECT(ctrl.animate()); // transitions run until a gesture start closes them

    QSignalSpy spy(&ctrl, &OverlayController::animateChanged);

    ctrl.setProgress(300, 700, 0); // gesture start: closes the gate
    EXPECT(!ctrl.animate());
    EXPECT(spy.count() == 1);

    // The gesture's Show follows and closes it again. No change, no signal.
    ctrl.show({"ä", "à"}, -1, QStringLiteral("TopCol4"));
    EXPECT(!ctrl.animate());
    EXPECT(spy.count() == 1);

    // Reopening (the renderer, once the surface has drawn) reports once.
    ctrl.setAnimate(true);
    EXPECT(ctrl.animate());
    EXPECT(spy.count() == 2);

    ctrl.setAnimate(true);
    EXPECT(spy.count() == 2);
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    testShowPopulatesStateAndEmits();
    testSetProgressElapsedCompensation();
    testShowEmptyVariantsHides();
    testHideClearsVisibleKeepsPosition();
    testShowWithEmptyPositionPreservesPrevious();
    testLabelModeFlag();
    testAdaptorShowForwardsLabel();
    testQuitSchedulesAppExit();
    testLabelShowClearsProgressBar();
    testPlainShowKeepsProgressBar();
    testCyclingKeepsTransitions();
    testShowFromHiddenSnaps();
    testRetriggerDuringCommitFlashSnaps();
    testNewVariantsWhileVisibleSnaps();
    testSetProgressSnaps();
    testGateClosesBeforeStateChanges();
    testSetAnimateIsIdempotentAndSilent();

    std::fprintf(stderr, "testoverlaycontroller: all tests passed\n");
    return 0;
}
