#include <QDBusConnection>
#include <QFile>
#include <QGuiApplication>
#include <QMargins>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QScreen>
#include <QStandardPaths>
#include <QTextStream>
#include <QWindow>
#include <memory>

#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>

#include "OverlayController.h"

namespace {

using LSWindow = LayerShellQt::Window;

constexpr int kEdgeMargin = 24;

// Reads the Theme= key from the editor's config file so the overlay
// starts with the user's chosen palette instead of flashing the default
// one for the first cycle. Absent/malformed file → default theme.
QString loadInitialTheme() {
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    QFile f(base + QStringLiteral("/fcitx5/conf/schnelle-umlaute.conf"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream in(&f);
    QString section;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.size() - 2);
            continue;
        }
        if (section != QLatin1String("Theme")) continue;
        const int eq = line.indexOf('=');
        if (eq < 0) continue;
        if (line.left(eq) == QLatin1String("Theme")) {
            return line.mid(eq + 1).trimmed();
        }
    }
    return {};
}

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

// Rebuilds the QML window only when the overlay position changes. Wayland
// layer-shell forbids changing anchors/margins/output after the first
// commit, so moving between positions or monitors needs a fresh surface.
// Plain variants/currentIndex updates (i.e. cycling) ride the existing
// Q_PROPERTY bindings — rebuilding on every keystroke caused visible
// flicker because each rebuild commits a new layer-shell surface.
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
        const QString pos = ctrl_->position();
        if (engine_ && pos == lastPosition_) {
            // Same position, surface already committed — QML bindings on
            // OverlayController.variants/currentIndex update the content.
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
        // Tell the compositor to pick the output itself (output=NULL on the
        // wire). KWin's wlr-layer-shell implementation places that on the
        // monitor containing the focused surface — i.e. where the user is
        // typing. QGuiApplication::primaryScreen() is unusable on Wayland:
        // wl_output has no primary concept and Qt returns whichever output
        // the compositor bound first, which on KDE Plasma does not track
        // the user's configured primary (QTBUG-90716).
#if defined(SCHNELLE_LAYERSHELLQT_HAS_ACTIVE_SCREEN)
        ls->setWantsToBeOnActiveScreen(true);
#elif defined(SCHNELLE_LAYERSHELLQT_HAS_SCREEN_CONFIG)
        // 6.0-6.5 path. Deprecated since 6.6 but same protocol-level NULL.
        ls->setScreenConfiguration(LSWindow::ScreenFromCompositor);
#else
        // LayerShellQt < 6.0 (Ubuntu 24.04 ships 5.27.11) has neither API.
        // Best we can do is ask Qt for "primary" and pin the window there
        // via QWindow::setScreen — this is the buggy-on-multi-monitor path
        // but keeps the overlay functional on older distros.
        if (auto *scr = QGuiApplication::primaryScreen()) qwin->setScreen(scr);
#endif
        const auto a = anchorsFor(ctrl_->position());
        ls->setAnchors(a.anchors);
        ls->setMargins(a.margins);
        // Layer-shell props must be set before the first commit. Now that
        // everything is configured, reveal the window.
        qwin->setVisible(true);
        lastPosition_ = pos;
    }

    void teardown() {
        if (engine_) {
            engine_->clearComponentCache();
            engine_.reset();
        }
        lastPosition_.clear();
    }

    OverlayController *ctrl_;
    std::unique_ptr<QQmlApplicationEngine> engine_;
    QString lastPosition_;
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
    const QString initialTheme = loadInitialTheme();
    if (!initialTheme.isEmpty()) ctrl->setTheme(initialTheme);
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
