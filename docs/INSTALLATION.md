# Installation

## Quick Install (supported distributions)

```bash
git clone https://github.com/Maik-0000FF/schnelle-umlaute.git
cd schnelle-umlaute
./install.sh
```

The installer automatically detects your distribution (Arch, Debian, Ubuntu, Linux Mint, Fedora, openSUSE) and will:
- Check and install dependencies via the appropriate package manager
- Build and install the addon, the standalone editor, the cycle overlay daemon, and the per-user setup helper
- Configure environment variables automatically
- Detect Shift trigger key conflicts that break uppercase mappings
- Guide you through the setup

**After installation:** Logout and login, then run `fcitx5-config-qt` to add "Schnelle Umlaute" to your input methods.

---

## Arch Linux (AUR)

```bash
yay -S fcitx5-schnelle-umlaute-git
```

After install, run the per-user setup helper once (writes the fcitx5 environment variables to `~/.config/environment.d/fcitx5.conf` and sets up the autostart entry):

```bash
schnelle-umlaute-setup
```

Then logout and login. The `-git` package follows the `dev` branch and ships features ahead of the latest tagged release. A stable AUR package (`fcitx5-schnelle-umlaute`) tracking tagged releases will be added later.

> If you skip `schnelle-umlaute-setup` and launch `schnelle-umlaute-editor`, the editor detects the missing environment variables on startup and offers to create the same `environment.d/fcitx5.conf` file. This is a safety net — the editor does not set up autostart, so the standalone helper is still the complete path.

> **wlroots compositors (Hyprland, sway, …):** A compositor started straight from a TTY (`exec Hyprland`) does not import `environment.d`, so the steps above won't activate the variables on their own. Either launch your session through a display manager or [uwsm](https://github.com/Vladimir-csp/uwsm) (then `environment.d` is honored normally), or put the variables in the compositor config — on Hyprland the editor offers an **Add to config** button for `~/.config/hypr/hyprland.conf`. See [TROUBLESHOOTING.md](TROUBLESHOOTING.md#editor-shows-an-activation-pending-dialog-hyprland--sway--other-wlroots).

---

## What gets installed

All install paths (script, manual, AUR) deliver the same artifacts:

| Path | Component |
|---|---|
| `/usr/lib/fcitx5/schnelle-umlaute.so` | Fcitx5 input method addon (the core engine) |
| `/usr/bin/schnelle-umlaute-editor` | Standalone QML editor for mappings, leader keys, app filter, overlay |
| `/usr/bin/schnelle-umlaute-overlay` | Cycle overlay daemon (Wayland with wlr-layer-shell) |
| `/usr/bin/schnelle-umlaute-setup` | One-time per-user environment setup helper |
| `/usr/share/applications/schnelle-umlaute-editor.desktop` | App launcher entry for the editor |
| `/usr/share/icons/hicolor/scalable/apps/schnelle-umlaute-editor.svg` | Editor icon |
| `/usr/share/dbus-1/services/de.schnelle_umlaute.Overlay.service` | DBus activation entry for the overlay daemon |
| `/usr/share/fcitx5/addon/schnelle-umlaute.conf*` | Addon registration + config descriptor |
| `/usr/share/fcitx5/inputmethod/schnelle-umlaute.conf` | Input-method registration |

(Paths shown for an `/usr` prefix install; manual builds use `/usr/local/...` by default.)

---

## Manual Installation (Arch Linux)

**1. Install Dependencies**

```bash
sudo pacman -S fcitx5 fcitx5-configtool fcitx5-qt fcitx5-gtk cmake extra-cmake-modules gcc pkgconf \
    qt6-declarative layer-shell-qt
```

**2. Build the Addon**

```bash
cd addon
./build.sh
```

**3. Install**

```bash
cd build
sudo cmake --install .
```

**4. First-run setup**

```bash
schnelle-umlaute-setup
```

Writes `~/.config/environment.d/fcitx5.conf` (env-vars: GTK/Qt/X11) and configures autostart for your session (KDE Wayland gets `Hidden=true` because KWin handles startup; other sessions get a regular XDG autostart entry). Idempotent — safe to re-run.

**5. Logout and Login**

```bash
# After logout/login, verify Fcitx5 is running:
fcitx5-remote   # Should print "1" (inactive) or "2" (active)
```

---

## Manual Installation (Ubuntu / Debian / Linux Mint)

**1. Install Dependencies**

```bash
sudo apt update
sudo apt install fcitx5 fcitx5-config-qt fcitx5-frontend-gtk3 fcitx5-frontend-gtk4 \
    fcitx5-frontend-qt5 libfcitx5core-dev fcitx5-modules-dev qt6-base-dev \
    libfcitx5-qt6-dev cmake extra-cmake-modules g++ gettext pkg-config \
    libxkbcommon-dev qt6-declarative-dev qml6-module-qtquick-controls \
    liblayershellqtinterface-dev
```

**2. Build the Addon**

```bash
cd addon
./build.sh
```

**3. Install**

```bash
cd build
sudo cmake --install .
```

**4. Set Fcitx5 as Default Input Method**

```bash
im-config -n fcitx5
```

**5. First-run setup**

```bash
schnelle-umlaute-setup
```

Writes the env-vars and the autostart entry for your session.

**6. Logout and Login**

```bash
# After logout/login, verify Fcitx5 is running:
fcitx5-remote   # Should print "1" (inactive) or "2" (active)
```

**GNOME Users:** If Fcitx5 doesn't work in GNOME apps, run:
```bash
gsettings set org.gnome.settings-daemon.plugins.xsettings overrides "{'Gtk/IMModule':<'fcitx'>}"
```

---

## Manual Installation (Fedora)

**1. Install Dependencies**

```bash
sudo dnf install fcitx5 fcitx5-configtool fcitx5-gtk fcitx5-qt6 \
    fcitx5-devel fcitx5-qt-devel qt6-qtbase-devel \
    cmake extra-cmake-modules gcc-c++ gettext \
    qt6-qtdeclarative-devel layer-shell-qt-devel
```

**2. Build the Addon**

```bash
cd addon
./build.sh
```

**3. Install**

```bash
cd build
sudo cmake --install .
```

**4. First-run setup**

```bash
schnelle-umlaute-setup
```

**5. Logout and Login**

```bash
fcitx5-remote   # Should print "1" (inactive) or "2" (active)
```

---

## Manual Installation (openSUSE)

**1. Install Dependencies**

```bash
sudo zypper install fcitx5 fcitx5-configtool fcitx5-gtk3 fcitx5-gtk4 fcitx5-qt6 \
    fcitx5-devel fcitx5-qt-devel qt6-base-devel libxkbcommon-devel \
    cmake extra-cmake-modules gcc-c++ gettext \
    qt6-declarative-devel qt6-quickcontrols2-devel layer-shell-qt6-devel
```

**2. Build the Addon**

```bash
cd addon
./build.sh
```

**3. Install**

```bash
cd build
sudo cmake --install .
```

**4. First-run setup**

```bash
schnelle-umlaute-setup
```

**5. Logout and Login**

```bash
fcitx5-remote   # Should print "1" (inactive) or "2" (active)
```

---

## Post-install: enable the Schnelle Umlaute input method

Open `fcitx5-config-qt` (or KDE System Settings → Input Method), add **Schnelle Umlaute** to your active input methods, and toggle it with <kbd>Ctrl</kbd>+<kbd>Space</kbd>.

To configure mappings, leader keys, the app filter, and the cycle overlay, click the gear/Configure button next to the addon — fcitx5-configtool launches the standalone `schnelle-umlaute-editor`. You can also start it directly from the command line, an application launcher, or its desktop entry.

---

## Uninstallation

### Quick Uninstall

```bash
./uninstall.sh
```

The uninstaller automatically detects your distribution and will:
- Remove all installed addon files
- Ask if you want to remove environment/autostart configuration
- Reload Fcitx5

### Manual Uninstallation

If the build directory still exists, use the install manifest:
```bash
cd addon/build
sudo xargs rm -f < install_manifest.txt
# Reload fcitx5 to unload the removed addon:
fcitx5-remote -r
```

Otherwise, remove files manually. The addon library path depends on your distribution:
```bash
# Arch Linux
sudo rm /usr/lib/fcitx5/schnelle-umlaute.so

# Ubuntu/Debian/Linux Mint (x86_64)
sudo rm /usr/lib/x86_64-linux-gnu/fcitx5/schnelle-umlaute.so

# Fedora / openSUSE (x86_64)
sudo rm /usr/lib64/fcitx5/schnelle-umlaute.so

# Source build (cmake --install with default prefix)
sudo rm /usr/local/lib/fcitx5/schnelle-umlaute.so

# Standalone editor + overlay binaries + setup helper (any distribution)
sudo rm /usr/bin/schnelle-umlaute-editor
sudo rm /usr/bin/schnelle-umlaute-overlay
sudo rm /usr/bin/schnelle-umlaute-setup

# Editor desktop entry, icon, and overlay DBus service
sudo rm /usr/share/applications/schnelle-umlaute-editor.desktop
sudo rm /usr/share/icons/hicolor/scalable/apps/schnelle-umlaute-editor.svg
sudo rm /usr/share/dbus-1/services/de.schnelle_umlaute.Overlay.service

# Legacy fcitx5-configtool Qt plugin (only present on installs from
# versions before 1.2.0 — ignore "No such file or directory" otherwise):
sudo rm /usr/lib/fcitx5/qt6/libschnelle-umlaute-config-editor.so 2>/dev/null

# Common data files (all distributions)
sudo rm /usr/share/fcitx5/addon/schnelle-umlaute.conf
sudo rm /usr/share/fcitx5/addon/schnelle-umlaute.conf.in
sudo rm /usr/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml
sudo rm /usr/share/fcitx5/inputmethod/schnelle-umlaute.conf

# Optional: remove user configuration
rm ~/.config/fcitx5/conf/schnelle-umlaute.conf
rm -r ~/.config/fcitx5/schnelle-umlaute/
rm ~/.config/environment.d/fcitx5.conf

# Reload fcitx5 to unload the removed addon:
fcitx5-remote -r
```
