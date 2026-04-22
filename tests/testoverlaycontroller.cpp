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

#define EXPECT(cond) do {                                                   \
    if (!(cond)) {                                                          \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);\
        std::abort();                                                       \
    }                                                                       \
} while (0)

// show() with variants populates every field and flips visible to true.
void testShowPopulatesStateAndEmits() {
    OverlayController ctrl;
    QSignalSpy spy(&ctrl, &OverlayController::stateChanged);

    ctrl.show({"ä", "Ä"}, 1, QStringLiteral("BottomRight"), 100, 200);

    EXPECT(ctrl.visible());
    EXPECT(ctrl.currentIndex() == 1);
    EXPECT(ctrl.position() == QStringLiteral("BottomRight"));
    EXPECT(ctrl.cursorX() == 100);
    EXPECT(ctrl.cursorY() == 200);
    EXPECT(ctrl.variants().size() == 2);
    EXPECT(ctrl.variants().at(0) == QStringLiteral("ä"));
    EXPECT(spy.count() == 1);
}

// show() with an empty variants list must not display the overlay — the
// addon calls hide() via an empty show() payload in some paths and the
// QML engine teardown relies on visible() being false for that case.
void testShowEmptyVariantsHides() {
    OverlayController ctrl;
    ctrl.show({"x"}, 0, QStringLiteral("TopCenter"), 0, 0);
    EXPECT(ctrl.visible());

    ctrl.show({}, 0, QStringLiteral("TopCenter"), 0, 0);
    EXPECT(!ctrl.visible());
    EXPECT(ctrl.variants().isEmpty());
}

// hide() clears visibility but keeps the last-known position so the
// renderer doesn't snap back to the default anchor between cycles.
void testHideClearsVisibleKeepsPosition() {
    OverlayController ctrl;
    ctrl.show({"ö", "Ö"}, 0, QStringLiteral("TopLeft"), 50, 60);
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
    ctrl.show({"ü"}, 0, QStringLiteral("Center"), 0, 0);
    EXPECT(ctrl.position() == QStringLiteral("Center"));

    ctrl.show({"ü"}, 0, QString(), 0, 0);
    EXPECT(ctrl.position() == QStringLiteral("Center"));
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

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    testShowPopulatesStateAndEmits();
    testShowEmptyVariantsHides();
    testHideClearsVisibleKeepsPosition();
    testShowWithEmptyPositionPreservesPrevious();
    testQuitSchedulesAppExit();

    std::fprintf(stderr, "testoverlaycontroller: all tests passed\n");
    return 0;
}
