#include "overlay_client.h"
#include "overlay_protocol.h"

#include <cstdint>
#include <fcitx-utils/dbus/message.h>
#include <fcitx-utils/log.h>

namespace fcitx {

namespace {
constexpr const char *kService = "de.schnelle_umlaute.Overlay";
constexpr const char *kPath = "/de/schnelle_umlaute/Overlay";
constexpr const char *kInterface = "de.schnelle_umlaute.Overlay1";
// Timeout for the synchronous version handshake in start(). It runs off the hot
// path (once per enable transition), and only when a daemon already owns the
// name, so it never triggers activation and a healthy daemon replies in well
// under this bound.
constexpr uint64_t kHandshakeTimeoutUsec = 200'000;
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
                         int currentIndex, const std::string &position,
                         bool label) {
    if (!capability_.supported)
        return;
    if (!bus_ || !bus_->isOpen() || variants.empty())
        return;
    auto msg = bus_->createMethodCall(kService, kPath, kInterface, "Show");
    msg << variants << int32_t(currentIndex) << position << label;
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

void OverlayClient::setProgress(int leadMs, int windowMs, uint64_t startUsec) {
    if (!capability_.supported)
        return;
    if (!bus_ || !bus_->isOpen())
        return;
    auto msg =
        bus_->createMethodCall(kService, kPath, kInterface, "SetProgress");
    msg << int32_t(leadMs) << int32_t(windowMs) << int64_t(startUsec);
    msg.send();
    bus_->flush();
}

void OverlayClient::freezeProgress() {
    if (!capability_.supported)
        return;
    if (!bus_ || !bus_->isOpen())
        return;
    auto msg =
        bus_->createMethodCall(kService, kPath, kInterface, "FreezeProgress");
    msg.send();
    bus_->flush();
}

void OverlayClient::quitStaleDaemon() {
    // GetNameOwner: is a daemon already running? This does not activate one, so
    // when nobody owns the name we do nothing and let the Hide poke in start()
    // activate the freshly installed binary.
    const std::string owner = bus_->serviceOwner(kService, kHandshakeTimeoutUsec);
    bool gotVersion = false;
    int reported = -1;
    if (!owner.empty()) {
        auto query = bus_->createMethodCall(kService, kPath, kInterface,
                                            "GetProtocolVersion");
        auto reply = query.call(kHandshakeTimeoutUsec);
        if (!reply.isError()) {
            int32_t v = 0;
            reply >> v;
            reported = v;
            gotVersion = true;
        }
    }
    if (overlayDaemonIsStale(!owner.empty(), gotVersion, reported,
                             schnelle_umlaute::kOverlayProtocolVersion)) {
        FCITX_INFO() << "Schnelle: overlay daemon protocol mismatch (daemon "
                     << (gotVersion ? std::to_string(reported)
                                    : std::string("pre-handshake"))
                     << ", engine "
                     << schnelle_umlaute::kOverlayProtocolVersion
                     << "); restarting it";
        quit();
    }
}

void OverlayClient::start() {
    if (!capability_.supported)
        return;
    if (!bus_ || !bus_->isOpen())
        return;
    // Replace a stale daemon (an old build still owning the name after an
    // in-place upgrade) first, so the poke below brings up the freshly
    // installed binary instead of feeding one that can't parse our current
    // calls. After a stale quit the fresh daemon activates on the next real
    // call (this Hide may still land on the exiting old one).
    quitStaleDaemon();
    // Sends a no-op Hide to the service name. DBus sees the call and, if
    // the daemon isn't already running, activates it via the .service file.
    // If the daemon is already running, Hide is idempotent.
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
