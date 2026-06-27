#include "overlay_client.h"

#include <fcitx-utils/dbus/message.h>
#include <fcitx-utils/log.h>

namespace fcitx {

namespace {
constexpr const char *kService = "de.schnelle_umlaute.Overlay";
constexpr const char *kPath = "/de/schnelle_umlaute/Overlay";
constexpr const char *kInterface = "de.schnelle_umlaute.Overlay1";
} // namespace

OverlayClient::OverlayClient() : capability_(detectLayerShellCapability()) {
    if (!capability_.supported) {
        FCITX_INFO() << "Schnelle: Overlay disabled — session "
                     << capability_.session << ": " << capability_.reason;
    }
    try {
        bus_ = std::make_unique<dbus::Bus>(dbus::BusType::Session);
    } catch (...) {
        bus_.reset();
    }
}

OverlayClient::~OverlayClient() = default;

void OverlayClient::show(const std::vector<std::string> &variants,
                         int currentIndex, const std::string &position) {
    if (!capability_.supported)
        return;
    if (!bus_ || !bus_->isOpen() || variants.empty())
        return;
    auto msg = bus_->createMethodCall(kService, kPath, kInterface, "Show");
    msg << variants << int32_t(currentIndex) << position;
    // Fire-and-forget. flush() is needed because the bus isn't attached to
    // fcitx5's event loop — without it messages pile up in the send buffer
    // and the daemon never sees them.
    msg.send();
    bus_->flush();
}

void OverlayClient::hide() {
    if (!bus_ || !bus_->isOpen())
        return;
    auto msg = bus_->createMethodCall(kService, kPath, kInterface, "Hide");
    msg.send();
    bus_->flush();
}

void OverlayClient::setProgress(int leadMs, int windowMs) {
    if (!capability_.supported)
        return;
    if (!bus_ || !bus_->isOpen())
        return;
    auto msg =
        bus_->createMethodCall(kService, kPath, kInterface, "SetProgress");
    msg << int32_t(leadMs) << int32_t(windowMs);
    msg.send();
    bus_->flush();
}

void OverlayClient::freezeProgress() {
    if (!bus_ || !bus_->isOpen())
        return;
    auto msg =
        bus_->createMethodCall(kService, kPath, kInterface, "FreezeProgress");
    msg.send();
    bus_->flush();
}

void OverlayClient::start() {
    // Sends a no-op Hide to the service name. DBus sees the call and, if
    // the daemon isn't already running, activates it via the .service file.
    // If the daemon is already running, Hide is idempotent.
    if (!capability_.supported)
        return;
    if (!bus_ || !bus_->isOpen())
        return;
    auto msg = bus_->createMethodCall(kService, kPath, kInterface, "Hide");
    msg.send();
    bus_->flush();
}

void OverlayClient::quit() {
    if (!bus_ || !bus_->isOpen())
        return;
    auto msg = bus_->createMethodCall(kService, kPath, kInterface, "Quit");
    msg.send();
    bus_->flush();
}

void OverlayClient::applyEnabledTransition(bool enabled) {
    switch (decideOverlayLifecycleAction(lastEnabled_, enabled)) {
    case OverlayLifecycleAction::Start:
        start();
        break;
    case OverlayLifecycleAction::Quit:
        quit();
        break;
    case OverlayLifecycleAction::None:
        break;
    }
    lastEnabled_ = enabled;
}

} // namespace fcitx
