# Upgrading

## Upgrading from v1.1.x to v1.2.0

No mapping or config migration required. v1.2.0 is config-compatible with v1.1.x — your existing `mappings.txt` and `schnelle-umlaute.conf` continue to work unchanged. The settings file gains optional `[Overlay]` and `[AppFilter]` sections; their defaults are safe (overlay disabled, filter mode `Disabled`), so no action is needed unless you want to use those features.

### What changed

- **Editor moved to a standalone application.** The Qt Widgets plugin `libschnelle-umlaute-config-editor.so` (loaded by `fcitx5-config-qt` from inside its own window) is **removed**. It is replaced by a standalone QML application: `schnelle-umlaute-editor`. fcitx5-configtool's gear/Configure button now launches this binary instead — same entry point, separate window. You can also start it directly from the command line, an application launcher, or its desktop entry.
- **New cycle overlay daemon.** A new binary `schnelle-umlaute-overlay` provides an on-screen overlay during accent cycling. It is DBus-activated (no autostart entry) and only available on Wayland compositors with `wlr-layer-shell` (KDE Plasma, sway, Hyprland, …). Disabled by default — enable in the editor's Settings tab.
- **New per-user setup helper.** A small script `schnelle-umlaute-setup` ships in `/usr/bin/`. Run once after install/upgrade to write your fcitx5 environment variables into `~/.config/environment.d/fcitx5.conf` and the autostart entry. Idempotent, refuses to run as root.
- **Engine decomposed.** The addon source is now split across several modules (`app_filter`, `hand_classifier`, `mappings_loader`, `overlay_client`, `state.h`, `config.h`, …). No user-visible behavior change — see [ARCHITECTURE.md](ARCHITECTURE.md).

### Recommended upgrade path

**AUR users** (`fcitx5-schnelle-umlaute-git`):
```bash
yay -S fcitx5-schnelle-umlaute-git
schnelle-umlaute-setup     # only on first install of v1.2.0
```
The post-install message reminds you to run the setup helper.

**Manual install users**:
```bash
git pull
./install.sh
```
`install.sh` rebuilds and replaces the system files, including removing the legacy `libschnelle-umlaute-config-editor.so` plugin if present. Then logout and login.

### Cleanup of legacy files

If you previously installed v1.0/v1.1 manually, the legacy plugin file may still be on disk after upgrading. Both `install.sh` and `uninstall.sh` now clean it up automatically. To verify manually:

```bash
ls /usr/lib/fcitx5/qt6/libschnelle-umlaute-config-editor.so 2>/dev/null
ls /usr/local/lib/fcitx5/qt6/libschnelle-umlaute-config-editor.so 2>/dev/null
```

If either still exists after upgrade, remove it with `sudo rm`.

---

## Upgrading from v1.0.0 to v1.1.0

No migration required. v1.1.0 is fully config-compatible with v1.0.0 — your existing `mappings.txt` and `schnelle-umlaute.conf` continue to work unchanged.

New in v1.1.0: an optional **App Filter** that can disable the addon in selected applications (or restrict it to a whitelist). See [Configuration → App Filter](CONFIGURATION.md#app-filter).

---

## Upgrading from v0.x to v1.0.0

Version 1.0.0 introduced a new configuration format that is **not compatible** with previous versions:

- **Mappings** are now stored in `~/.config/fcitx5/schnelle-umlaute/mappings.txt` using `Input=Output` format (previously stored as sections in `schnelle-umlaute.conf`)
- **Settings** (delays, leader keys) remain in `~/.config/fcitx5/conf/schnelle-umlaute.conf`

### Recommended upgrade path

Run `./uninstall.sh` first (choose "y" to remove user configuration), then `./install.sh`. Your mappings will be reset to defaults — reconfigure them via `schnelle-umlaute-editor` or edit `mappings.txt` manually.
