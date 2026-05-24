#ifndef SCHNELLE_UMLAUTE_EDITOR_ENV_SETUP_H
#define SCHNELLE_UMLAUTE_EDITOR_ENV_SETUP_H

#include <QObject>
#include <QString>

// Detects whether the fcitx5 input-method environment variables are set
// up for the current user, and writes the canonical environment.d
// drop-in when the editor first runs without them.
//
// The check is invoked on every editor start — not just first run — so
// that a user who wipes the file, or whose installation gets shadowed
// by another input method, is reminded before they edit mappings that
// would otherwise have no effect anywhere.
class EnvSetup : public QObject {
    Q_OBJECT

public:
    explicit EnvSetup(QObject *parent = nullptr);

    // True when GTK_IM_MODULE, QT_IM_MODULE and XMODIFIERS in the
    // current process environment all carry the fcitx values. Reads
    // the process environment, which on systemd-managed logins
    // reflects the contents of ~/.config/environment.d/*.conf at
    // session start.
    Q_INVOKABLE bool isConfigured() const;

    // Writes ~/.config/environment.d/fcitx5.conf with the canonical
    // four-line content (GTK_IM_MODULE / QT_IM_MODULE / XMODIFIERS /
    // GLFW_IM_MODULE). Overwrites unconditionally — any pre-existing
    // deviation would be wrong by definition for this addon.
    //
    // Returns true on success, false on filesystem error (parent dir
    // uncreatable, write permission denied, etc.).
    Q_INVOKABLE bool writeConfig();

    // Absolute path of the file writeConfig() targets. Exposed so the
    // QML dialog can display it as part of the explanation instead of
    // hard-coding the path on the QML side.
    Q_INVOKABLE QString configPath() const;

    // True when the config file on disk already contains the three
    // canonical fcitx variables (GTK_IM_MODULE / QT_IM_MODULE /
    // XMODIFIERS). This is used together with isConfigured() to
    // distinguish two failure modes:
    //   isConfigured() == false && hasValidConfigFile() == false
    //     → first-run state: dialog offers "Set up now"
    //   isConfigured() == false && hasValidConfigFile() == true
    //     → user already ran setup but has not logged out/in yet, so
    //       the file is correct but environment.d hasn't been re-read
    //       in this session. Dialog explains the logout requirement
    //       instead of offering setup again, which would be confusing.
    Q_INVOKABLE bool hasValidConfigFile() const;

    // True when this session imports ~/.config/environment.d at login
    // (display-manager / uwsm launch). False for compositors started
    // straight from a TTY (e.g. `exec Hyprland`), where the drop-in is
    // never read and logging out does not activate the variables — those
    // need the variables in the compositor configuration instead. Used to
    // pick between the "logout pending" and "compositor config" dialogs.
    Q_INVOKABLE bool honorsEnvironmentD() const;

    // Human-readable session descriptor for the compositor-config dialog,
    // e.g. "Hyprland (Wayland)".
    Q_INVOKABLE QString sessionName() const;

    // Ready-to-paste environment lines for the detected compositor, or an
    // empty string on environment.d sessions. For Hyprland these are
    // `env = KEY,VALUE` directives; for other wlroots compositors a plain
    // KEY=VALUE list.
    Q_INVOKABLE QString compositorEnvSnippet() const;

    // Config file the snippet belongs in (e.g.
    // "~/.config/hypr/hyprland.conf"), or empty when not known precisely
    // and the dialog should give generic guidance.
    Q_INVOKABLE QString compositorConfigPath() const;

    // Idempotently appends the detected compositor's env snippet to its
    // config file (the one compositorConfigPath() names). Preserves the
    // user's existing content verbatim and only adds a labelled block when
    // the lines are not already present. Creates the file and parent
    // directory if missing. A session restart is still required afterwards
    // for the compositor to export the variables — the caller must say so.
    //
    // Returns true on success (including the no-op case where the lines were
    // already present), false when there is no known config path for this
    // compositor (e.g. sway/river) or on filesystem error.
    Q_INVOKABLE bool writeCompositorConfig();
};

#endif
