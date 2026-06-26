# Architecture

Schnelle Umlaute is composed of four shipped artifacts plus a per-user setup helper. The core is a **native Fcitx5 addon** written in **C++20** using the `InputMethodEngineV2` API; the editor and the cycle overlay daemon are **standalone Qt 6 / QML** binaries that talk to the addon (and to each other) over DBus.

## Components

```
┌─────────────────────────────────────────────────────────────────┐
│  fcitx5  (system input-method framework)                        │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  schnelle-umlaute.so  (this addon — the engine)           │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                ▲                                  │
        DBus    │ ReloadAddonConfig    DBus Show() │ Quit()
                │                                  ▼
   ┌────────────────────────┐          ┌──────────────────────────┐
   │  schnelle-umlaute-     │          │  schnelle-umlaute-       │
   │  editor                │          │  overlay  (DBus-activated)│
   │  (standalone QML app)  │          │  (Wayland layer-shell)    │
   └────────────────────────┘          └──────────────────────────┘
```

### Addon — `schnelle-umlaute.so`

The Fcitx5 input-method engine. Source under `addon/src/`, decomposed into focused modules:

| File | Responsibility |
|---|---|
| `schnelle-umlaute.cpp` / `.h` | InputMethodEngineV2 implementation, key dispatch, lifecycle |
| `state.h` | Per-input-context engine state (waiting key, timer, recently-committed flag, …) |
| `config.h` | Strongly-typed view of `schnelle-umlaute.conf` (Delays, Leaders, AppFilter, Overlay) |
| `app_filter.cpp` / `.h` | Blacklist/whitelist matching against the program identifier fcitx5 reports |
| `hand_classifier.cpp` / `.h` | Layout-independent left/right-hand classification for dual-split custom leaders |
| `mappings_loader.cpp` / `.h` | Reads/writes `mappings.txt` (`Input=Output` format with cycling variants) |
| `overlay_client.cpp` / `.h` | DBus client for `de.schnelle_umlaute.Overlay` — calls `Show()` / `Quit()` |
| `overlay_lifecycle.h` | Pure logic for when to start/stop the overlay daemon (covered by tests) |
| `layer_shell_capability.h` | Compositor capability detection: gates the overlay on systems lacking `wlr-layer-shell` |

### Standalone editor — `schnelle-umlaute-editor`

A Qt Quick application under `addon/editor/`. Launched in three equivalent ways:
- fcitx5-configtool's gear/Configure button (via the `[Editor] External=schnelle-umlaute-editor` entry in the addon's config descriptor)
- The CLI command `schnelle-umlaute-editor`
- The desktop launcher (Activities / app menu)

It edits two files under `~/.config/fcitx5/`:
- `conf/schnelle-umlaute.conf` (settings: delays, leaders, app filter, overlay)
- `schnelle-umlaute/mappings.txt` (mappings)

After a save, the editor calls **`Controller1.ReloadAddonConfig`** on fcitx5's DBus interface, so the running addon picks up changes live — no `fcitx5-remote -r` needed when configuring through the editor.

### Cycle overlay daemon — `schnelle-umlaute-overlay`

A small Qt Quick + LayerShellQt daemon under `addon/overlay/`. **DBus-activated** via `/usr/share/dbus-1/services/de.schnelle_umlaute.Overlay.service` — there is no XDG-Autostart entry. The addon starts the daemon lazily the first time it has a variant to display, and stops it (DBus `Quit()`) when the user disables the overlay in the editor.

The daemon draws a small layer-shell surface that follows the focused output (`setWantsToBeOnActiveScreen(true)` on LayerShellQt 6.6+). The DBus interface is intentionally minimal: `Show(variants: as, currentIndex: i, gestureId: s)` and `Quit()`.

Compositors without `wlr-layer-shell` (GNOME/Mutter, X11) cannot host the surface. Detection happens at runtime in `layer_shell_capability.h`; the editor greys out the Overlay toggle and the addon skips DBus activation on those systems.

### Per-user setup helper — `schnelle-umlaute-setup`

A small Bash script under `addon/scripts/schnelle-umlaute-setup`, installed to `/usr/bin/`. Idempotent, refuses to run as root. Writes:
- `~/.config/environment.d/fcitx5.conf` (env-vars: `GTK_IM_MODULE`, `QT_IM_MODULE`, `XMODIFIERS`, `GLFW_IM_MODULE`)
- `~/.config/autostart/org.fcitx.Fcitx5.desktop` (per session: `Hidden=true` on KDE Wayland, regular autostart elsewhere)

Replaces the boilerplate that earlier versions duplicated across each per-distro install path.

## How a key event flows internally

1. Fcitx5 calls the addon's `keyEvent()` handler for every key.
2. **App filter check**: if the focused app matches the configured blacklist (or is absent from the whitelist), the key passes through unchanged — addon is invisible.
3. If the key is a configured **mapped input** (default: `a/o/u/s` plus their Shifted forms), the engine enters the **waiting state**: the keystroke is suppressed (the user does not see the literal `a` yet) and a timeout timer starts (default 400 ms lowercase, 700 ms uppercase — both configurable).
4. While waiting:
   - **Leader key arrives in time** → commit the mapped output (`commitString("ä")`). If the mapping has multiple variants (e.g. `é,è,ê,ë`), display the first variant in the **preedit** instead of committing, and arm cycling on subsequent leader presses; commit on key release.
   - **Same key released** → commit the original character (`commitString("a")`).
   - **Different key pressed** → finalise the waiting key first (commit `a`), then forward the new key. This is the "fast typing shortcut" — typing `a`, `k` quickly produces `ak` with no perceived lag.
   - **Timer fires** → commit the original character. The next Space goes through `commitString` to preserve text ordering (the "ordering guard").
5. No clipboard, no key simulation, no XTest — `commitString()` only.

## IPC topology

| From | To | Mechanism | Purpose |
|---|---|---|---|
| editor | fcitx5 (`/controller`) | DBus method call `Controller1.ReloadAddonConfig` | Live-reload after config save |
| addon | overlay daemon | DBus auto-activation | Lazy start on first `Show()` |
| addon | overlay daemon | DBus method `Show(as, i, s)` | Render variants for current gesture |
| addon | overlay daemon | DBus method `Quit()` | Stop daemon when overlay is disabled |

Tested in `tests/testoverlaylifecycle.cpp` and `tests/testoverlaycontroller.cpp`.

## Comparison with other approaches

| Approach | Clipboard-Free | No Root | X11 | Wayland | Complexity |
|----------|---------------|---------|-----|---------|------------|
| **Fcitx5 Addon (This)** | ✅ | ✅ | ✅ | ✅ | Medium |
| evdev-rs + xclip | ❌ | ❌ | ✅ | ⚠️ | Low |
| IBus | ✅ | ✅ | ✅ | ✅ | High |
| XTest | ✅ | ❌ | ✅ | ❌ | Low |
