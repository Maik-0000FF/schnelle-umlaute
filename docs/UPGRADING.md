# Upgrading

## Unreleased

No config or mapping migration required. Pull / reinstall and restart fcitx5. If you have the overlay enabled, **restart the `schnelle-umlaute-overlay` daemon** (or toggle the overlay off and on in the editor) so the new build takes over. This release is the one where that restart matters functionally: the timing progress bar's update message gained a field, so an old daemon left running just won't draw the bar (cycling and the rest of the overlay are unaffected) until it restarts.

## Upgrading from v1.2.3 to v1.3.0

`[Overlay] AtCursor` (added in 1.2.3) is replaced by `[Overlay] Placement` with three values: `Grid` (the fixed Row/Column position), `MouseCursor` (at the mouse pointer), and the new `TextCaret` (at the text input cursor, rendered through fcitx5's candidate window, so it needs no layer-shell and works on X11 too). The editor migrates an old `AtCursor=True` to `MouseCursor` automatically.

No other config or mapping migration required. v1.3.0 is otherwise config-compatible with v1.2.3. Pull / reinstall and restart fcitx5. If you have the overlay enabled, restart the `schnelle-umlaute-overlay` daemon (or toggle the overlay off and on in the editor) so it picks up the new build.

### What changed

- **Overlay at the text caret.** The new `TextCaret` placement shows the accent variants at the text input cursor through fcitx5's candidate window, so it needs no layer-shell and works on X11 too. An opt-in option styles that candidate window to match the editor theme.
- **Clearer overlay settings.** The editor's Overlay section leads with the placement choice, and placements that need wlr-layer-shell are flagged inline on sessions that lack it.
- **Themed editor chrome.** The right-click context menu in editor input fields and the control tooltips now follow the editor theme instead of the platform default.

## Upgrading from v1.2.2 to v1.2.3

No mapping or config migration required. v1.2.3 is config-compatible with v1.2.2. Pull / reinstall and restart fcitx5. If you have the overlay enabled, restart the `schnelle-umlaute-overlay` daemon (or toggle the overlay off and on in the editor) so it picks up the new build.

### What changed

- **Overlay can follow the mouse cursor.** A new `[Overlay] AtCursor` option anchors the overlay at the pointer instead of the fixed Row/Column grid; the grid stays as the fallback for compositors that can't report the cursor. Off by default.
- **Overlay timing progress bar.** A new `[Overlay] ProgressBar` option draws a timing bar for the whole accent gesture (min-hold lead-in, then the `[min, max]` leader window counting down). Off by default.
- **Per-theme overlay colours, click-through surface, refined cursor picker.** The overlay now draws its accent/progress colours from the active theme, never intercepts input (click-through), and the editor's position picker is refined. No config or action required.

## Upgrading from v1.2.1 to v1.2.2

No mapping or config migration required. v1.2.2 is config-compatible with v1.2.1. Pull / reinstall and restart fcitx5.

### What changed

- **Minimum-hold lower bound for the accent window.** Each delay is now a window `[min, max]` instead of a single timeout. The lower handle is a minimum hold time: a leader (e.g. Space) arriving before it yields the plain character, so fast typists no longer trigger accents by accident. The editor's two delay sliders become range sliders (drag the line between the handles to move the whole window) with a 10 ms step. The lower bound defaults to 0, so existing configs and behaviour are unchanged until you raise it.
- **TTY-launched compositors get the right environment guidance.** When `schnelle-umlaute-editor` detects a session started straight from a TTY (e.g. Hyprland via `exec-once`), where `~/.config/environment.d/` is never read, it now writes the input-method variables into the compositor config and shows compositor-specific instructions instead of the unhelpful "log out and back in" dialog.

## Upgrading from v1.2.0 to v1.2.1

No mapping or config migration required. v1.2.1 is config-compatible with v1.2.0. Pull / reinstall and log out / in if your environment variables changed.

### What changed

- **Editor prompts for missing environment setup.** When `schnelle-umlaute-editor` starts and detects that `GTK_IM_MODULE`, `QT_IM_MODULE` or `XMODIFIERS` are not set to `fcitx`, it shows a modal dialog offering to create `~/.config/environment.d/fcitx5.conf`. The check runs on every start, not just first run, so users who skipped `schnelle-umlaute-setup` reach a working install without consulting the docs. The dialog does *not* configure autostart — `schnelle-umlaute-setup` remains the complete path.
- **Theme-aware dropdowns.** Both pickers in the editor's Settings tab (theme, app-filter mode) now render their popup, item delegates and indicator from the active theme palette instead of the system default. The active row uses the theme's accent colour (violet / blue / blue / yellow per theme) instead of a constant green.
- **Underline-style main tabs.** The Settings/Mappings tab strip switches from a pill-button look to plain underline tabs. The active tab is marked by a 2 px accent-coloured underline; inactive tabs lift to full text colour on hover.

No action required for either of the visual changes — they apply on first launch of the updated editor.

## Upgrading from v1.1.x to v1.2.0

No mapping or config migration required. v1.2.0 is config-compatible with v1.1.x — your existing `mappings.txt` and `schnelle-umlaute.conf` continue to work unchanged. The settings file gains optional `[Overlay]` and `[AppFilter]` sections; their defaults are safe (overlay disabled, filter mode `Disabled`), so no action is needed unless you want to use those features.

### What changed

- **Editor moved to a standalone application.** The Qt Widgets plugin `libschnelle-umlaute-config-editor.so` (loaded by `fcitx5-config-qt` from inside its own window) is **removed**. It is replaced by a standalone QML application: `schnelle-umlaute-editor`. fcitx5-config-qt's gear/Configure button now launches this binary instead (same entry point, separate window). You can also start it directly from the command line, an application launcher, or its desktop entry.
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
