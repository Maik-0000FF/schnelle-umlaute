# Troubleshooting

## Accidental accents when typing fast

**Symptom:** When typing fast with Space as leader key, accents appear at word boundaries instead of a normal letter + space.

**Examples:**

| Language | Expected | Actual (fast typing) | Cause |
|----------|----------|---------------------|-------|
| German | Der Bus kommt gleich. | Der Buß kommt gleich. | `s` still held when Space is pressed → ß |
| French | Je mange une pomme chaque jour. | Je mange une pommé chaque jour. | `e` still held when Space is pressed → é |
| Spanish | El tren sale a las ocho. | El treñ sale a las ocho. | `n` still held when Space is pressed → ñ |

**Cause:** Space serves as both word separator and leader key. When typing quickly, the mapped key at the end of a word hasn't been released yet when Space is pressed — the addon interprets this as a hold+space gesture and outputs the accent instead of the normal letter + space.

**Solution:** Switch to a leader key that doesn't conflict with normal typing:

- **Arrow keys** (<kbd>←</kbd><kbd>→</kbd><kbd>↑</kbd><kbd>↓</kbd>) — dedicated, no conflict
- **Alt / AltGr** — dedicated, no conflict
- **Custom keys** (e.g. `f`, `j`) — tested across multiple languages with few conflicts
- **Dual custom leaders** (hand-split) — one leader per keyboard half, near-zero conflicts

See [Configuration → Leader Key](CONFIGURATION.md#leader-key) for setup instructions.

## Addon stops working after moving/switching windows (fcitx5 5.1.18+)

KWin tiling scripts (e.g. [MouseTiler](https://github.com/rxappdev/MouseTiler)) can interfere with the addon — mapped keys stop producing output. Disable the tiling script to fix this.

## Uppercase mappings not working (Shift trigger conflict)

**Symptom:** Lowercase mappings (a → ä) work, but uppercase mappings (Shift+A → Ä) switch the input method instead.

**Cause:** Fcitx5 uses Shift as input method trigger key. When you press Shift+A, fcitx5 intercepts the Shift press to switch input methods before the addon can process it.

**Fix:** Change the trigger key in Fcitx5 settings:
1. Open `fcitx5-config-qt` → **Global Options** → **Trigger Input Method**
2. Remove any Shift-based triggers and use **Ctrl+Space** instead

> **Note:** The install script detects this conflict and offers to fix it automatically.

## Addon not showing in fcitx5-config-qt

Check if all files are installed:
```bash
ls /usr/lib/fcitx5/schnelle-umlaute.so
ls /usr/share/fcitx5/addon/schnelle-umlaute.conf
ls /usr/share/fcitx5/addon/schnelle-umlaute.conf.in  # For GUI config options
ls /usr/share/fcitx5/inputmethod/schnelle-umlaute.conf  # This is important!
```

If missing, reinstall:
```bash
cd addon/build
sudo cmake --install .
fcitx5 -r
```

## Configuration options not visible in GUI

If the addon appears but you can't see DelayLowercase/DelayUppercase settings:

1. Check if the config descriptor is installed:
   ```bash
   ls /usr/share/fcitx5/addon/schnelle-umlaute.conf.in
   ```

2. If missing, reinstall the addon:
   ```bash
   cd addon && ./build.sh && cd build && sudo cmake --install . && fcitx5 -r
   ```

## Works in terminal but not in other apps (Firefox, Kate, etc.)

Environment variables must be set correctly. See the environment variables step in the [Installation Guide](INSTALLATION.md) for your platform.

Then **logout and login again** for changes to take effect.

## Umlauts not appearing

1. Make sure you're switched to "Schnelle Umlaute" input method (<kbd>Ctrl</kbd> + <kbd>Space</kbd>)
2. Check Fcitx5 is running: `ps aux | grep fcitx5`
3. Try holding the key longer before pressing Space
4. Verify environment variables are set: `echo $GTK_IM_MODULE` (should output "fcitx")

## Addon is visible but not activatable / Fcitx5 not responding

If you can see "Schnelle Umlaute" in fcitx5-config-qt but cannot activate it, or if Fcitx5 stopped working after a system crash:

1. **Check Fcitx5 status:**
   ```bash
   fcitx5-remote
   ```
   - Should show: `1` (inactive) or `2` (active)
   - If it shows: `0` → Fcitx5 not initialized

2. **Restart Fcitx5:**
   ```bash
   fcitx5 -r
   ```

3. **Activate addon:**
   ```bash
   fcitx5-remote -s schnelle-umlaute
   ```

4. **Verify:**
   ```bash
   fcitx5-remote -n  # Should show: schnelle-umlaute
   fcitx5-remote     # Should show: 2 (active)
   ```

This is common after system crashes or unexpected shutdowns.

## KDE Wayland users: "Fcitx should be launched by KWin" warning

If you see a warning about Fcitx5 not being launched by KWin, fix it for optimal Wayland experience:

1. Open **System Settings** → **Virtual Keyboard** (or search for "virtuell")
2. Select **"Fcitx 5"** from the dropdown (instead of "None")
3. Apply changes
4. **Restart your session** (logout/login)

This enables the native Wayland input method protocol and eliminates the warning.

## Input method not shared across applications

By default, Fcitx5 remembers the input method **per application**. If you switch to "Schnelle Umlaute" in Firefox, the terminal may still use the US keyboard.

To share the input method state globally:

1. Open Fcitx5 configuration: `fcitx5-config-qt`
2. Go to **Global Options**
3. Set **Share Input State** to **All**
4. Restart Fcitx5: `fcitx5 -r`

## WezTerm known issues

WezTerm has upstream issues with fcitx5 that are **not caused by this addon**:

- **Addon not working after fcitx5 restart:** WezTerm cannot reconnect to fcitx5 via XIM. Re-login or restart WezTerm after addon changes. ([#2819](https://github.com/wezterm/wezterm/issues/2819))
- **Key repeat flood after fcitx5 restart:** If fcitx5 is killed (e.g. via `killall`) while a mapped key is being processed, WezTerm may lose the key release event. This causes the last key to repeat endlessly. Re-login to fix. ([#2819](https://github.com/wezterm/wezterm/issues/2819))
- **Copy/paste between WezTerm windows:** May require clicking into the target window first on Wayland. ([#6685](https://github.com/wezterm/wezterm/issues/6685))

These issues do not occur in Kitty, which uses a more robust IME integration.

## XWayland mixed mode: addon stops working globally

**Symptom:** After opening an X11 application from a Wayland session (e.g. via `--ozone-platform=x11` or native X11 apps like `xterm`), the addon stops working — not only in the X11 app, but in **all** applications. Mapped keys are swallowed with no output. The addon does not recover after closing the X11 app.

**Affected applications:**
| Application | Status after XWayland app opened |
|---|---|
| WezTerm | Broken |
| Chromium | Broken |
| Neovide | Broken |
| Ghostty | Not affected |
| Firefox | Not affected |

**Cause:** XWayland sends focus events that are incompatible with fcitx5's Wayland input method protocol. This corrupts the input method connection for applications with fragile IM bindings (winit-based apps via XIM, Chromium's custom input stack). Applications with robust IM implementations (Ghostty, Firefox) are not affected.

**Recovery:** Restarting fcitx5 alone does not fix the affected apps. A re-login is not always sufficient either. A full system reboot reliably restores functionality.

**Note:** This is not a bug in the addon — it is caused by the interaction between XWayland focus handling and the affected applications' input method implementations. In a pure X11 or pure Wayland session, this does not occur.

## Build errors

Make sure you have C++20 support:
```bash
gcc --version  # Should be 11 or newer
```

## Cycling preview not visible

**Symptom:** When cycling through accent variants, you don't see the current character changing. The cycling works, but the preview is not displayed.

**Cause 1 - "Show Preedit In Application" disabled:** The addon uses Fcitx5's client preedit to display the cycling preview. If **"Show Preedit In Application"** is disabled in Fcitx5's global options, the preview cannot be forwarded to the application.

**Fix:** Open `fcitx5-config-qt` → **Global Options** → enable **"Show Preedit In Application"**.

| KDE System Settings | fcitx5-config-qt |
|:-:|:-:|
| ![KDE](assets/screenshot-global-options-kde.png) | ![Qt](assets/screenshot-global-options-qt.png) |

**Cause 2 - Terminal emulators:** Many terminal emulators don't display preedit (composition) text visually, even when the setting is enabled. The cycling still works internally.

**Workaround for terminals:** Count your <kbd>Space</kbd> presses to reach the desired variant:
- 1× <kbd>Space</kbd> = first variant (e.g., ä)
- 2× <kbd>Space</kbd> = second variant (e.g., à)
- 3× <kbd>Space</kbd> = third variant (e.g., â)

The final character is committed when you release the input key. In GUI applications (Firefox, Kate, etc.), the cycling preview is displayed in real-time.

## Double characters in security-sensitive input fields (banking, login, etc.)

**Symptom:** When typing a mapped character (e.g., `1` mapped on Input 10) in a security-sensitive input field (banking forms, login pages, 2FA codes), the character appears twice (e.g., `11` instead of `1`).

**Cause:** This is **not a bug in the addon**. Security-sensitive input fields often disable or partially implement the input method (IME) protocol. The addon's normal flow is:

1. Key press → `filterAndAccept()` consumes the key (original character suppressed)
2. Key release or timeout → `commitString()` sends the character once

When a security field ignores `filterAndAccept()`, both the raw keystroke **and** the committed string reach the field, resulting in a double character.

**Affected:** Any mapped character - letters, digits, and special characters alike. It is more noticeable with digits (e.g., year input `2024` becomes `20024`) because digits are commonly used in security forms.

**Workaround:**
- Switch to your base keyboard layout (<kbd>Ctrl</kbd> + <kbd>Space</kbd>) before entering data in security-sensitive fields
- Avoid mapping characters that are frequently needed in security contexts (digits, common password characters)

## General compatibility note

Not all applications fully support Fcitx5's input method protocol. The addon relies on the application correctly handling:
- Preedit (composition) text display
- Commit string insertion
- Input context state management

Applications with custom text rendering or non-standard input handling may not work correctly. If you encounter issues in a specific application, please report it on the GitHub issues page.
