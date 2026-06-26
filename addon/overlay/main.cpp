#include <algorithm>
#include <memory>
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
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream in(&f);
    QString section;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.size() - 2);
            continue;
        }
        if (section != QLatin1String("Theme"))
            continue;
        const int eq = static_cast<int>(line.indexOf('='));
        if (eq < 0)
            continue;
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

// Pre-1.2 used a 3×3 grid; 1.2 moved to 7×3. Map the old names onto
// the equivalent column so legacy DBus callers and older configs keep
// working.
QString canonicalizePosition(const QString &v) {
    if (v == QLatin1String("TopLeft"))
        return QStringLiteral("TopCol1");
    if (v == QLatin1String("TopCenter"))
        return QStringLiteral("TopCol4");
    if (v == QLatin1String("TopRight"))
        return QStringLiteral("TopCol7");
    if (v == QLatin1String("CenterLeft"))
        return QStringLiteral("CenterCol1");
    if (v == QLatin1String("Center"))
        return QStringLiteral("CenterCol4");
    if (v == QLatin1String("CenterRight"))
        return QStringLiteral("CenterCol7");
    if (v == QLatin1String("BottomLeft"))
        return QStringLiteral("BottomCol1");
    if (v == QLatin1String("BottomCenter"))
        return QStringLiteral("BottomCol4");
    if (v == QLatin1String("BottomRight"))
        return QStringLiteral("BottomCol7");
    return v;
}

bool parsePosition(const QString &pos, int &row, int &col) {
    int prefixLen = 0;
    if (pos.startsWith(QLatin1String("TopCol"))) {
        row = 0;
        prefixLen = 6;
    } else if (pos.startsWith(QLatin1String("CenterCol"))) {
        row = 1;
        prefixLen = 9;
    } else if (pos.startsWith(QLatin1String("BottomCol"))) {
        row = 2;
        prefixLen = 9;
    } else
        return false;

    bool ok = false;
    const int n = pos.mid(prefixLen).toInt(&ok);
    if (!ok || n < 1 || n > 7)
        return false;
    col = n - 1;
    return true;
}

// 7-column × 3-row grid on the active output, uniformly spaced at 12.5%
// of screen width: columns land at 12.5/25/37.5/50/62.5/75/87.5 %.
// Col 2 and Col 6 hit the centers of a 50/50 splitscreen (25 % and
// 75 %); Col 4 stays compositor-centered (no horizontal anchor). Col 1
// and Col 7 no longer hug the screen edge — they sit one step inward
// so the spacing around each anchor is uniform.
Anchored anchorsFor(const QString &position, int screenWidth,
                    int overlayWidth) {
    int row = 0, col = 0;
    if (!parsePosition(canonicalizePosition(position), row, col)) {
        return {{}, {}};
    }

    LSWindow::Anchors a;
    int top = 0, bottom = 0, left = 0, right = 0;

    if (row == 0) {
        a |= LSWindow::AnchorTop;
        top = kEdgeMargin;
    } else if (row == 2) {
        a |= LSWindow::AnchorBottom;
        bottom = kEdgeMargin;
    }

    if (col == 3) {
        // no horizontal anchor → screen-centered
    } else if (col < 3) {
        const int center = screenWidth * (col + 1) / 8;
        a |= LSWindow::AnchorLeft;
        left = std::max(kEdgeMargin, center - overlayWidth / 2);
    } else {
        const int centerFromRight = screenWidth * (7 - col) / 8;
        a |= LSWindow::AnchorRight;
        right = std::max(kEdgeMargin, centerFromRight - overlayWidth / 2);
    }

    return {a, QMargins(left, top, right, bottom)};
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
        engine_->load(
            QUrl(QStringLiteral("qrc:/qt/qml/SchnelleUmlauteOverlay/Overlay.qml")));
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
        if (!ls)
            return;
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
        if (auto *scr = QGuiApplication::primaryScreen())
            qwin->setScreen(scr);
#endif
        // Col 2/3/5/6 need the screen width (to place the overlay at a
        // fraction of it) and the overlay's own width (to subtract half
        // so the anchored margin lands the *center* of the overlay on
        // the target fraction, not its leading edge). Col 1/4/7 don't
        // use these values, so inaccurate fallbacks are harmless.
        QScreen *targetScreen = qwin->screen();
        if (!targetScreen)
            targetScreen = QGuiApplication::primaryScreen();
        const int screenWidth =
            targetScreen ? targetScreen->geometry().width() : 1920;
        const int overlayWidth = qwin->width() > 0 ? qwin->width() : 200;
        const auto a = anchorsFor(ctrl_->position(), screenWidth, overlayWidth);
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
    QGuiApplication::setApplicationName(
        QStringLiteral("schnelle-umlaute-overlay"));
    QGuiApplication::setOrganizationName(QStringLiteral("schnelle-umlaute"));
    QGuiApplication::setQuitOnLastWindowClosed(false);

    auto *ctrl = new OverlayController(&app);
    const QString initialTheme = loadInitialTheme();
    if (!initialTheme.isEmpty())
        ctrl->setTheme(initialTheme);
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
