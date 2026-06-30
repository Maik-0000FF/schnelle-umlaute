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

    std::fprintf(stderr, "testoverlaycontroller: all tests passed\n");
    return 0;
}
