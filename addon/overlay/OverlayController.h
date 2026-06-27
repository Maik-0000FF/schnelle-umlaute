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

public:
    explicit OverlayController(QObject *parent = nullptr);

    QStringList variants() const { return variants_; }
    int currentIndex() const { return currentIndex_; }
    QString position() const { return position_; }
    bool visible() const { return visible_; }
    QString theme() const { return theme_; }
    int progressLeadMs() const { return progressLeadMs_; }
    int progressWindowMs() const { return progressWindowMs_; }
    bool progressActive() const { return progressActive_; }
    bool progressFrozen() const { return progressFrozen_; }

    // Called via DBus adapter
    void show(const QStringList &variants, int currentIndex,
              const QString &position);
    void hide();
    void quit();
    void setTheme(const QString &theme);
    // Forwards the KWin cursor script's reply (see CursorSource) on to any
    // listener via cursorReported(). The renderer connects its active
    // KWinCursorSource to it.
    void sendCursor(int x, int y);
    // Starts the progress timeline: leadMs lead-in then windowMs window. Marks
    // it active and un-frozen.
    void setProgress(int leadMs, int windowMs);
    // Holds the bar at its current position (a leader caught the window).
    void freezeProgress();

    static bool isValidTheme(const QString &name);

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
    QString theme_ = QStringLiteral("schnelle-umlaute");
    int progressLeadMs_ = 0;
    int progressWindowMs_ = 0;
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
              const QString &position);
    void Hide();
    void Quit();
    void SetTheme(const QString &theme);
    // Called by the KWin cursor script with the live global pointer pixel.
    void SendCursor(int x, int y);
    void SetProgress(int leadMs, int windowMs);
    void FreezeProgress();

private:
    OverlayController *ctrl_;
};

#endif
