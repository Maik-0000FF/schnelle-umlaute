#include "SingleInstance.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QQuickWindow>

namespace {

// Reverse-domain DBus identifier matching the overlay daemon's naming
// (de.schnelle_umlaute.Overlay1). The `1` is the major-version suffix
// recommended by the freedesktop spec so a future incompatible API
// change can coexist with this one.
constexpr auto kServiceName = "de.schnelle_umlaute.Editor1";
constexpr auto kObjectPath = "/Editor";
constexpr auto kInterfaceName = "de.schnelle_umlaute.Editor1";

} // namespace

SingleInstanceAdaptor::SingleInstanceAdaptor(QQuickWindow *window)
    : QDBusAbstractAdaptor(window), window_(window) {}

void SingleInstanceAdaptor::Raise() {
    if (!window_)
        return;
    window_->show();
    window_->raise();
    window_->requestActivate();
}

namespace SingleInstance {

bool acquireOrRaise() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        // No session bus → run unguarded. Better to have a usable
        // editor in headless edge cases than to refuse to start.
        return true;
    }
    if (bus.registerService(QString::fromLatin1(kServiceName))) {
        // Race won — we are the canonical editor.
        return true;
    }
    // Race lost — another editor owns the name. Ask it to surface and
    // tell the caller to bail. Fire-and-forget: we do not wait for a
    // reply, the running instance might be on a slow event loop and
    // we have nothing useful to do with the reply anyway.
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kServiceName), QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName), QStringLiteral("Raise"));
    bus.send(msg);
    return false;
}

void registerOnWindow(QQuickWindow *window) {
    if (!window)
        return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;
    // Adaptor parented to the window so its lifetime tracks the UI.
    // registerObject takes a non-owning pointer to the window; the
    // adaptor's QObject child relationship handles cleanup.
    new SingleInstanceAdaptor(window);
    bus.registerObject(QString::fromLatin1(kObjectPath), window);
}

} // namespace SingleInstance
