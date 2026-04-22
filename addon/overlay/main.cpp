#include <QCursor>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QMargins>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QScreen>
#include <QWindow>
#include <memory>

#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>

#include "OverlayController.h"

namespace {

using LSWindow = LayerShellQt::Window;

constexpr int kEdgeMargin = 24;

struct Anchored {
    LSWindow::Anchors anchors;
    QMargins margins;
};

Anchored anchorsFor(const QString &position) {
    if (position == QLatin1String("TopLeft"))
        return {LSWindow::Anchors(LSWindow::AnchorTop) | LSWindow::AnchorLeft,
                {kEdgeMargin, kEdgeMargin, 0, 0}};
    if (position == QLatin1String("TopCenter"))
        return {LSWindow::Anchors(LSWindow::AnchorTop),
                {0, kEdgeMargin, 0, 0}};
    if (position == QLatin1String("TopRight"))
        return {LSWindow::Anchors(LSWindow::AnchorTop) | LSWindow::AnchorRight,
                {0, kEdgeMargin, kEdgeMargin, 0}};
    if (position == QLatin1String("CenterLeft"))
        return {LSWindow::Anchors(LSWindow::AnchorLeft),
                {kEdgeMargin, 0, 0, 0}};
    if (position == QLatin1String("Center"))
        return {{}, {}};
    if (position == QLatin1String("CenterRight"))
        return {LSWindow::Anchors(LSWindow::AnchorRight),
                {0, 0, kEdgeMargin, 0}};
    if (position == QLatin1String("BottomLeft"))
        return {LSWindow::Anchors(LSWindow::AnchorBottom) | LSWindow::AnchorLeft,
                {kEdgeMargin, 0, 0, kEdgeMargin}};
    if (position == QLatin1String("BottomCenter"))
        return {LSWindow::Anchors(LSWindow::AnchorBottom),
                {0, 0, 0, kEdgeMargin}};
    if (position == QLatin1String("BottomRight"))
        return {LSWindow::Anchors(LSWindow::AnchorBottom) | LSWindow::AnchorRight,
                {0, 0, kEdgeMargin, kEdgeMargin}};
    return {{}, {}};
}

QScreen *pickScreen(int cursorX, int cursorY) {
    // If the addon provided a real caret position (global coords), use it.
    // Otherwise default to the primary screen so the overlay lands in a
    // predictable place — the mouse cursor is a poor proxy for "where the
    // user is typing" because it can be left anywhere.
    if (cursorX >= 0) {
        if (auto *s = QGuiApplication::screenAt({cursorX, cursorY})) return s;
    }
    return QGuiApplication::primaryScreen();
}

// Creates a fresh QML window every time. Wayland layer-shell doesn't let us
// change anchors, margins or the output after the first commit, so the only
// reliable way to move the overlay between positions or monitors is to
// destroy the current window and commit a new surface from scratch.
class OverlayRenderer : public QObject {
public:
    explicit OverlayRenderer(OverlayController *ctrl)
        : QObject(ctrl), ctrl_(ctrl) {
        connect(ctrl, &OverlayController::stateChanged, this,
                &OverlayRenderer::syncToController);
    }

private:
    void syncToController() {
        if (!ctrl_->visible() || ctrl_->variants().isEmpty()) {
            teardown();
            return;
        }
        teardown();

        engine_ = std::make_unique<QQmlApplicationEngine>();
        engine_->rootContext()->setContextProperty("OverlayController", ctrl_);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        engine_->loadFromModule("SchnelleUmlauteOverlay", "Overlay");
#else
        engine_->load(QUrl(QStringLiteral(
            "qrc:/SchnelleUmlauteOverlay/Overlay.qml")));
#endif
        if (engine_->rootObjects().isEmpty()) {
            engine_.reset();
            return;
        }
        auto *qwin = qobject_cast<QWindow *>(engine_->rootObjects().first());
        if (!qwin) {
            engine_.reset();
            return;
        }
        auto *ls = LSWindow::get(qwin);
        if (!ls) return;
        ls->setLayer(LSWindow::LayerOverlay);
        ls->setKeyboardInteractivity(LSWindow::KeyboardInteractivityNone);
        ls->setScope(QStringLiteral("schnelle-umlaute-overlay"));
        if (auto *scr = pickScreen(ctrl_->cursorX(), ctrl_->cursorY())) {
            // QWindow::setScreen works on all LayerShellQt versions; Qt's
            // Wayland integration forwards the output hint to the layer
            // surface. LSWindow::setScreen only exists from 6.5+ and isn't
            // needed here — we set it before the first commit.
            qwin->setScreen(scr);
        }
        const auto a = anchorsFor(ctrl_->position());
        ls->setAnchors(a.anchors);
        ls->setMargins(a.margins);
        // Layer-shell props must be set before the first commit. Now that
        // everything is configured, reveal the window.
        qwin->setVisible(true);
    }

    void teardown() {
        if (engine_) {
            engine_->clearComponentCache();
            engine_.reset();
        }
    }

    OverlayController *ctrl_;
    std::unique_ptr<QQmlApplicationEngine> engine_;
};

} // namespace

int main(int argc, char *argv[]) {
    // Qt 6.5+ detects the layer-shell role automatically, and the explicit
    // call is deprecated since LayerShellQt 6.6. Only call it on older Qt
    // (Ubuntu 24.04 still ships Qt 6.4) so newer systems see no warning.
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    LayerShellQt::Shell::useLayerShell();
#endif

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("schnelle-umlaute-overlay"));
    QGuiApplication::setOrganizationName(QStringLiteral("schnelle-umlaute"));
    QGuiApplication::setQuitOnLastWindowClosed(false);

    auto *ctrl = new OverlayController(&app);
    new OverlayDBusAdaptor(ctrl);
    new OverlayRenderer(ctrl);

    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QStringLiteral("/de/schnelle_umlaute/Overlay"),
                            ctrl, QDBusConnection::ExportAdaptors)) {
        qCritical("Failed to register DBus object");
        return 1;
    }
    if (!bus.registerService(QStringLiteral("de.schnelle_umlaute.Overlay"))) {
        qCritical("Failed to register DBus service (already running?)");
        return 2;
    }

    return app.exec();
}
