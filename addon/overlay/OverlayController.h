#ifndef SCHNELLE_UMLAUTE_OVERLAY_CONTROLLER_H
#define SCHNELLE_UMLAUTE_OVERLAY_CONTROLLER_H

#include <QDBusAbstractAdaptor>
#include <QObject>
#include <QQmlEngine>
#include <QStringList>

class OverlayController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList variants READ variants NOTIFY stateChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY stateChanged)
    Q_PROPERTY(QString position READ position NOTIFY stateChanged)
    Q_PROPERTY(bool visible READ visible NOTIFY stateChanged)

public:
    explicit OverlayController(QObject *parent = nullptr);

    QStringList variants() const { return variants_; }
    int currentIndex() const { return currentIndex_; }
    QString position() const { return position_; }
    bool visible() const { return visible_; }

    // Called via DBus adapter
    void show(const QStringList &variants, int currentIndex,
              const QString &position, int cursorX, int cursorY);
    void hide();

    int cursorX() const { return cursorX_; }
    int cursorY() const { return cursorY_; }

Q_SIGNALS:
    void stateChanged();

private:
    QStringList variants_;
    int currentIndex_ = 0;
    QString position_ = QStringLiteral("TopCenter");
    bool visible_ = false;
    int cursorX_ = -1;
    int cursorY_ = -1;
};

// org.freedesktop.DBus adapter matching de.schnelle_umlaute.Overlay1.
class OverlayDBusAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "de.schnelle_umlaute.Overlay1")

public:
    explicit OverlayDBusAdaptor(OverlayController *ctrl);

public Q_SLOTS:
    void Show(const QStringList &variants, int currentIndex,
              const QString &position, int cursorX, int cursorY);
    void Hide();

private:
    OverlayController *ctrl_;
};

#endif
