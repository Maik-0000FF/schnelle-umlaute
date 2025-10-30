# Schnelle Umlaute - Fcitx5 C++ Addon

Quick German umlaut input using Fcitx5 native addon.

## Features

- ✅ **No clipboard usage** - Direct text insertion via `commitString()`
- ✅ **No root permissions** - Runs as user process
- ✅ **X11 + Wayland** - Native support for both
- ✅ **Hold & Wait** - Hold letter + Leader key → umlaut
- ✅ **Zero latency** - No backspace needed
- ✅ **Configurable leader key** - Space, Arrow keys, or combinations
- ✅ **20 custom mappings** - Define your own character transformations

## How it works

1. Hold a letter key (a, o, u, s - configurable)
2. Press leader key within the time window:
   - **Leader key**: Space by default (configurable to Arrow keys or combinations)
   - **400ms** for lowercase letters (a, o, u, s)
   - **700ms** for uppercase letters (A, O, U)
3. → Get the umlaut! (ä, ö, ü, ß, Ä, Ö, Ü - or your custom output)

**Note:** You can keep Shift pressed while pressing the leader key for uppercase umlauts

## Configuration

All settings can be configured via `fcitx5-config-qt`:

- **Delay times** (50-2000ms range)
- **Leader key** (Space, Left/Right Arrow, or combinations)
- **Character mappings** (20 slots: Input → Output)
  - Default: German umlauts (a→ä, o→ö, u→ü, s→ß, A→Ä, O→Ö, U→Ü)
  - Customize for other languages or shortcuts
  - Press "Defaults" button to restore German umlauts

## Build Requirements

```bash
sudo pacman -S cmake extra-cmake-modules fcitx5
```

## Build & Install

```bash
# Build
./build.sh

# Install
cd build
sudo make install

# Restart Fcitx5
fcitx5 -r
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
| Clipboard usage | ✅ None | ❌ Always |
| Root permissions | ✅ Not needed | ❌ Required |
| Wayland support | ✅ Native | ⚠️ Fallback |
| X11 support | ✅ Native | ✅ Native |
| Setup complexity | ⚠️ Medium | ✅ Simple |
| Universal | ⚠️ Fcitx5 required | ✅ Always works |

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

### Addon not showing in fcitx5-config-qt

```bash
# Check if addon is installed
ls /usr/lib/fcitx5/schnelle-umlaute.so

# Check addon configuration
ls /usr/share/fcitx5/addon/schnelle-umlaute.conf

# Restart Fcitx5
fcitx5 -r
```

### Build errors

Make sure all dependencies are installed:
```bash
pacman -Q cmake extra-cmake-modules fcitx5
```

## License

GPL-3.0+
