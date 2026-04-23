#include "OverlayDBusClient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>

OverlayDBusClient::OverlayDBusClient(QObject *parent) : QObject(parent) {}

void OverlayDBusClient::sendTheme(const QString &theme) {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) return;
    auto *iface = bus.interface();
    if (!iface) return;
    if (!iface->isServiceRegistered(
            QStringLiteral("de.schnelle_umlaute.Overlay"))) {
        return;
    }
    auto msg = QDBusMessage::createMethodCall(
        QStringLiteral("de.schnelle_umlaute.Overlay"),
        QStringLiteral("/de/schnelle_umlaute/Overlay"),
        QStringLiteral("de.schnelle_umlaute.Overlay1"),
        QStringLiteral("SetTheme"));
    msg << theme;
    bus.asyncCall(msg);
}
