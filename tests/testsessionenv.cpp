// Unit tests for classifySessionEnv().
// Pure header-only logic — verifies the rule table that decides whether
// the editor tells the user to log out (environment.d is imported) or to
// edit the compositor configuration (TTY-launched compositor that never
// reads environment.d).

#include "src/session_env.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using fcitx::classifySessionEnv;
using fcitx::EnvMechanism;

#define EXPECT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,       \
                         #cond);                                               \
            std::abort();                                                      \
        }                                                                      \
    } while (0)

// Hyprland is started from a TTY by default — environment.d is not read,
// so the variables belong in hyprland.conf as `env = KEY,VALUE` lines.
void testHyprlandIsCompositorConf() {
    auto info = classifySessionEnv("wayland", "Hyprland");
    EXPECT(info.mechanism == EnvMechanism::CompositorConf);
    EXPECT(info.session == "Hyprland (Wayland)");
    EXPECT(info.configPath == "~/.config/hypr/hyprland.conf");
    EXPECT(info.snippet.find("env = GTK_IM_MODULE,fcitx") != std::string::npos);
    EXPECT(info.snippet.find("env = QT_IM_MODULE,fcitx") != std::string::npos);
    EXPECT(info.snippet.find("env = XMODIFIERS,@im=fcitx") !=
           std::string::npos);
}

// Other wlroots compositors share the CompositorConf verdict but have no
// single config syntax, so the neutral KEY=VALUE list is shown and no
// config path is claimed.
void testSwayIsCompositorConfWithGenericSnippet() {
    auto info = classifySessionEnv("wayland", "sway");
    EXPECT(info.mechanism == EnvMechanism::CompositorConf);
    EXPECT(info.configPath.empty());
    EXPECT(info.snippet.find("GTK_IM_MODULE=fcitx") != std::string::npos);
    EXPECT(info.snippet.find("env =") == std::string::npos);
}

void testRiverIsCompositorConf() {
    EXPECT(classifySessionEnv("wayland", "river").mechanism ==
           EnvMechanism::CompositorConf);
}

void testNiriIsCompositorConf() {
    EXPECT(classifySessionEnv("wayland", "niri").mechanism ==
           EnvMechanism::CompositorConf);
}

// KDE Plasma logs in through SDDM, which goes through the systemd
// graphical session and imports environment.d → relogin works.
void testKdeIsEnvironmentD() {
    auto info = classifySessionEnv("wayland", "KDE");
    EXPECT(info.mechanism == EnvMechanism::EnvironmentD);
    EXPECT(info.snippet.empty());
    EXPECT(info.configPath.empty());
    EXPECT(info.session == "KDE (Wayland)");
}

// Ubuntu ships XDG_CURRENT_DESKTOP="ubuntu:GNOME"; substring match must
// not misfire and GNOME stays on the environment.d path.
void testGnomeIsEnvironmentD() {
    EXPECT(classifySessionEnv("x11", "ubuntu:GNOME").mechanism ==
           EnvMechanism::EnvironmentD);
}

// Unset desktop is the conservative default: keep the original relogin
// advice rather than guessing at compositor config.
void testUnsetDesktopDefaultsToEnvironmentD() {
    auto info = classifySessionEnv(nullptr, nullptr);
    EXPECT(info.mechanism == EnvMechanism::EnvironmentD);
    EXPECT(info.session == "unknown");
}

// Session-type only affects the descriptor suffix, never the verdict.
void testSessionTypeOnlyLabels() {
    EXPECT(classifySessionEnv("x11", "KDE").session == "KDE (X11)");
    EXPECT(classifySessionEnv(nullptr, "KDE").session == "KDE");
}

int main() {
    testHyprlandIsCompositorConf();
    testSwayIsCompositorConfWithGenericSnippet();
    testRiverIsCompositorConf();
    testNiriIsCompositorConf();
    testKdeIsEnvironmentD();
    testGnomeIsEnvironmentD();
    testUnsetDesktopDefaultsToEnvironmentD();
    testSessionTypeOnlyLabels();
    std::puts("testsessionenv: all passed");
    return 0;
}
