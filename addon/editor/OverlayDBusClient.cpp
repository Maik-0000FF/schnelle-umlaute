#include "OverlayDBusClient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>

OverlayDBusClient::OverlayDBusClient(QObject *parent) : QObject(parent) {}

void OverlayDBusClient::call(const QString &method, const QVariantList &args) {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;
    auto *iface = bus.interface();
    if (!iface)
        return;
    if (!iface->isServiceRegistered(
            QStringLiteral("de.schnelle_umlaute.Overlay"))) {
        return;
    }
    auto msg = QDBusMessage::createMethodCall(
        QStringLiteral("de.schnelle_umlaute.Overlay"),
        QStringLiteral("/de/schnelle_umlaute/Overlay"),
        QStringLiteral("de.schnelle_umlaute.Overlay1"), method);
    msg.setArguments(args);
    bus.asyncCall(msg);
}

void OverlayDBusClient::sendTheme(const QString &theme) {
    call(QStringLiteral("SetTheme"), {theme});
}

void OverlayDBusClient::sendReloadConfig() {
    call(QStringLiteral("ReloadConfig"));
}
