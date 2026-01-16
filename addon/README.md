# Schnelle Umlaute - Fcitx5 C++ Addon

Quick German umlaut input using Fcitx5 native addon.

## Features

- **No clipboard usage** - Direct text insertion via `commitString()`
- **No root permissions** - Runs as user process
- **X11 + Wayland** - Native support for both
- **Hold & Wait** - Hold letter + Leader key → umlaut
- **Zero latency** - No backspace needed
- **Configurable leader key** - Space, Arrow keys, or combinations
- **20 custom mappings** - Define your own character transformations
- **Accent cycling** - Define multiple outputs per key, cycle through with repeated leader key presses

## How it works

<kbd>a</kbd> + <kbd>Space</kbd> → <kbd>ä</kbd>

1. Hold a letter key (<kbd>a</kbd>, <kbd>o</kbd>, <kbd>u</kbd>, <kbd>s</kbd> - configurable)
2. Press leader key within the time window:
   - **Leader key**: <kbd>Space</kbd> by default (configurable to Arrow keys or combinations)
   - **400ms** for lowercase letters
   - **700ms** for uppercase letters
3. → Get the umlaut!

**Note:** You can keep <kbd>Shift</kbd> pressed while pressing the leader key for uppercase umlauts

## Important: Keyboard Layout Requirement

This addon is **not a standalone keyboard layout** - it works **alongside** your existing keyboard layout.

```
Physical key → Base layout (US) → Addon → Application
```

**You always need a base keyboard layout** (e.g., US) in your Fcitx5 configuration. The addon:
- Receives characters that are already translated by your base layout
- Only modifies the configured keys (<kbd>a</kbd>, <kbd>o</kbd>, <kbd>u</kbd>, <kbd>s</kbd>, etc.)
- Passes all other keys through unchanged

**Example:**

| You press | Result |
|-----------|--------|
| <kbd>a</kbd> + <kbd>Space</kbd> | ä |
| <kbd>b</kbd> | b (no mapping) |

## Accent Cycling

You can define multiple outputs per input using **comma-separated values** in a single output field.

**Example 1 - French e:**
- Input: `e`
- Output: `é,è,ê,ë`

**Example 2 - German/French a:**
- Input: `a`
- Output: `ä,à,â,æ`

**Usage:**

Hold <kbd>a</kbd> + <kbd>Space</kbd> → ä + <kbd>Space</kbd> → à + <kbd>Space</kbd> → â ...

The character cycles through all defined outputs. Release the key to confirm.

## Emojis, Symbols and Snippets

Outputs are not limited to single characters. You can also use:

**Emojis:**
- Input: <kbd>h</kbd>
- Output: `❤️,💜,💙,💚`

**Special symbols:**
- Input: <kbd>p</kbd>
- Output: `π,φ,θ,Ω`

**Math symbols:**
- Input: <kbd>m</kbd>
- Output: `±,×,÷,≠,≈,∞`

**Short text snippets:**
- Input: <kbd>@</kbd>
- Output: `mail@example.com`

**Common phrases:**
- Input: <kbd>g</kbd>
- Output: `Good morning!,Good afternoon!,Good evening!`

This makes the addon useful beyond accents - use it for any frequently typed text or symbols.

## Configuration

All settings can be configured via `fcitx5-config-qt`:

- **Delay times** (50-2000ms range)
- **Leader key** (<kbd>Space</kbd>, <kbd>←</kbd>/<kbd>→</kbd> Arrow, or combinations)
- **Character mappings** (20 slots: Input → Output)
  - Default: German umlauts (<kbd>a</kbd>→ä, <kbd>o</kbd>→ö, <kbd>u</kbd>→ü, <kbd>s</kbd>→ß, <kbd>A</kbd>→Ä, <kbd>O</kbd>→Ö, <kbd>U</kbd>→Ü)
  - Customize for other languages or shortcuts
  - Use comma-separated values for cycling (e.g. `ä,à,â,æ`)
  - Press "Defaults" button to restore German umlauts

## Build Requirements

```bash
sudo pacman -S cmake extra-cmake-modules fcitx5
```

## Build & Install

**Important:** Always use `./build.sh` to build! Do not run `cmake ..` manually, as this will install to the wrong location (`/usr/local/` instead of `/usr/`).

```bash
# Build (sets correct install prefix /usr)
./build.sh

# Install
cd build
sudo make install

# Restart Fcitx5
fcitx5 -r
```

### Environment Variables (Required for GTK/Qt Apps)

For the addon to work in **all applications** (Firefox, Kate, etc.), you must set environment variables:

```bash
mkdir -p ~/.config/environment.d
cat > ~/.config/environment.d/fcitx5.conf << 'EOF'
GTK_IM_MODULE=fcitx5
QT_IM_MODULE=fcitx5
XMODIFIERS=@im=fcitx5
EOF
```

**Important:** You must **logout and login** for these variables to take effect!

Verify after login:
```bash
echo $GTK_IM_MODULE   # Should show: fcitx5
```

## Usage

1. Open Fcitx5 configuration:
   ```bash
   fcitx5-config-qt
   ```

2. Add "Schnelle Umlaute" as input method

3. Switch to it (default: <kbd>Ctrl</kbd> + <kbd>Space</kbd>)

4. Type umlauts:

| Hold | Press | Result |
|------|-------|--------|
| <kbd>a</kbd> | <kbd>Space</kbd> | ä |
| <kbd>o</kbd> | <kbd>Space</kbd> | ö |
| <kbd>u</kbd> | <kbd>Space</kbd> | ü |
| <kbd>s</kbd> | <kbd>Space</kbd> | ß |
| <kbd>Shift</kbd> + <kbd>a</kbd> | <kbd>Space</kbd> | Ä |
| <kbd>Shift</kbd> + <kbd>o</kbd> | <kbd>Space</kbd> | Ö |
| <kbd>Shift</kbd> + <kbd>u</kbd> | <kbd>Space</kbd> | Ü |

## Advantages over evdev-rs approach

| Feature | Fcitx5 Addon | evdev-rs |
|---------|--------------|----------|
| Clipboard usage | None | Always |
| Root permissions | Not needed | Required |
| Wayland support | Native | Fallback |
| X11 support | Native | Native |
| Setup complexity | Medium | Simple |
| Universal | Fcitx5 required | Always works |

## Uninstall

```bash
cd build
sudo make uninstall
```

## Development

- **Source**: `src/schnelle-umlaute.cpp`
- **Build system**: CMake
- **Fcitx5 API**: InputMethodEngineV2

## Troubleshooting

### "Schnelle Umlaute nicht verfügbar" / Addon not loading

This usually means the addon was installed to the wrong location.

**Check the fcitx5 log:**
```bash
fcitx5 -r 2>&1 | grep -i schnelle
```

If you see `Could not locate library schnelle-umlaute.so`:

1. Remove wrongly installed files:
   ```bash
   sudo rm -f /usr/local/lib/fcitx5/schnelle-umlaute.so
   sudo rm -f /usr/local/share/fcitx5/addon/schnelle-umlaute.conf*
   sudo rm -f /usr/local/share/fcitx5/inputmethod/schnelle-umlaute.conf
   ```

2. Rebuild with correct prefix:
   ```bash
   ./build.sh
   cd build
   sudo make install
   fcitx5 -r
   ```

### Addon not showing in fcitx5-config-qt

```bash
# Check if addon is installed in correct location
ls /usr/lib/fcitx5/schnelle-umlaute.so
ls /usr/share/fcitx5/addon/schnelle-umlaute.conf
ls /usr/share/fcitx5/inputmethod/schnelle-umlaute.conf

# Restart Fcitx5
fcitx5 -r
```

### Works in terminal but not in Firefox/Kate/other apps

Environment variables are not set. See [Environment Variables](#environment-variables-required-for-gtkqt-apps) section above.

**Remember:** You must logout and login after setting the variables!

### Input method state is not shared across applications

By default, Fcitx5 remembers the input method **per application**. If you switch to "Schnelle Umlaute" in Firefox, the terminal may still use US keyboard.

To share the input method state globally:

1. Open Fcitx5 configuration: `fcitx5-config-qt`
2. Go to **Global Options**
3. Set **Share Input State** to **All**
4. Restart Fcitx5: `fcitx5 -r`

### Build errors

Make sure all dependencies are installed:
```bash
pacman -Q cmake extra-cmake-modules fcitx5
```

## License

GPL-3.0+
