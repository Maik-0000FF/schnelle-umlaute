#ifndef SCHNELLE_UMLAUTE_CURSOR_SOURCE_H
#define SCHNELLE_UMLAUTE_CURSOR_SOURCE_H

// Per-compositor source of the global mouse-pointer position for the
// "show at cursor" overlay mode. Wayland clients may not query the global
// pointer, and none of the compositor-specific routes are a portable client
// API, so this abstracts over them: the renderer asks a CursorSource picked
// for the running desktop and stays compositor-agnostic. A backend that can't
// answer reports failure (std::nullopt) and the renderer falls back to the
// configured grid position.
//
// Port of SpaceUX's src/main/cursor-source.ts (KWin script / hyprctl / mmsg).
// Each query is one-shot — there is no background polling or persistent helper
// process, so the feature adds no idle load.

#include <functional>
#include <optional>

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

namespace schnelle_umlaute {

struct CursorPos {
    int x;
    int y;
};

// Invoked exactly once per getCursor() call: a value on success, std::nullopt
// when this backend can't answer (wrong compositor, failure, timeout).
using CursorCallback = std::function<void(std::optional<CursorPos>)>;

// Parse a `{"x":<num>,"y":<num>,...}` payload (hyprctl / mmsg both emit this
// shape, possibly fractional) into a rounded global pixel, or std::nullopt
// when it is not that shape. Pure, so it is unit-tested without a compositor.
std::optional<CursorPos> parseXyJson(const QByteArray &json);

// Abstract backend.
class CursorSource : public QObject {
    Q_OBJECT
public:
    explicit CursorSource(QObject *parent = nullptr) : QObject(parent) {}
    ~CursorSource() override = default;
    virtual void getCursor(CursorCallback cb) = 0;
    // Delivers a KWin script reply. Only the KWin backend acts on it; the
    // others ignore it, so the renderer can wire the controller's
    // cursorReported signal to the active source without a downcast.
    virtual void reportCursor(int /*x*/, int /*y*/) {}
};

// hyprctl / mmsg: run a CLI that prints the pointer as JSON and parse stdout.
class CliCursorSource : public CursorSource {
    Q_OBJECT
public:
    CliCursorSource(QString program, QStringList args,
                    QObject *parent = nullptr);
    void getCursor(CursorCallback cb) override;

private:
    QString program_;
    QStringList args_;
};

// KDE Wayland: round-trip through a KWin script over D-Bus. The script reads
// `workspace.cursorPos` and calls back into the overlay daemon's existing
// service (SendCursor), which forwards here via reportCursor(). A stale
// Wayland coordinate (clients can't read the live global cursor) is exactly
// what this avoids.
class KWinCursorSource : public CursorSource {
    Q_OBJECT
public:
    // scriptDir: where the helper KWin script file is materialised (an XDG
    // state dir). serviceName/objectPath/interfaceName: the daemon's own DBus
    // identity, which the script calls back into.
    KWinCursorSource(QString scriptDir, QString serviceName, QString objectPath,
                     QString interfaceName, QObject *parent = nullptr);
    void getCursor(CursorCallback cb) override;

    // Called by the daemon when the KWin script's SendCursor reply lands.
    void reportCursor(int x, int y) override;

private:
    void runScript(int id);
    // Unload a loaded KWin script by its D-Bus path. Used both on resolve (the
    // script that produced the reply) and on the timeout race where a script
    // finished loading after the query already gave up.
    void stopScript(const QString &path);
    void resolve(std::optional<CursorPos> pos);
    bool ensureScriptWritten();

    QString scriptDir_;
    QString scriptPath_;
    QString serviceName_;
    QString objectPath_;
    QString interfaceName_;
    bool scriptWritten_ = false;
    // DBus path of the script currently loaded in KWin, so resolve() can
    // unload it. Empty when none is loaded.
    QString currentScriptPath_;
    // Bounds the wait for the SendCursor reply; on timeout the query fails
    // (std::nullopt) and the renderer falls back to the grid.
    QTimer *timer_ = nullptr;
    CursorCallback pending_;
};

// Unsupported compositor: always fails, so the renderer keeps the grid
// position it would have used before this feature.
class NullCursorSource : public CursorSource {
    Q_OBJECT
public:
    explicit NullCursorSource(QObject *parent = nullptr)
        : CursorSource(parent) {}
    void getCursor(CursorCallback cb) override;
};

// Pick the backend for `desktop` (the normalised XDG_CURRENT_DESKTOP id).
// `kwinDeps` carries the daemon identity the KWin script calls back into and
// the script dir; ignored by the CLI / null backends. Returns a heap object
// parented to `parent`.
struct KWinDeps {
    QString scriptDir;
    QString serviceName;
    QString objectPath;
    QString interfaceName;
};
CursorSource *createCursorSource(const QString &xdgCurrentDesktop,
                                 const KWinDeps &kwinDeps, QObject *parent);

} // namespace schnelle_umlaute

#endif
