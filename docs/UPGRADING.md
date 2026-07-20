# Upgrading

## Upgrading from v1.5.x to v1.6.0

No mapping or config migration required. v1.6.0 is config-compatible with v1.5.x. Pull / reinstall and restart fcitx5.

### What changed

- **Version and help on the command line.** `schnelle-umlaute-editor` and `schnelle-umlaute-overlay` now accept `--version` / `-v` and `--help` / `-h`.
- **About dialog in the editor.** An info button in the editor header opens an About dialog with the version, a short description, and links to the repository, the issue tracker, and the license.

## Upgrading from v1.4.x to v1.5.0

**If you use a custom leader key, you have to re-capture it once.** A custom leader is no longer stored as the character it prints but as the physical key you press, and your existing config has no key recorded. Until you set it again, the custom leader is inactive; every other leader (Space, arrows, Alt/AltGr) keeps working, and your mappings are untouched.

Open the editor, go to the leader keys, click the custom leader field and press the key you want. The field now captures a real key press instead of taking a typed character. If you use the two-key hand split, do this for both.

Nothing else migrates: your mappings, profiles and the rest of `schnelle-umlaute.conf` carry over unchanged. Pull / reinstall and restart fcitx5. If you have the overlay enabled, the engine detects an overlay daemon left over from the old build (its wire protocol changed this cycle) and restarts it automatically.

### What changed

- **A custom leader is a physical key.** It is captured as a key press and matched by its key code, so it no longer depends on the character printed on the cap: the same physical key stays the leader across a layout switch, and Shift cannot confuse it. The character is still shown in the editor, and still checked against your mappings, but it no longer drives the matching. This also removed a class of bug on non-US layouts, where the addon compiled its own keyboard map and could resolve the wrong key.
- **The overlay is markedly cheaper.** Its daemon used to rebuild the whole QML engine every time the overlay appeared, which with the timing bar enabled happened on every keystroke of a mapped letter. It now builds once: 40 open/close cycles cost 2.9 s of daemon CPU before this release and 0.4 s after.
- **Overlay placement and cursor fixes.** In the "at the mouse pointer" mode on KDE, a slow reply from the compositor could place the overlay where the pointer had been during the *previous* keystroke; replies are now matched to the query that asked for them. A repeated key no longer leaves the previously chosen accent lit for a moment, and with the timing bar on, the panel no longer flashes before the timing window opens.

## Upgrading from v1.3.0 to v1.4.0

Your existing mappings and config carry over unchanged. On the first editor launch, v1.4.0 seeds a "Standard" profile that points at your current `mappings.txt` (the file is left exactly as it is, never rewritten) and records it in a new `profiles.conf`. The engine also falls back to `mappings.txt` when no profiles are configured, so nothing breaks even if you never open the editor. No manual config or mapping migration is required.

Pull / reinstall and restart fcitx5. If you have the overlay enabled, the engine now detects an overlay daemon left running from the old build (its wire protocol changed this cycle) and restarts it automatically, so the timing progress bar and the rest of the overlay come up on the new build with no manual step. Toggling the overlay off and on in the editor still works if you want to force it.

### New files

Two new items appear next to your mappings, both managed from the editor (no need to edit them by hand):

- `~/.config/fcitx5/schnelle-umlaute/profiles.conf`: the profile list, the active profile, and the switch/cycle shortcuts.
- `~/.config/fcitx5/schnelle-umlaute/profiles/`: the mapping files of the extra profiles (the Standard profile keeps using `mappings.txt`).

`schnelle-umlaute.conf` is unchanged: this release only adds config elsewhere, nothing is renamed or removed.

### What changed

- **Mapping profiles.** Keep several independent mapping sets and switch between them. Manage them on the Mappings tab (create, rename, delete, mark favorites, assign a per-profile switch shortcut). Switch at runtime with a profile's own shortcut, or step through them with configurable cycle-next / cycle-previous shortcuts (these walk the favorites if you marked any, otherwise every profile); the active profile's name flashes briefly on switch. Your previous single mapping set becomes the "Standard" profile automatically.
- **Preset library.** A Library dropdown beside the profile selector bundles ready-made mapping sets you can add with a click (copied into your own profiles, live immediately). It covers 36 language presets (for example Français, Español, Polski, Türkçe, Ελληνικά, Tiếng Việt, Pīnyīn) and a set of symbol presets: Math & Symbols, Currency, IPA, polytonic Greek, Romanization, Proto-Indo-European, Arrows, Typography, LaTeX/Typst, Braille, and an emoji set. Many presets use the full keyboard, including uppercase and symbol keys.
- **`#` and `\` as input keys.** These can now be mapped as input keys (previously a leading `#` began a comment line); the editor writes them with a backslash escape so they round-trip.
- **Editor design and keyboard operability.** A unified type scale and control sizing, full keyboard navigation (tab bar, list navigation, shortcuts), and a single, consistent focus/selection highlight that follows whichever input (mouse or keyboard) you are currently using.
- **Overlay accuracy, colours and self-healing.** A frame-accurate timing progress bar, more accurate gesture timing, a live marker when the profile switches, per-theme overlay and slider colours with label fixes, and automatic restart of a stale overlay daemon after an in-place upgrade.
- **Robustness fixes.** A round of fixes across the editor and engine: input and output data-loss edge cases, profile-switch rendering, editor keyboard handling, and overlay lifecycle.

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

- **Editor prompts for missing environment setup.** When `schnelle-umlaute-editor` starts and detects that `GTK_IM_MODULE`, `QT_IM_MODULE` or `XMODIFIERS` are not set to `fcitx`, it shows a modal dialog offering to create `~/.config/environment.d/fcitx5.conf`. The check runs on every start, not just first run, so users who skipped `schnelle-umlaute-setup` reach a working install without consulting the docs. The dialog does *not* configure autostart, `schnelle-umlaute-setup` remains the complete path.
- **Theme-aware dropdowns.** Both pickers in the editor's Settings tab (theme, app-filter mode) now render their popup, item delegates and indicator from the active theme palette instead of the system default. The active row uses the theme's accent colour (violet / blue / blue / yellow per theme) instead of a constant green.
- **Underline-style main tabs.** The Settings/Mappings tab strip switches from a pill-button look to plain underline tabs. The active tab is marked by a 2 px accent-coloured underline; inactive tabs lift to full text colour on hover.

No action required for either of the visual changes, they apply on first launch of the updated editor.

## Upgrading from v1.1.x to v1.2.0

No mapping or config migration required. v1.2.0 is config-compatible with v1.1.x, your existing `mappings.txt` and `schnelle-umlaute.conf` continue to work unchanged. The settings file gains optional `[Overlay]` and `[AppFilter]` sections; their defaults are safe (overlay disabled, filter mode `Disabled`), so no action is needed unless you want to use those features.

### What changed

- **Editor moved to a standalone application.** The Qt Widgets plugin `libschnelle-umlaute-config-editor.so` (loaded by `fcitx5-config-qt` from inside its own window) is **removed**. It is replaced by a standalone QML application: `schnelle-umlaute-editor`. fcitx5-config-qt's gear/Configure button now launches this binary instead (same entry point, separate window). You can also start it directly from the command line, an application launcher, or its desktop entry.
- **New cycle overlay daemon.** A new binary `schnelle-umlaute-overlay` provides an on-screen overlay during accent cycling. It is DBus-activated (no autostart entry) and only available on Wayland compositors with `wlr-layer-shell` (KDE Plasma, sway, Hyprland, …). Disabled by default, enable in the editor's Settings tab.
- **New per-user setup helper.** A small script `schnelle-umlaute-setup` ships in `/usr/bin/`. Run once after install/upgrade to write your fcitx5 environment variables into `~/.config/environment.d/fcitx5.conf` and the autostart entry. Idempotent, refuses to run as root.
- **Engine decomposed.** The addon source is now split across several modules (`app_filter`, `hand_classifier`, `mappings_loader`, `overlay_client`, `state.h`, `config.h`, …). No user-visible behavior change, see [ARCHITECTURE.md](ARCHITECTURE.md).

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

No migration required. v1.1.0 is fully config-compatible with v1.0.0, your existing `mappings.txt` and `schnelle-umlaute.conf` continue to work unchanged.

New in v1.1.0: an optional **App Filter** that can disable the addon in selected applications (or restrict it to a whitelist). See [Configuration → App Filter](CONFIGURATION.md#app-filter).

---

## Upgrading from v0.x to v1.0.0

Version 1.0.0 introduced a new configuration format that is **not compatible** with previous versions:

- **Mappings** are now stored in `~/.config/fcitx5/schnelle-umlaute/mappings.txt` using `Input=Output` format (previously stored as sections in `schnelle-umlaute.conf`)
- **Settings** (delays, leader keys) remain in `~/.config/fcitx5/conf/schnelle-umlaute.conf`

### Recommended upgrade path

Run `./uninstall.sh` first (choose "y" to remove user configuration), then `./install.sh`. Your mappings will be reset to defaults, reconfigure them via `schnelle-umlaute-editor` or edit `mappings.txt` manually.
