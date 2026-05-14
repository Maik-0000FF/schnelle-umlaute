#ifndef SCHNELLE_UMLAUTE_EDITOR_SINGLE_INSTANCE_H
#define SCHNELLE_UMLAUTE_EDITOR_SINGLE_INSTANCE_H

#include <QDBusAbstractAdaptor>
#include <QObject>

class QQuickWindow;

// DBus adaptor exposed by the running editor so that a second
// `schnelle-umlaute-editor` invocation can hand control back to the
// existing window instead of opening a duplicate UI.
class SingleInstanceAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "de.schnelle_umlaute.Editor1")

public:
    explicit SingleInstanceAdaptor(QQuickWindow *window);

public Q_SLOTS:
    // Surface the existing editor window: show it (in case it was
    // minimised), raise it above sibling windows, and ask the window
    // manager to give it keyboard focus. Wayland compositors honour
    // requestActivate() only as a hint; KWin and Mutter do bring the
    // window to the foreground in practice when the request originates
    // from a process the compositor can identify.
    void Raise();

private:
    QQuickWindow *window_;
};

namespace SingleInstance {

// Attempt to claim the editor's DBus service name on the session bus.
// Outcome:
//   true  → no other editor is running; caller should proceed to set
//           up the QML window and then register the adaptor on it
//           (see registerOnWindow()).
//   false → another editor instance already owns the name; a Raise()
//           call has been dispatched to it on the user's behalf and
//           the caller should exit with status 0 before creating any
//           UI of its own.
//
// If the session bus is not available (e.g. headless or ssh without
// DBUS_SESSION_BUS_ADDRESS), this function returns true so the editor
// remains usable in that mode — at the cost of the single-instance
// guarantee.
bool acquireOrRaise();

// Once the QML window exists in the first-instance code path, hand it
// to a fresh adaptor and publish the object on the session bus so
// later Raise() calls reach it. The adaptor is parented to the window
// and is cleaned up when the window is destroyed.
void registerOnWindow(QQuickWindow *window);

} // namespace SingleInstance

#endif
