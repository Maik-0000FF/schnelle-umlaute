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

public:
    explicit OverlayController(QObject *parent = nullptr);

    QStringList variants() const { return variants_; }
    int currentIndex() const { return currentIndex_; }
    QString position() const { return position_; }
    bool visible() const { return visible_; }
    QString theme() const { return theme_; }

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

    static bool isValidTheme(const QString &name);

Q_SIGNALS:
    void stateChanged();
    void themeChanged();
    void cursorReported(int x, int y);

private:
    QStringList variants_;
    int currentIndex_ = 0;
    QString position_ = QStringLiteral("TopCenter");
    bool visible_ = false;
    QString theme_ = QStringLiteral("schnelle-umlaute");
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

private:
    OverlayController *ctrl_;
};

#endif
