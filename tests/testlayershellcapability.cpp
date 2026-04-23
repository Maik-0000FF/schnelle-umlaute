// Unit tests for checkLayerShellCapability().
// Pure header-only logic — verifies the rule table that decides whether
// the overlay's wlr-layer-shell window can work in the current session.

#include "src/layer_shell_capability.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using fcitx::checkLayerShellCapability;

#define EXPECT(cond) do {                                                   \
    if (!(cond)) {                                                          \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);\
        std::abort();                                                       \
    }                                                                       \
} while (0)

// KDE Plasma on Wayland is the reference target. KWin ships wlr-layer-shell.
void testKdeWaylandIsSupported() {
    auto cap = checkLayerShellCapability("wayland", "KDE");
    EXPECT(cap.supported);
    EXPECT(cap.reason.empty());
}

// GNOME's Mutter refuses to implement wlr-layer-shell. The reason string
// must mention GNOME so the editor can surface it.
void testGnomeWaylandIsUnsupported() {
    auto cap = checkLayerShellCapability("wayland", "GNOME");
    EXPECT(!cap.supported);
    EXPECT(cap.reason.find("GNOME") != std::string::npos);
}

// Ubuntu ships XDG_CURRENT_DESKTOP="ubuntu:GNOME". Substring match must
// still catch the GNOME token.
void testUbuntuGnomeIsUnsupported() {
    auto cap = checkLayerShellCapability("wayland", "ubuntu:GNOME");
    EXPECT(!cap.supported);
}

// X11 sessions can't host layer-shell surfaces regardless of DE.
void testX11IsUnsupported() {
    auto cap = checkLayerShellCapability("x11", "KDE");
    EXPECT(!cap.supported);
    EXPECT(cap.reason.find("Wayland") != std::string::npos);
}

void testX11XfceIsUnsupported() {
    auto cap = checkLayerShellCapability("x11", "XFCE");
    EXPECT(!cap.supported);
}

// wlroots-based compositors: sway, Hyprland, river, wayfire.
void testSwayIsSupported() {
    auto cap = checkLayerShellCapability("wayland", "sway");
    EXPECT(cap.supported);
}

void testHyprlandIsSupported() {
    auto cap = checkLayerShellCapability("wayland", "Hyprland");
    EXPECT(cap.supported);
}

void testRiverIsSupported() {
    auto cap = checkLayerShellCapability("wayland", "river");
    EXPECT(cap.supported);
}

void testWayfireIsSupported() {
    auto cap = checkLayerShellCapability("wayland", "wayfire");
    EXPECT(cap.supported);
}

// Unknown Wayland compositor: conservative default — report unsupported
// so users aren't left wondering why cycling is broken.
void testUnknownWaylandIsUnsupported() {
    auto cap = checkLayerShellCapability("wayland", "MysteryWM");
    EXPECT(!cap.supported);
    EXPECT(!cap.reason.empty());
}

// Env vars missing entirely (e.g. headless CI, detached shell).
void testNullEnvIsUnsupported() {
    auto cap = checkLayerShellCapability(nullptr, nullptr);
    EXPECT(!cap.supported);
}

void testEmptyEnvIsUnsupported() {
    auto cap = checkLayerShellCapability("", "");
    EXPECT(!cap.supported);
}

// Case-insensitive matching: some compositors capitalize oddly
// (e.g. "KDE" vs "kde"). Both should be accepted.
void testLowercaseKdeIsSupported() {
    auto cap = checkLayerShellCapability("wayland", "kde");
    EXPECT(cap.supported);
}

// Session string surfaced in the editor must include both DE and protocol.
void testSessionStringIncludesProtocol() {
    auto cap1 = checkLayerShellCapability("wayland", "GNOME");
    EXPECT(cap1.session.find("GNOME") != std::string::npos);
    EXPECT(cap1.session.find("Wayland") != std::string::npos);

    auto cap2 = checkLayerShellCapability("x11", "XFCE");
    EXPECT(cap2.session.find("XFCE") != std::string::npos);
    EXPECT(cap2.session.find("X11") != std::string::npos);
}

int main() {
    testKdeWaylandIsSupported();
    testGnomeWaylandIsUnsupported();
    testUbuntuGnomeIsUnsupported();
    testX11IsUnsupported();
    testX11XfceIsUnsupported();
    testSwayIsSupported();
    testHyprlandIsSupported();
    testRiverIsSupported();
    testWayfireIsSupported();
    testUnknownWaylandIsUnsupported();
    testNullEnvIsUnsupported();
    testEmptyEnvIsUnsupported();
    testLowercaseKdeIsSupported();
    testSessionStringIncludesProtocol();
    std::fprintf(stderr, "testlayershellcapability: all tests passed\n");
    return 0;
}
