#ifndef SCHNELLE_UMLAUTE_OVERLAY_CLIENT_H
#define SCHNELLE_UMLAUTE_OVERLAY_CLIENT_H

#include <fcitx-utils/dbus/bus.h>
#include <memory>
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
              const std::string &position, int cursorX, int cursorY);
    void hide();

private:
    std::unique_ptr<dbus::Bus> bus_;
};

} // namespace fcitx

#endif
