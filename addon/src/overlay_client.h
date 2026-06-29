#ifndef SCHNELLE_UMLAUTE_OVERLAY_CLIENT_H
#define SCHNELLE_UMLAUTE_OVERLAY_CLIENT_H

#include "layer_shell_capability.h"
#include "overlay_lifecycle.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <fcitx-utils/dbus/bus.h>

namespace fcitx {

// Sends show/hide requests to the standalone
// de.schnelle_umlaute.Overlay DBus service. The service may not be running;
// calls are fire-and-forget and a missing service is silently ignored so the
// addon works regardless of whether the overlay daemon is installed.
class OverlayClient {
public:
    OverlayClient();
    ~OverlayClient();

    // label=true renders variants[0] as one full-width text (a profile-switch
    // name) instead of single-glyph accent cells.
    void show(const std::vector<std::string> &variants, int currentIndex,
              const std::string &position, bool label = false);
    void hide();

    // Starts the timing progress bar: a lead-in segment of leadMs (the
    // min-hold) followed by a window segment of windowMs (max - min). The
    // daemon animates it; sent right before show() when the gesture begins.
    void setProgress(int leadMs, int windowMs);
    // Freezes the progress bar in place (called when a leader press starts
    // cycling, so the bar holds at the moment the window was caught).
    void freezeProgress();

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
    // Compositor check sampled once at construction. On sessions without
    // wlr-layer-shell support (GNOME, X11) the overlay daemon would spawn
    // but fail to cycle, so we short-circuit show()/start() here.
    LayerShellCapability capability_;
};

} // namespace fcitx

#endif
