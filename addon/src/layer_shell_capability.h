#ifndef SCHNELLE_UMLAUTE_LAYER_SHELL_CAPABILITY_H
#define SCHNELLE_UMLAUTE_LAYER_SHELL_CAPABILITY_H

// Pure detection for wlr-layer-shell support in the current session.
// Deliberately free of Qt/DBus/fcitx5 deps so it's unit-testable and
// reusable by both the addon (overlay_client) and the standalone editor
// (SettingsModel).

#include <cstdlib>
#include <cstring>
#include <string>

namespace fcitx {

struct LayerShellCapability {
    bool supported;
    // Human-readable session descriptor, e.g. "GNOME (Wayland)" or
    // "XFCE (X11)". Shown in the editor so the user understands why the
    // overlay is unavailable.
    std::string session;
    // Empty when supported is true. Otherwise explains the limitation in
    // one sentence.
    std::string reason;
};

namespace detail {

// Case-insensitive substring match. `haystack` is colon-separated per
// xdg-desktop-spec (e.g. "ubuntu:GNOME"), but we just scan the whole
// string — the needles we care about don't collide.
inline bool containsCI(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    const auto hlen = std::strlen(haystack);
    const auto nlen = std::strlen(needle);
    if (nlen == 0 || nlen > hlen) return false;
    for (std::size_t i = 0; i + nlen <= hlen; ++i) {
        std::size_t j = 0;
        for (; j < nlen; ++j) {
            const char a = haystack[i + j];
            const char b = needle[j];
            const char la =
                (a >= 'A' && a <= 'Z') ? char(a - 'A' + 'a') : a;
            const char lb =
                (b >= 'A' && b <= 'Z') ? char(b - 'A' + 'a') : b;
            if (la != lb) break;
        }
        if (j == nlen) return true;
    }
    return false;
}

} // namespace detail

// Decide whether the current session supports wlr-layer-shell.
//
// `sessionType`  — content of XDG_SESSION_TYPE  ("wayland", "x11", ...)
// `currentDesktop` — content of XDG_CURRENT_DESKTOP ("KDE", "ubuntu:GNOME", ...)
//
// Both may be nullptr or empty (env var not set). The check is
// conservative: an unknown Wayland compositor is reported as
// unsupported rather than guessing.
inline LayerShellCapability
checkLayerShellCapability(const char *sessionType,
                          const char *currentDesktop) {
    using detail::containsCI;

    const std::string desktop =
        (currentDesktop && *currentDesktop) ? currentDesktop : "unknown";
    const bool isWayland =
        sessionType && std::strcmp(sessionType, "wayland") == 0;
    const bool isX11 =
        sessionType && std::strcmp(sessionType, "x11") == 0;

    LayerShellCapability cap{false, "", ""};
    cap.session = desktop;
    if (isWayland) {
        cap.session += " (Wayland)";
    } else if (isX11) {
        cap.session += " (X11)";
    }

    if (!isWayland) {
        cap.reason =
            "wlr-layer-shell is a Wayland-only protocol. "
            "X11 sessions cannot host the overlay.";
        return cap;
    }

    // GNOME's Mutter refuses to implement wlr-layer-shell upstream, so
    // the overlay window falls back to a regular toplevel and cycling
    // updates get dropped. Fail fast.
    if (containsCI(currentDesktop, "GNOME") ||
        containsCI(currentDesktop, "Unity")) {
        cap.reason =
            "GNOME/Mutter does not implement wlr-layer-shell. "
            "The overlay will not cycle correctly.";
        return cap;
    }

    // Compositors with known wlr-layer-shell support.
    const char *supportedDesktops[] = {
        "KDE",      // KWin
        "sway",
        "Hyprland",
        "river",
        "wayfire",
        "niri",
        "Miracle-WM",
        "LabWC",
    };
    for (const auto *d : supportedDesktops) {
        if (containsCI(currentDesktop, d)) {
            cap.supported = true;
            return cap;
        }
    }

    // Unknown compositor on Wayland — be conservative. The overlay may
    // actually work (wlroots-based compositors often do), but we'd
    // rather let the user know than fail silently.
    cap.reason =
        "Compositor is not known to implement wlr-layer-shell. "
        "Tested: KDE Plasma, sway, Hyprland, river, wayfire.";
    return cap;
}

// Convenience wrapper that reads the environment once.
inline LayerShellCapability detectLayerShellCapability() {
    return checkLayerShellCapability(std::getenv("XDG_SESSION_TYPE"),
                                     std::getenv("XDG_CURRENT_DESKTOP"));
}

} // namespace fcitx

#endif
