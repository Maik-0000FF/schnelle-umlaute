#include <algorithm>
#include <memory>
#include <optional>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QFile>
#include <QGuiApplication>
#include <QMargins>
#include <QPoint>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRect>
#include <QScreen>
#include <QStandardPaths>
#include <QTextStream>
#include <QWindow>

#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>

#include "CursorSource.h"
#include "OverlayController.h"
#include "cursor_overlay_geometry.h"
#include "overlay_render.h"
#include "progress_overlay_geometry.h"

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

// Run the layout pass NOW, depth-first, so implicit sizes are current.
//
// The panel's width comes from a RowLayout, and QQuickLayout does not compute
// its implicit size when the model changes: it invalidates and defers the work
// to the polish pass, which the render loop drives. A hidden window never
// renders, so with a persistent engine nothing would run it, and the anchor
// math below would read the PREVIOUS overlay's width: a 2-cell panel followed
// by a 7-cell one would be anchored as if it were still 2 cells wide, landing
// the panel off its column. (Measured on the layer-shell wire: set_margin was
// one show behind.) Rebuilding the engine per show used to hide this, because a
// fresh QML load lays out on completion.
//
// Children first: a parent's implicit size is derived from theirs.
void polishTree(QQuickItem *item) {
    if (!item)
        return;
    const auto children = item->childItems();
    for (QQuickItem *child : children)
        polishTree(child);
    item->ensurePolished();
}

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

// Drives the QML window from the controller's state. The engine is built once
// and lives for the daemon's lifetime; showing and hiding is
// QWindow::setVisible(), and only a change of position or mode re-anchors.
//
// The surface still comes and goes with the window, and that is deliberate: a
// layer surface bakes its anchors at the first commit, and Qt drops the
// wl_surface on hide and creates a fresh layer surface on show, so a new
// position always gets anchors that take effect. What the engine's lifetime
// buys is the rest: no QML re-parse, no object tree, no scene graph rebuild per
// open. That used to run on EVERY show, which with the timing bar enabled means
// every keystroke of a mapped letter (the bar shows from t=0). Measured on the
// wire, 40 open/close cycles cost 2.9 s of daemon CPU before and 0.4 s after.
//
// Plain variants/currentIndex/theme updates (i.e. cycling) never touch any of
// this: they ride the Q_PROPERTY bindings on the surface that is already up.
class OverlayRenderer : public QObject {
public:
    explicit OverlayRenderer(OverlayController *ctrl)
        : QObject(ctrl), ctrl_(ctrl) {
        connect(ctrl, &OverlayController::stateChanged, this,
                &OverlayRenderer::syncToController);
    }

private:
    void syncToController() {
        const QString pos = ctrl_->position();
        const schnelle_umlaute::render::RenderRequest req{
            ctrl_->visible(), !ctrl_->variants().isEmpty(), pos.toStdString(),
            ctrl_->label()};
        const schnelle_umlaute::render::RenderState state{
            active_, lastPosition_.toStdString(), lastLabel_};

        switch (schnelle_umlaute::render::decideRenderAction(req, state)) {
        case schnelle_umlaute::render::RenderAction::None:
            // Content-only update on the surface that is already up. If this was
            // a gesture start (the controller closed the gate before writing the
            // new values), THIS surface is the one that will draw them, so it is
            // the one to wait on before letting transitions run again.
            if (!ctrl_->animate())
                armTransitionRestore();
            return;
        case schnelle_umlaute::render::RenderAction::Hide:
            hideWindow();
            return;
        case schnelle_umlaute::render::RenderAction::Show:
            break;
        }

        if (!ensureEngine())
            return;
        // Hide before re-anchoring. Layer-shell bakes the anchors of a surface
        // at its first commit, and Qt drops the wl_surface when the window is
        // hidden, so this is what gets the new position a surface of its own.
        hideWindow();
        // Mark the placement committed BEFORE the possibly-async cursor fetch,
        // so a cycling update arriving mid-fetch takes the None branch above and
        // rides the existing bindings instead of racing the reply.
        lastPosition_ = pos;
        lastLabel_ = ctrl_->label();
        active_ = true;
        epoch_ = schnelle_umlaute::render::nextEpoch(epoch_);
        reveal(pos, epoch_);
    }

    // Builds the QML engine and its root window ONCE. Layer-shell properties
    // that never change (layer, scope, keyboard interactivity, output policy)
    // are set here; only anchors and margins are per-show.
    bool ensureEngine() {
        if (engine_)
            return qwin_ != nullptr;

        engine_ = std::make_unique<QQmlApplicationEngine>();
        engine_->rootContext()->setContextProperty("OverlayController", ctrl_);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        engine_->loadFromModule("SchnelleUmlauteOverlay", "Overlay");
#else
        // See addon/editor/main.cpp: on Qt 6.4 a URL-loaded root is not tied
        // to its module, so `import SchnelleUmlauteOverlay` needs the embedded
        // qmldir on an import path. Overlay.qml has no singleton today, but
        // keep this symmetric with the editor so it cannot regress the same way.
        engine_->addImportPath(QStringLiteral("qrc:/qt/qml"));
        engine_->load(
            QUrl(QStringLiteral("qrc:/qt/qml/SchnelleUmlauteOverlay/Overlay.qml")));
#endif
        if (engine_->rootObjects().isEmpty()) {
            engine_.reset();
            return false;
        }
        auto *qwin = qobject_cast<QWindow *>(engine_->rootObjects().first());
        if (!qwin) {
            engine_.reset();
            return false;
        }
        auto *ls = LSWindow::get(qwin);
        if (!ls) {
            engine_.reset();
            return false;
        }
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
        qwin_ = qwin;
        return true;
    }

    // The controller turns transitions off when a gesture start overwrites the
    // last one's values (see OverlayController::show). Turn them back on once the
    // surface has actually DRAWN them: any earlier and the very change that was
    // snapped could still animate, any later and the cycling handover would lose
    // its animation.
    //
    // Call this from the paths that KNOW which surface is about to draw, never
    // from the animateChanged signal: setAnimate(false) is idempotent, so the
    // Show that follows a SetProgress emits nothing at all (the gate is already
    // shut), and an arming made off that signal would still be pointed at the
    // previous gesture's window.
    void armTransitionRestore() {
        auto *qq = qobject_cast<QQuickWindow *>(qwin_.data());
        if (!qq)
            return;
        // Exactly one restore may be pending. frameSwapped fires every frame,
        // from the render thread, so a standing connection would queue a
        // cross-thread call 60 times a second only to hit setAnimate's early
        // return, and a placement that never draws (a failed cursor query, a
        // gesture cancelled before it revealed) would leave its arming behind for
        // the next one to pile onto.
        //
        // Two things do that: the member holds the live connection so the next
        // arming can drop it, and the generation makes a call the PREVIOUS
        // surface already queued inert, since disconnecting does not unqueue what
        // is already posted. Without it, such a call would reopen the gate before
        // the new surface has drawn.
        QObject::disconnect(restore_);
        const quint64 generation = ++restoreGeneration_;
        restore_ = connect(qq, &QQuickWindow::frameSwapped, this,
                           [this, generation]() {
                               if (generation != restoreGeneration_)
                                   return;
                               QObject::disconnect(restore_);
                               ctrl_->setAnimate(true);
                           });
        // Do not assume a frame is coming. A gesture start can re-send exactly
        // what is already on screen (the same variants at the same no-highlight
        // index, e.g. the same key pressed again while its preview is still up):
        // nothing in the scene changes, nothing renders, and the gate would stay
        // shut until some later change happened to draw one. The first cycling
        // handover would then snap instead of animating. Ask for the frame.
        qq->requestUpdate();
    }

    void hideWindow() {
        if (qwin_)
            qwin_->setVisible(false);
        active_ = false;
        // Whatever a still-pending cursor reply was going to place, it is not
        // this. Close its epoch.
        epoch_ = schnelle_umlaute::render::nextEpoch(epoch_);
    }

    // Settle the QML layout so implicit sizes (the panel width the anchor math
    // reads) reflect the variants that are on screen right now. See polishTree.
    void settleLayout() {
        if (auto *qq = qobject_cast<QQuickWindow *>(qwin_.data()))
            polishTree(qq->contentItem());
    }

    void reveal(const QString &pos,
                const schnelle_umlaute::render::RenderEpoch epoch) {
        QWindow *qwin = qwin_;
        if (!qwin)
            return;

        const schnelle_umlaute::CursorPositionSpec spec =
            schnelle_umlaute::parseCursorPosition(pos.toStdString());
        const QString grid = QString::fromStdString(spec.grid);

        // Grid reveal: anchor the surface per the 7×3 position and show. Col
        // 2/3/5/6 need the screen width (to place the overlay at a fraction of
        // it) and the overlay's own width (to subtract half so the anchored
        // margin lands the *center* of the overlay on the target fraction, not
        // its leading edge). Col 1/4/7 don't use these, so a fallback is
        // harmless. Layer-shell props must be set before this first commit.
        QPointer<QWindow> qwinPtr(qwin);
        auto revealGrid = [this, qwinPtr, grid, epoch]() {
            // Deferred: reached from the cursor callback too, which may fire
            // after this placement is over. `grid` is a copy of THAT gesture's
            // position, so applying it to a later one would misplace it.
            if (!schnelle_umlaute::render::isEpochCurrent(epoch, epoch_))
                return;
            if (!qwinPtr)
                return;
            // Every caller lands here: the synchronous grid path, and both of the
            // cursor callback's fallbacks (no pointer, no screen). This IS the
            // pass that makes the implicit sizes current, so the anchor math
            // below reads the width of the panel about to be shown rather than
            // the last one's. On the deferred paths it additionally catches up
            // with any cycling that happened while the pointer query was in
            // flight.
            settleLayout();
            auto *ls2 = LSWindow::get(qwinPtr);
            if (!ls2)
                return;
            QScreen *scr = qwinPtr->screen();
            if (!scr)
                scr = QGuiApplication::primaryScreen();
            const int sw = scr ? scr->geometry().width()
                               : schnelle_umlaute::render::kFallbackScreenWidth;
            const int ow = qwinPtr->width() > 0
                               ? qwinPtr->width()
                               : schnelle_umlaute::render::kFallbackOverlayWidth;
            auto a = anchorsFor(grid, sw, ow);
            // In progress mode the surface includes the bar overhang to the
            // right of the panel; anchorsFor centres the whole surface, which
            // would shift the panel left by half the bar. Re-anchor horizontally
            // so the PANEL lands on the column (vertical/row placement stays),
            // clamped so the bar's right end stays on the output.
            int row = 0, col = 0;
            if (ctrl_->progressActive() &&
                parsePosition(canonicalizePosition(grid), row, col)) {
                (void)row;
                // The surface is max(panel, bar) wide with the bar left-aligned;
                // read the panel width from QML so the panel (not the surface)
                // is centred on the column. If the property can't be read (0),
                // skip the override and keep anchorsFor's surface-centring
                // rather than mis-place the panel.
                const int frameW = qwinPtr->property("frameWidth").toInt();
                if (frameW > 0) {
                    a.anchors &= ~(LSWindow::AnchorLeft | LSWindow::AnchorRight);
                    a.anchors |= LSWindow::AnchorLeft;
                    a.margins.setLeft(
                        schnelle_umlaute::progress::gridPanelLeftMargin(
                            col, sw, frameW, ow, kEdgeMargin));
                    a.margins.setRight(0);
                }
            }
            ls2->setAnchors(a.anchors);
            ls2->setMargins(a.margins);
            qwinPtr->setVisible(true);
            // This surface is the one that will draw the snapped state, so it is
            // the one whose first frame reopens the gate.
            armTransitionRestore();
        };

        if (!spec.atCursor) {
            revealGrid();
            return;
        }

        // Cursor mode: fetch the global pointer asynchronously, then place the
        // overlay's lower-left corner there (on the cursor's output). Any
        // failure — unsupported compositor, query error, timeout — falls back
        // to the grid reveal above.
        cursorSource()->getCursor(
            [this, qwinPtr, revealGrid, epoch](
                std::optional<schnelle_umlaute::CursorPos> cur) {
                // The gesture that asked for this pointer may be long over: hidden
                // again, or superseded by the next one (which reopens the window,
                // so ctrl_->visible() is true again and answers nothing). The
                // window outlives the gesture now, so only the epoch can tell.
                if (!schnelle_umlaute::render::isEpochCurrent(epoch, epoch_))
                    return;
                if (!qwinPtr || !ctrl_->visible())
                    return;
                if (!cur) {
                    revealGrid();
                    return;
                }
                auto *ls2 = LSWindow::get(qwinPtr);
                if (!ls2)
                    return;
                // This path reads the size itself rather than going through
                // revealGrid(), so it settles the layout itself too: it runs event
                // loops after the placement started, and a variants update in
                // between would have invalidated the implicit sizes.
                settleLayout();
                QScreen *scr =
                    QGuiApplication::screenAt(QPoint(cur->x, cur->y));
                if (!scr)
                    scr = qwinPtr->screen();
                if (!scr)
                    scr = QGuiApplication::primaryScreen();
                if (!scr) {
                    revealGrid();
                    return;
                }
                qwinPtr->setScreen(scr);
                const QRect geo = scr->geometry();
                const int ow =
                    qwinPtr->width() > 0
                        ? qwinPtr->width()
                        : schnelle_umlaute::render::kFallbackOverlayWidth;
                const int oh =
                    qwinPtr->height() > 0
                        ? qwinPtr->height()
                        : schnelle_umlaute::render::kFallbackOverlayHeight;
                const auto m = schnelle_umlaute::cursorMargins(
                    cur->x, cur->y, geo.x(), geo.y(), geo.width(), geo.height(),
                    ow, oh);
                ls2->setAnchors(LSWindow::Anchors(LSWindow::AnchorTop |
                                                  LSWindow::AnchorLeft));
                ls2->setMargins(QMargins(m.left, m.top, 0, 0));
                qwinPtr->setVisible(true);
                armTransitionRestore();
            });
    }

    // Lazily build the cursor backend for the running compositor and wire the
    // KWin script reply path. Created on first cursor-mode open so a user who
    // never enables it pays nothing; lives for the daemon's lifetime.
    schnelle_umlaute::CursorSource *cursorSource() {
        if (!cursorSource_) {
            schnelle_umlaute::KWinDeps deps;
            deps.scriptDir = QStandardPaths::writableLocation(
                                 QStandardPaths::GenericCacheLocation) +
                             QStringLiteral("/schnelle-umlaute-overlay");
            deps.serviceName = QStringLiteral("de.schnelle_umlaute.Overlay");
            deps.objectPath = QStringLiteral("/de/schnelle_umlaute/Overlay");
            deps.interfaceName = QStringLiteral("de.schnelle_umlaute.Overlay1");
            cursorSource_ = schnelle_umlaute::createCursorSource(
                QString::fromLocal8Bit(qgetenv("XDG_CURRENT_DESKTOP")), deps,
                this);
            // The KWin script calls SendCursor on the daemon's service, which
            // surfaces as cursorReported here; forward it to the source.
            // A no-op for the CLI / null sources.
            connect(ctrl_, &OverlayController::cursorReported, cursorSource_,
                    [src = cursorSource_](int requestId, int x, int y) {
                        src->reportCursor(requestId, x, y);
                    });
        }
        return cursorSource_;
    }

    OverlayController *ctrl_;
    std::unique_ptr<QQmlApplicationEngine> engine_;
    // Root QML window, owned by the engine and alive for the daemon's lifetime.
    QPointer<QWindow> qwin_;
    // True from the moment we commit to showing at (lastPosition_, lastLabel_)
    // until the window is hidden again. Replaces the old "engine_ != nullptr"
    // proxy, which only worked because teardown() destroyed the engine.
    bool active_ = false;
    // Identifies the current placement, so async work started by an earlier one
    // (the cursor fetch) can recognise that it is late and stand down.
    schnelle_umlaute::render::RenderEpoch epoch_ =
        schnelle_umlaute::render::kFirstEpoch;
    QString lastPosition_;
    // Part of the surface-reuse key: label and grid modes have very different
    // widths, and the layer-shell anchor margin is baked from the width at
    // surface-build time (re-anchoring needs a fresh surface). Switching mode
    // must rebuild, or the reused surface keeps a stale margin and renders
    // off-center at fractional-column placements.
    bool lastLabel_ = false;
    schnelle_umlaute::CursorSource *cursorSource_ = nullptr;
    // The one pending "restore transitions on the next drawn frame" connection,
    // and the generation that says which placement armed it.
    QMetaObject::Connection restore_;
    quint64 restoreGeneration_ = 0;
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
    QGuiApplication::setApplicationVersion(
        QStringLiteral(SCHNELLE_UMLAUTE_VERSION));
    QGuiApplication::setQuitOnLastWindowClosed(false);

    // Handle --version / --help before registering the D-Bus service, so both
    // print and exit immediately without touching the session bus.
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Schnelle Umlaute overlay daemon"));
    parser.addHelpOption();    // -h / --help
    parser.addVersionOption(); // -v / --version
    parser.process(app);

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
