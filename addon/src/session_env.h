#ifndef SCHNELLE_UMLAUTE_SESSION_ENV_H
#define SCHNELLE_UMLAUTE_SESSION_ENV_H

// Pure classification of how the current session delivers input-method
// environment variables to applications. Qt/DBus/fcitx5-free so it's
// unit-testable and shared by the standalone editor (EnvSetup).
//
// Background: the editor writes ~/.config/environment.d/fcitx5.conf and,
// when the variables are not yet active, tells the user to log out and
// back in. That advice is only correct for sessions whose graphical
// target is started by the systemd user manager (display-manager logins,
// uwsm) — those import environment.d. A compositor launched straight from
// a TTY (`exec Hyprland` in a login shell, plain `sway`, ...) never
// inherits environment.d, so relogin changes nothing and the setup dialog
// reappears forever. For those sessions the variables belong in the
// compositor's own configuration instead.

#include <cstdlib>
#include <cstring>
#include <string>

namespace fcitx {

enum class EnvMechanism {
    // Session imports ~/.config/environment.d at login (display manager,
    // uwsm). Writing the drop-in + relogin activates the variables.
    EnvironmentD,
    // Compositor started outside the systemd graphical session (TTY
    // `exec`). environment.d is not honored; variables must go into the
    // compositor configuration.
    CompositorConf,
};

struct SessionEnvInfo {
    EnvMechanism mechanism;
    // Human-readable descriptor, e.g. "Hyprland (Wayland)".
    std::string session;
    // Ready-to-paste lines for CompositorConf sessions. Empty for
    // EnvironmentD.
    std::string snippet;
    // Compositor config file the snippet belongs in, e.g.
    // "~/.config/hypr/hyprland.conf". Empty when not known precisely.
    std::string configPath;
};

namespace session_env_detail {

// Case-insensitive substring match. XDG_CURRENT_DESKTOP is colon-
// separated per xdg-desktop-spec (e.g. "ubuntu:GNOME"); scanning the
// whole string is fine because the needles we look for don't collide.
inline bool containsCI(const char *haystack, const char *needle) {
    if (!haystack || !needle) {
        return false;
    }
    const auto hlen = std::strlen(haystack);
    const auto nlen = std::strlen(needle);
    if (nlen == 0 || nlen > hlen) {
        return false;
    }
    for (std::size_t i = 0; i + nlen <= hlen; ++i) {
        std::size_t j = 0;
        for (; j < nlen; ++j) {
            const char a = haystack[i + j];
            const char b = needle[j];
            const char la = (a >= 'A' && a <= 'Z') ? char(a - 'A' + 'a') : a;
            const char lb = (b >= 'A' && b <= 'Z') ? char(b - 'A' + 'a') : b;
            if (la != lb) {
                break;
            }
        }
        if (j == nlen) {
            return true;
        }
    }
    return false;
}

} // namespace session_env_detail

// Classify how `currentDesktop` (XDG_CURRENT_DESKTOP) delivers env vars.
// `sessionType` (XDG_SESSION_TYPE) is used only to label the descriptor.
//
// Heuristic by the compositor's *default* launch method: wlroots-family
// compositors are overwhelmingly started from a TTY, so they map to
// CompositorConf. Everything else — and the unknown/empty case — maps to
// EnvironmentD, preserving the original relogin advice. A misclassified
// uwsm/DM launch is harmless: the dialog text names both remedies, and if
// the variables were actually active the dialog never opens.
inline SessionEnvInfo classifySessionEnv(const char *sessionType,
                                         const char *currentDesktop) {
    using session_env_detail::containsCI;

    SessionEnvInfo info{EnvMechanism::EnvironmentD,
                        (currentDesktop && *currentDesktop) ? currentDesktop
                                                            : "unknown",
                        "", ""};
    if (sessionType && std::strcmp(sessionType, "wayland") == 0) {
        info.session += " (Wayland)";
    } else if (sessionType && std::strcmp(sessionType, "x11") == 0) {
        info.session += " (X11)";
    }

    if (containsCI(currentDesktop, "Hyprland")) {
        info.mechanism = EnvMechanism::CompositorConf;
        info.configPath = "~/.config/hypr/hyprland.conf";
        info.snippet = "env = GTK_IM_MODULE,fcitx\n"
                       "env = QT_IM_MODULE,fcitx\n"
                       "env = XMODIFIERS,@im=fcitx";
        return info;
    }

    // Other wlroots compositors with no single config syntax for env
    // vars. Show the neutral variable list; the dialog explains they must
    // be exported before the compositor starts (or launch via uwsm).
    if (containsCI(currentDesktop, "sway") ||
        containsCI(currentDesktop, "river") ||
        containsCI(currentDesktop, "niri") ||
        containsCI(currentDesktop, "wayfire") ||
        containsCI(currentDesktop, "Hikari") ||
        containsCI(currentDesktop, "LabWC")) {
        info.mechanism = EnvMechanism::CompositorConf;
        info.snippet = "GTK_IM_MODULE=fcitx\n"
                       "QT_IM_MODULE=fcitx\n"
                       "XMODIFIERS=@im=fcitx";
        return info;
    }

    return info;
}

// Convenience wrapper that reads the environment once.
inline SessionEnvInfo detectSessionEnv() {
    return classifySessionEnv(std::getenv("XDG_SESSION_TYPE"),
                              std::getenv("XDG_CURRENT_DESKTOP"));
}

} // namespace fcitx

#endif
