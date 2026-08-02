#ifndef SCHNELLE_UMLAUTE_EDITOR_OVERLAY_DBUS_CLIENT_H
#define SCHNELLE_UMLAUTE_EDITOR_OVERLAY_DBUS_CLIENT_H

#include <QObject>
#include <QString>
#include <QVariantList>

class OverlayDBusClient : public QObject {
    Q_OBJECT
public:
    explicit OverlayDBusClient(QObject *parent = nullptr);

    // Sends SetTheme to the overlay daemon iff it is already on the bus.
    // We skip the call if the service is not registered so a theme switch
    // never silently activates the daemon for a user who never enabled
    // the overlay feature.
    void sendTheme(const QString &theme);

    // Tells the daemon its config on disk changed, so it re-reads and
    // re-derives instead of being handed each value. The daemon has to be able
    // to derive on its own anyway: when the desktop flips light/dark the editor
    // is usually not running, so it cannot push anything. Passing no arguments
    // is what keeps a future config key from needing another protocol bump.
    void sendReloadConfig();

private:
    // Fire a method at the daemon, or do nothing if it is not on the bus.
    void call(const QString &method, const QVariantList &args = {});
};

#endif
