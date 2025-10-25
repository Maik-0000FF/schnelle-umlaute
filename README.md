# Schnelle Umlaute - Fcitx5 Edition

> **Quick German Umlaut Input for Linux** - The clipboard-free solution!

Type German umlauts (ä, ö, ü, ß) on US keyboard layouts with a simple **hold + space** gesture, powered by Fcitx5.

## 🎯 What Makes This Special?

Unlike other solutions that use clipboard manipulation or keyboard simulation, this Fcitx5 addon uses **direct text insertion** (`commitString()`), which means:

✅ **NO clipboard interference** - Your clipboard stays untouched
✅ **NO root permissions** - Runs as normal user
✅ **Works on X11 AND Wayland** - Native support for both
✅ **Zero latency** - Hold & Wait pattern, no backspace needed
✅ **Perfect integration** - Part of Fcitx5, not a background daemon

## 🚀 How It Works

### Gesture Flow

```mermaid
stateDiagram-v2
    [*] --> Waiting: Press 'a'
    Waiting --> Umlaut: Space within 400ms
    Waiting --> Normal: Release or timeout
    Umlaut --> [*]: ä ✨
    Normal --> [*]: a
```

### Step-by-Step Guide

#### 🔤 Lowercase Umlauts (400ms)

```mermaid
graph LR
    A["① Hold 'a'"] --> B["② Tap Space<br/>(within 400ms)"]
    B --> C["③ ä appears ✨"]

    style A fill:#64b5f6,stroke:#1976d2,stroke-width:3px,color:#000
    style B fill:#ffd54f,stroke:#f57f17,stroke-width:3px,color:#000
    style C fill:#81c784,stroke:#388e3c,stroke-width:3px,color:#000
```

**Works for:** `a`→`ä` | `o`→`ö` | `u`→`ü` | `s`→`ß`

#### 🔠 Uppercase Umlauts (700ms - longer for coordination)

```mermaid
graph LR
    A["① Hold Shift+A"] --> B["② Tap Space<br/>(within 700ms)<br/>Shift stays pressed!"]
    B --> C["③ Ä appears ✨"]

    style A fill:#64b5f6,stroke:#1976d2,stroke-width:3px,color:#000
    style B fill:#ffd54f,stroke:#f57f17,stroke-width:3px,color:#000
    style C fill:#81c784,stroke:#388e3c,stroke-width:3px,color:#000
```

**Works for:** `A`→`Ä` | `O`→`Ö` | `U`→`Ü`

## 📋 Requirements

- **Arch Linux** (or compatible distro)
- **Fcitx5** - Input Method Framework
- **CMake** and **extra-cmake-modules** - For building
- **GCC with C++20 support** - For compilation

## 📦 Installation

### Quick Start (Recommended)

The easiest way to install:

```bash
git clone https://github.com/Maik-0000FF/schnelle-umlaute.git
cd schnelle-umlaute
./install.sh
```

The script will:
- Check and install dependencies
- Build and install the addon
- Configure environment variables automatically
- Guide you through the setup

**After installation:** Logout and login, then run `fcitx5-config-qt` to add "Schnelle Umlaute" to your input methods.

### Install via AUR (Coming Soon)

For Arch Linux users, the package will be available on AUR:

```bash
yay -S schnelle-umlaute-fcitx5
# or
paru -S schnelle-umlaute-fcitx5
```

### Manual Installation

If you prefer manual installation:

**1. Install Dependencies**

```bash
sudo pacman -S fcitx5 fcitx5-configtool cmake extra-cmake-modules gcc
```

**2. Build the Addon**

```bash
cd addon
./build.sh
```

**3. Install**

```bash
cd build
sudo make install
```

**4. Configure Environment Variables**

For the addon to work in ALL applications (GTK, Qt, browsers, terminals, etc.), set up environment variables:

```bash
mkdir -p ~/.config/environment.d
cat > ~/.config/environment.d/fcitx5.conf << 'EOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
GLFW_IM_MODULE=ibus
EOF
```

**Note:** `GLFW_IM_MODULE=ibus` is required for Kitty terminal and other GLFW-based applications.

**5. Logout and Login**

**IMPORTANT:** You must logout and login again for the environment variables to take effect!

```bash
# After logout/login, verify Fcitx5 is running:
fcitx5 -r
```

## 🎮 Setup & Usage

### Configure Fcitx5

1. Open Fcitx5 configuration:
   ```bash
   fcitx5-config-qt
   ```

2. Go to **"Input Method"** tab (Eingabemethode)

3. Click **"+"** to add a new input method

4. Search for **"Schnelle Umlaute"**

5. Add it to your input methods

6. Click **"Apply"** or **"OK"**

### Using the Addon

1. **Switch to Schnelle Umlaute** input method (default: `Ctrl+Space`)

2. **Type umlauts:**

| Want | Hold        | Press | Result |
|------|-------------|-------|--------|
| ä    | a           | Space | ä      |
| ö    | o           | Space | ö      |
| ü    | u           | Space | ü      |
| ß    | s           | Space | ß      |
| Ä    | A (Shift+a) | Space | Ä      |
| Ö    | O (Shift+o) | Space | Ö      |
| Ü    | U (Shift+u) | Space | Ü      |

3. **Type normally:** If you don't press Space within the time window, the normal letter appears

## 🏗️ Architecture

This is a **native Fcitx5 addon** written in **C++**, using the Fcitx5 InputMethodEngineV2 API.

**Key Components:**
- `addon/src/schnelle-umlaute.cpp` - Main addon logic with Hold & Wait implementation
- `addon/CMakeLists.txt` - Build configuration
- `addon/data/schnelle-umlaute.conf` - Fcitx5 addon registration

**How it works internally:**
1. Fcitx5 calls our `keyEvent()` handler for every key
2. When accent key (a/o/u/s) is pressed: suppress output, start timer
3. If Space within delay (400ms lowercase, 700ms uppercase): call `commitString(umlaut)` for direct insertion
4. If timeout or key released: call `commitString(normalLetter)`
5. No clipboard, no key simulation - pure text insertion!

## 🆚 Comparison with Other Approaches

| Approach | Clipboard-Free | No Root | X11 | Wayland | Complexity |
|----------|---------------|---------|-----|---------|------------|
| **Fcitx5 Addon (This)** | ✅ | ✅ | ✅ | ✅ | Medium |
| evdev-rs + xclip | ❌ | ❌ | ✅ | ⚠️ | Low |
| IBus | ✅ | ✅ | ✅ | ✅ | High |
| XTest | ✅ | ❌ | ✅ | ❌ | Low |

## 🐛 Troubleshooting

### Addon not showing in fcitx5-config-qt

Check if all files are installed:
```bash
ls /usr/lib/fcitx5/schnelle-umlaute.so
ls /usr/share/fcitx5/addon/schnelle-umlaute.conf
ls /usr/share/fcitx5/inputmethod/schnelle-umlaute.conf  # This is important!
```

If missing, reinstall:
```bash
cd addon/build
sudo make install
fcitx5 -r
```

### Works in terminal but not in other apps (Firefox, Kate, etc.)

You need to set environment variables and **logout/login**:

```bash
mkdir -p ~/.config/environment.d
cat > ~/.config/environment.d/fcitx5.conf << 'EOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
EOF
```

Then **logout and login again** for changes to take effect.

### Umlauts not appearing

1. Make sure you're switched to "Schnelle Umlaute" input method (Ctrl+Space)
2. Check Fcitx5 is running: `ps aux | grep fcitx5`
3. Try holding the key longer before pressing Space
4. Verify environment variables are set: `echo $GTK_IM_MODULE` (should output "fcitx")

### KDE Wayland users: "Fcitx should be launched by KWin" warning

If you see a warning about Fcitx5 not being launched by KWin, fix it for optimal Wayland experience:

1. Open **System Settings** → **Virtual Keyboard** (or search for "virtuell")
2. Select **"Fcitx 5"** from the dropdown (instead of "None")
3. Apply changes
4. **Restart your session** (logout/login)

This enables the native Wayland input method protocol and eliminates the warning.

### Kitty terminal not working

If the addon doesn't work in Kitty terminal, make sure you have set `GLFW_IM_MODULE=ibus` in your environment variables (see step 4 of Installation), then **logout and login**.

### Build errors

Make sure you have C++20 support:
```bash
gcc --version  # Should be 11 or newer
```

## 🗑️ Uninstallation

### Quick Uninstall (Recommended)

```bash
./uninstall.sh
```

The script will:
- Remove all installed files
- Ask if you want to remove environment configuration
- Restart Fcitx5

### Manual Uninstallation

```bash
cd addon/build
sudo make uninstall
fcitx5 -r
```

Or remove files manually:
```bash
sudo rm /usr/lib/fcitx5/schnelle-umlaute.so
sudo rm /usr/share/fcitx5/addon/schnelle-umlaute.conf
sudo rm /usr/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml
sudo rm /usr/share/fcitx5/inputmethod/schnelle-umlaute.conf
rm ~/.config/environment.d/fcitx5.conf  # Optional: remove environment config
fcitx5 -r
```

## 📚 Documentation

- **[INSTALL.md](INSTALL.md)** - Complete installation guide with troubleshooting
- **[addon/README.md](addon/README.md)** - Detailed addon documentation
- **[CLAUDE.md](CLAUDE.md)** - Developer context and design decisions

## 🤝 Contributing

Contributions welcome! This addon is:
- Written in **C++20**
- Uses **Fcitx5 InputMethodEngineV2 API**
- Built with **CMake**

## 📄 License

GPL-3.0+

## 👨‍💻 Author

Created by [Maik-0000FF](https://github.com/Maik-0000FF)

## 🙏 Credits

Inspired by [Windows PowerToys Quick Accent](https://learn.microsoft.com/en-us/windows/powertoys/quick-accent)

Built with:
- **Fcitx5** - Input Method Framework
- **C++20** - Modern C++ with chrono and optional
- **CMake** - Build system
- **Extra CMake Modules (ECM)** - KDE build tools

---

**Version:** 0.1.1
**Status:** Working - tested and functional
**Date:** 2025-10-25
