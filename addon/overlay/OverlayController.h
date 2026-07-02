#ifndef SCHNELLE_UMLAUTE_OVERLAY_CONTROLLER_H
#define SCHNELLE_UMLAUTE_OVERLAY_CONTROLLER_H

#include <QDBusAbstractAdaptor>
#include <QObject>
#include <QStringList>

class OverlayController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList variants READ variants NOTIFY stateChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY stateChanged)
    Q_PROPERTY(QString position READ position NOTIFY stateChanged)
    Q_PROPERTY(bool visible READ visible NOTIFY stateChanged)
    // Label mode: render variants[0] as one full-width text (a profile-switch
    // name) instead of fixed single-glyph accent cells, so the name is not
    // truncated.
    Q_PROPERTY(bool label READ label NOTIFY stateChanged)
    // theme fires its own signal: palette changes don't need a surface
    // rebuild, only QML property re-evaluation.
    Q_PROPERTY(QString theme READ theme NOTIFY themeChanged)
    // Progress bar state fires its own signal: like theme it only drives QML
    // property re-evaluation and must not trigger a layer-shell surface rebuild
    // in the renderer (which listens to stateChanged only).
    Q_PROPERTY(int progressLeadMs READ progressLeadMs NOTIFY progressChanged)
    Q_PROPERTY(
        int progressWindowMs READ progressWindowMs NOTIFY progressChanged)
    Q_PROPERTY(bool progressActive READ progressActive NOTIFY progressChanged)
    Q_PROPERTY(bool progressFrozen READ progressFrozen NOTIFY progressChanged)
    // How far the gesture had already elapsed (ms) when SetProgress arrived,
    // measured against the engine's start timestamp on the shared monotonic
    // clock. The QML bar starts pre-advanced by this so D-Bus delivery latency
    // doesn't shift the visual window later than the engine's real one.
    Q_PROPERTY(
        int progressElapsedMs READ progressElapsedMs NOTIFY progressChanged)

public:
    explicit OverlayController(QObject *parent = nullptr);

    QStringList variants() const { return variants_; }
    int currentIndex() const { return currentIndex_; }
    QString position() const { return position_; }
    bool visible() const { return visible_; }
    bool label() const { return label_; }
    QString theme() const { return theme_; }
    int progressLeadMs() const { return progressLeadMs_; }
    int progressWindowMs() const { return progressWindowMs_; }
    bool progressActive() const { return progressActive_; }
    bool progressFrozen() const { return progressFrozen_; }
    int progressElapsedMs() const { return progressElapsedMs_; }

    // Called via DBus adapter
    void show(const QStringList &variants, int currentIndex,
              const QString &position, bool label = false);
    void hide();
    void quit();
    void setTheme(const QString &theme);
    // Forwards the KWin cursor script's reply (see CursorSource) on to any
    // listener via cursorReported(). The renderer connects its active
    // KWinCursorSource to it.
    void sendCursor(int x, int y);
    // Starts the progress timeline: leadMs lead-in then windowMs window. Marks
    // it active and un-frozen. startUsec is the gesture's start on the engine's
    // CLOCK_MONOTONIC clock; the elapsed offset since then (measured here on the
    // same clock) is exposed as progressElapsedMs so the bar can compensate for
    // delivery latency. startUsec <= 0 disables the compensation (elapsed 0).
    void setProgress(int leadMs, int windowMs, qint64 startUsec);
    // Holds the bar at its current position (a leader caught the window).
    void freezeProgress();

    static bool isValidTheme(const QString &name);

    // Pure progress-bar geometry exposed to QML so the sizing math lives once in
    // progress_overlay_geometry.h (tested), shared with the daemon's grid
    // placement, instead of being duplicated in QML bindings.
    Q_INVOKABLE int progressBarLength(int totalMs, int screenWidth) const;
    Q_INVOKABLE int progressLeadLength(int barLen, int leadMs, int totalMs) const;

    // Live elapsed time (ms) since the gesture start on the shared monotonic
    // clock, clamped to [0, total]. The QML bar samples this every frame so it
    // tracks the engine's real timeline instead of interpolating from a
    // once-set, late-started animation clock. That clock starts a frame or two
    // after SetProgress arrives but runs the full remaining duration, so it
    // finishes late and leaves the window open by a sliver right at the closing
    // edge. Sampling the real clock each frame keeps the error sub-frame and
    // non-cumulative. progressStartUsec_ <= 0 falls back to the arrival snapshot.
    Q_INVOKABLE int progressElapsedNowMs() const;

Q_SIGNALS:
    void stateChanged();
    void themeChanged();
    void cursorReported(int x, int y);
    void progressChanged();

private:
    QStringList variants_;
    int currentIndex_ = 0;
    QString position_ = QStringLiteral("TopCenter");
    bool visible_ = false;
    bool label_ = false;
    QString theme_ = QStringLiteral("schnelle-umlaute");
    int progressLeadMs_ = 0;
    int progressWindowMs_ = 0;
    int progressElapsedMs_ = 0;
    // Gesture start on the engine's CLOCK_MONOTONIC, kept so the live elapsed
    // getter can recompute against the real clock every frame. <= 0 disables it.
    qint64 progressStartUsec_ = 0;
    bool progressActive_ = false;
    bool progressFrozen_ = false;
};

// org.freedesktop.DBus adapter matching de.schnelle_umlaute.Overlay1.
class OverlayDBusAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "de.schnelle_umlaute.Overlay1")

public:
    explicit OverlayDBusAdaptor(OverlayController *ctrl);

public Q_SLOTS:
    void Show(const QStringList &variants, int currentIndex,
              const QString &position, bool label);
    void Hide();
    void Quit();
    void SetTheme(const QString &theme);
    // Called by the KWin cursor script with the live global pointer pixel.
    void SendCursor(int x, int y);
    void SetProgress(int leadMs, int windowMs, qlonglong startUsec);
    void FreezeProgress();
    // Wire-protocol version, so the engine can detect and restart a stale daemon
    // left over from an in-place upgrade. A daemon predating this method replies
    // UnknownMethod, which the engine reads as "stale".
    int GetProtocolVersion();

private:
    OverlayController *ctrl_;
};

#endif
