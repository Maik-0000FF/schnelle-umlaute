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

1. Hold a letter key (a, o, u, s - configurable)
2. Press leader key within the time window:
   - **Leader key**: Space by default (configurable to Arrow keys or combinations)
   - **400ms** for lowercase letters (a, o, u, s)
   - **700ms** for uppercase letters (A, O, U)
3. → Get the umlaut! (ä, ö, ü, ß, Ä, Ö, Ü - or your custom output)

**Note:** You can keep Shift pressed while pressing the leader key for uppercase umlauts

## Accent Cycling

You can define multiple outputs per input using **comma-separated values** in a single output field.

**Example 1 - French e:**
- Input: `e`
- Output: `é,è,ê,ë`

**Example 2 - German/French a:**
- Input: `a`
- Output: `ä,à,â,æ`

**Usage:**
1. Hold the key (e.g. `a`)
2. Press leader key (Space) → first character (`ä`)
3. Press leader key again → next character (`à`)
4. Press leader key again → next character (`â`)
5. Keep pressing → cycles through all defined characters

The previous character gets replaced automatically when cycling.

## Emojis, Symbols and Snippets

Outputs are not limited to single characters. You can also use:

**Emojis:**
- Input: `h`
- Output: `❤️,💜,💙,💚`

**Special symbols:**
- Input: `p`
- Output: `π,φ,θ,Ω`

**Math symbols:**
- Input: `m`
- Output: `±,×,÷,≠,≈,∞`

**Short text snippets:**
- Input: `@`
- Output: `mail@example.com`

**Common phrases:**
- Input: `g`
- Output: `Good morning!,Good afternoon!,Good evening!`

This makes the addon useful beyond accents - use it for any frequently typed text or symbols.

## Configuration

All settings can be configured via `fcitx5-config-qt`:

- **Delay times** (50-2000ms range)
- **Leader key** (Space, Left/Right Arrow, or combinations)
- **Character mappings** (20 slots: Input → Output)
  - Default: German umlauts (a→ä, o→ö, u→ü, s→ß, A→Ä, O→Ö, U→Ü)
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
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
EOF
```

**Important:** You must **logout and login** for these variables to take effect!

Verify after login:
```bash
echo $GTK_IM_MODULE   # Should show: fcitx
```

## Usage

1. Open Fcitx5 configuration:
   ```bash
   fcitx5-config-qt
   ```

2. Add "Schnelle Umlaute" as input method

3. Switch to it (default: Ctrl+Space)

4. Type umlauts:
   - Hold `a` + Space → ä
   - Hold `o` + Space → ö
   - Hold `u` + Space → ü
   - Hold `s` + Space → ß

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
