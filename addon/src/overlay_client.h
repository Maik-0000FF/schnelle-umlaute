#ifndef SCHNELLE_UMLAUTE_OVERLAY_CLIENT_H
#define SCHNELLE_UMLAUTE_OVERLAY_CLIENT_H

#include "overlay_lifecycle.h"

#include <fcitx-utils/dbus/bus.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fcitx {

// Sends show/hide requests to the standalone
// de.schnelle_umlaute.Overlay DBus service. The service may not be running;
// calls are fire-and-forget and a missing service is silently ignored so the
// addon works regardless of whether the overlay daemon is installed.
class OverlayClient {
public:
    OverlayClient();
    ~OverlayClient();

    void show(const std::vector<std::string> &variants, int currentIndex,
              const std::string &position);
    void hide();

    // Pokes the DBus service so a disabled-but-not-yet-running daemon is
    // activated. Used when the user enables the overlay in the editor so
    // the daemon is ready for the first cycling event.
    void start();

    // Asks the daemon to terminate. Sent when the user disables the
    // overlay so we don't leave an idle process behind.
    void quit();

    // Called each time the config is (re)loaded. Compares against the
    // last known enabled value and starts/stops the daemon accordingly.
    // The first call after construction is a no-op so the daemon isn't
    // eagerly spawned at fcitx5 startup.
    void applyEnabledTransition(bool enabled);

private:
    std::unique_ptr<dbus::Bus> bus_;
    std::optional<bool> lastEnabled_;
};

} // namespace fcitx

#endif
