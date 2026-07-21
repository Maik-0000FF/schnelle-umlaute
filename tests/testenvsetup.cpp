// Unit tests for EnvSetup::isConfigured() — the Plasma-aware "is the fcitx5
// input-method environment set up?" check that consumes session_env.h.
// isConfigured() reads only the process environment (no files), so the whole
// matrix is driven with setenv().

#include "EnvSetup.h"

#include <cstdio>
#include <cstdlib>

#define EXPECT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,       \
                         #cond);                                               \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

// Set (or clear, on nullptr) the three input-method variables isConfigured()
// inspects.
static void setImEnv(const char *gtk, const char *qt, const char *xmod) {
    gtk ? (void)setenv("GTK_IM_MODULE", gtk, 1) : (void)unsetenv("GTK_IM_MODULE");
    qt ? (void)setenv("QT_IM_MODULE", qt, 1) : (void)unsetenv("QT_IM_MODULE");
    xmod ? (void)setenv("XMODIFIERS", xmod, 1) : (void)unsetenv("XMODIFIERS");
}

// Set the session/desktop that decide whether the modules are needed.
static void setSession(const char *type, const char *desktop) {
    setenv("XDG_SESSION_TYPE", type, 1);
    setenv("XDG_CURRENT_DESKTOP", desktop, 1);
}

// Non-KDE (GNOME Wayland) needs the full module set: all three present is
// configured, any missing one is not.
void testGnomeRequiresFullSet() {
    EnvSetup env;
    setSession("wayland", "ubuntu:GNOME");
    setImEnv("fcitx", "fcitx", "@im=fcitx");
    EXPECT(env.isConfigured());
    setImEnv(nullptr, "fcitx", "@im=fcitx"); // GTK missing
    EXPECT(!env.isConfigured());
    setImEnv("fcitx", "fcitx", nullptr); // XMODIFIERS missing
    EXPECT(!env.isConfigured());
}

// KDE Plasma Wayland is configured by XMODIFIERS alone (the reduced set), and a
// legacy full environment still qualifies; a missing XMODIFIERS never does.
void testKdeWaylandXmodifiersAlone() {
    EnvSetup env;
    setSession("wayland", "KDE");
    setImEnv(nullptr, nullptr, "@im=fcitx"); // reduced: no GTK/QT
    EXPECT(env.isConfigured());
    setImEnv("fcitx", "fcitx", "@im=fcitx"); // legacy full still valid
    EXPECT(env.isConfigured());
    setImEnv("fcitx", "fcitx", nullptr); // XMODIFIERS missing
    EXPECT(!env.isConfigured());
}

// KDE X11 has no native text-input protocol, so its apps still need the full
// set — XMODIFIERS alone is not enough there.
void testKdeX11RequiresFullSet() {
    EnvSetup env;
    setSession("x11", "KDE");
    setImEnv(nullptr, nullptr, "@im=fcitx"); // reduced is not enough on X11
    EXPECT(!env.isConfigured());
    setImEnv("fcitx", "fcitx", "@im=fcitx");
    EXPECT(env.isConfigured());
}

int main() {
    testGnomeRequiresFullSet();
    testKdeWaylandXmodifiersAlone();
    testKdeX11RequiresFullSet();
    std::puts("testenvsetup: all passed");
    return 0;
}
