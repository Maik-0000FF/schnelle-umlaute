// Unit tests for OverlayController theme state — the palette is driven
// by SetTheme DBus calls from the editor. Guards the validator and the
// themeChanged signal that the overlay QML bindings hinge on.

#include "overlay/OverlayController.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QString>

#include <cstdio>
#include <cstdlib>

#define EXPECT(cond) do {                                                   \
    if (!(cond)) {                                                          \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);\
        std::abort();                                                       \
    }                                                                       \
} while (0)

// Fresh controller starts on the schnelle-umlaute palette so the first
// cycle after install looks like the marketing screenshots.
void testDefaultIsSchnelleUmlaute() {
    OverlayController ctrl;
    EXPECT(ctrl.theme() == QStringLiteral("schnelle-umlaute"));
}

// A valid theme name flips the state and emits themeChanged exactly once.
void testSetValidThemeEmitsAndUpdates() {
    OverlayController ctrl;
    QSignalSpy spy(&ctrl, &OverlayController::themeChanged);

    ctrl.setTheme(QStringLiteral("dark"));
    EXPECT(ctrl.theme() == QStringLiteral("dark"));
    EXPECT(spy.count() == 1);

    ctrl.setTheme(QStringLiteral("contrast"));
    EXPECT(ctrl.theme() == QStringLiteral("contrast"));
    EXPECT(spy.count() == 2);
}

// Repeating the current theme is a no-op: no extra signal, no churn in
// the QML bindings that depend on themeChanged.
void testSetSameThemeDoesNotEmit() {
    OverlayController ctrl;
    ctrl.setTheme(QStringLiteral("light"));
    QSignalSpy spy(&ctrl, &OverlayController::themeChanged);

    ctrl.setTheme(QStringLiteral("light"));
    EXPECT(spy.count() == 0);
    EXPECT(ctrl.theme() == QStringLiteral("light"));
}

// Unknown names are rejected silently (stderr log only). Guards against
// a stale editor or a corrupted config file flipping the palette to
// something Overlay.qml can't resolve.
void testSetInvalidThemeRejected() {
    OverlayController ctrl;
    ctrl.setTheme(QStringLiteral("dark"));
    QSignalSpy spy(&ctrl, &OverlayController::themeChanged);

    ctrl.setTheme(QStringLiteral("solarized"));
    EXPECT(spy.count() == 0);
    EXPECT(ctrl.theme() == QStringLiteral("dark"));

    ctrl.setTheme(QString());
    EXPECT(spy.count() == 0);
    EXPECT(ctrl.theme() == QStringLiteral("dark"));
}

// Pure validator: the four palette names the editor accepts and nothing
// else. Keep this in sync with Theme.qml's palettes map.
void testIsValidThemeMatchesPalettes() {
    EXPECT(OverlayController::isValidTheme(QStringLiteral("schnelle-umlaute")));
    EXPECT(OverlayController::isValidTheme(QStringLiteral("dark")));
    EXPECT(OverlayController::isValidTheme(QStringLiteral("light")));
    EXPECT(OverlayController::isValidTheme(QStringLiteral("contrast")));

    EXPECT(!OverlayController::isValidTheme(QString()));
    EXPECT(!OverlayController::isValidTheme(QStringLiteral("Dark"))); // case-sensitive
    EXPECT(!OverlayController::isValidTheme(QStringLiteral("solarized")));
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    testDefaultIsSchnelleUmlaute();
    testSetValidThemeEmitsAndUpdates();
    testSetSameThemeDoesNotEmit();
    testSetInvalidThemeRejected();
    testIsValidThemeMatchesPalettes();

    std::fprintf(stderr, "testoverlaytheme: all tests passed\n");
    return 0;
}
