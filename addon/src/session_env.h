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

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

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

// Split on '\n' into individual lines, dropping the trailing empty field a
// file ending in '\n' would otherwise produce (so a clean file yields no
// phantom blank line).
inline std::vector<std::string> splitLines(const std::string &text) {
    std::vector<std::string> lines;
    std::string cur;
    for (const char c : text) {
        if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) {
        lines.push_back(cur);
    }
    return lines;
}

// Strip leading/trailing ASCII whitespace.
inline std::string trim(const std::string &s) {
    const char *ws = " \t\r\f\v";
    const auto begin = s.find_first_not_of(ws);
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = s.find_last_not_of(ws);
    return s.substr(begin, end - begin + 1);
}

} // namespace session_env_detail

// Result of merging an env snippet into an existing compositor config.
struct CompositorEnvMerge {
    // false → every snippet line is already present uncommented; the caller
    // should skip the write entirely (treat as success, nothing to do).
    bool changed;
    // Full new file content to write when `changed` is true. The existing
    // content is preserved verbatim and the snippet is appended as a labelled
    // block; empty when `changed` is false.
    std::string content;
};

// Idempotently merge `snippet` (newline-separated config lines, e.g. the
// Hyprland `env = KEY,VALUE` directives) into `existing` compositor-config
// content. Never rewrites or reorders the user's hand-maintained file: if any
// snippet line is missing, the *whole* labelled block is appended at the end;
// if all lines are already present uncommented, returns {false, ""}.
//
// Appending the full block even when only some lines are missing can leave a
// duplicate `env =` line, which every target compositor tolerates (last value
// wins) — preferred over trying to surgically edit the user's file.
inline CompositorEnvMerge mergeCompositorEnv(const std::string &existing,
                                             const std::string &snippet) {
    using session_env_detail::splitLines;
    using session_env_detail::trim;

    std::vector<std::string> present;
    for (const auto &raw : splitLines(existing)) {
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        present.push_back(line);
    }

    bool allPresent = true;
    for (const auto &raw : splitLines(snippet)) {
        const std::string want = trim(raw);
        if (want.empty()) {
            continue;
        }
        if (std::find(present.begin(), present.end(), want) == present.end()) {
            allPresent = false;
            break;
        }
    }
    if (allPresent) {
        return {false, ""};
    }

    std::string out = existing;
    if (!out.empty() && out.back() != '\n') {
        out.push_back('\n');
    }
    if (!out.empty()) {
        out.push_back('\n'); // blank line separating our block from theirs
    }
    out += "# schnelle-umlaute: fcitx5 input-method environment\n";
    out += snippet;
    if (out.empty() || out.back() != '\n') {
        out.push_back('\n');
    }
    return {true, out};
}

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
