# Schnelle Umlaute | PowerToys Quick Accent Alternative for Linux

<p align="center"><img height="128" src="docs/assets/apple-touch-icon.png" alt="Schnelle Umlaute Icon"></p>

[![License: GPL-3.0](https://img.shields.io/badge/License-GPL--3.0-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-lightgrey)](https://www.linux.org/)
[![Fcitx5 Addon](https://img.shields.io/badge/Fcitx5-Addon-orange)](https://fcitx-im.org/)
[![Sponsor](https://img.shields.io/badge/Sponsor-%E2%9D%A4-pink?logo=github)](https://github.com/sponsors/Maik-0000FF)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-Support%20me-ff5e5b?logo=ko-fi&logoColor=white)](https://ko-fi.com/maik0000ff)

**Linux Alternative to Windows PowerToys Quick Accent** - Fast accent and special character input using hold+space gestures.

Missing **PowerToys Quick Accent** on Linux? This Fcitx5 input method addon lets you type accents, umlauts, emojis, symbols, and text snippets using intuitive hold + space keyboard gestures. Supports accent cycling (é → è → ê → ë) for German, French, Spanish and other languages. Clipboard-free operation on X11 and Wayland.

**Features:**
- Hold letter + space/arrow keys for accent characters
- Accent cycling: Press leader key repeatedly to cycle through variants (á → à → â → ã)
- Snippets: Map single keys to entire text phrases
- Braille Unicode characters (⠁⠃⠉⠙⠑ etc.) support
- Configurable activation keys and unlimited mapping slots
- No clipboard interference, no root permissions
- Works system-wide on X11 and Wayland

- **[Quick Start](#quick-start)**
- **[Usage](#usage)**
- **[How It Works](docs/HOW-IT-WORKS.md)**
- **[Installation](docs/INSTALLATION.md)**
- **[Configuration](docs/CONFIGURATION.md)**
- **[Troubleshooting](docs/TROUBLESHOOTING.md)**
- **[Architecture](docs/ARCHITECTURE.md)**
- **[Upgrading](docs/UPGRADING.md)**

## What Makes This Special?

Unlike clipboard-based or keyboard simulation solutions, this Fcitx5 addon uses **direct text insertion** (`commitString()`):

- **Clipboard stays untouched** - No interference with copy/paste
- **No root permissions required** - Runs as normal user
- **Native X11 and Wayland support** - No compatibility layers
- **Hold & Wait pattern** - Zero latency, no backspace needed
- **Part of Fcitx5** - Not a background daemon

## Quick Start

```bash
git clone https://github.com/Maik-0000FF/schnelle-umlaute.git
cd schnelle-umlaute
./install.sh
```

After installation: **Logout and login**, then run `fcitx5-config-qt` to add "Schnelle Umlaute" to your input methods.

> **Note:** Make sure to uncheck "Only Show Current Language" when searching for the addon.

![Input Method Configuration](docs/assets/screenshot-input-method.png)

## Usage

1. **Switch to Schnelle Umlaute** input method (<kbd>Ctrl</kbd> + <kbd>Space</kbd>) — tray icon shows **"Ää"**
2. **Type umlauts:** Hold a mapped key, then press <kbd>Space</kbd> within the time window
3. **Type normally:** Release the key without pressing <kbd>Space</kbd> — the normal letter appears

### Default Mappings

| Hold | + | Press | = | Result |
|------|---|-------|---|--------|
| <kbd>a</kbd> | + | <kbd>Space</kbd> | = | ä |
| <kbd>o</kbd> | + | <kbd>Space</kbd> | = | ö |
| <kbd>u</kbd> | + | <kbd>Space</kbd> | = | ü |
| <kbd>s</kbd> | + | <kbd>Space</kbd> | = | ß |
| <kbd>Shift</kbd>+<kbd>a</kbd> | + | <kbd>Space</kbd> | = | Ä |
| <kbd>Shift</kbd>+<kbd>o</kbd> | + | <kbd>Space</kbd> | = | Ö |
| <kbd>Shift</kbd>+<kbd>u</kbd> | + | <kbd>Space</kbd> | = | Ü |

All mappings are fully customizable — add French, Spanish, Emoji, Braille, or any Unicode character. See [Configuration](docs/CONFIGURATION.md).

## Requirements

- **Linux** with Fcitx5 (Arch, Ubuntu/Debian, Fedora, openSUSE)
- **CMake** and **extra-cmake-modules**
- **GCC with C++20 support**

## Uninstallation

```bash
./uninstall.sh
```

See [Installation Guide](docs/INSTALLATION.md#uninstallation) for manual uninstallation.

## Contributing

Contributions welcome! This addon is written in **C++20**, uses **Fcitx5 InputMethodEngineV2 API**, and is built with **CMake**.

## Support

> If you find Schnelle Umlaute useful, you can support its development:
>
> <a href="https://github.com/sponsors/Maik-0000FF">
>   <img src="https://img.shields.io/badge/GitHub_Sponsors-Support_this_project-ea4aaa?style=for-the-badge&logo=github" alt="GitHub Sponsors">
> </a>
> &nbsp;
> <a href="https://ko-fi.com/maik0000ff">
>   <img src="https://img.shields.io/badge/Ko--fi-Buy_me_a_coffee-ff5e5b?style=for-the-badge&logo=ko-fi&logoColor=white" alt="Ko-fi">
> </a>
>
> A star also helps — it makes this project easier to discover.

## License

GPL-3.0+

## Author

Created by [Maik-0000FF](https://github.com/Maik-0000FF)

## Credits

Inspired by [Windows PowerToys Quick Accent](https://learn.microsoft.com/en-us/windows/powertoys/quick-accent)

Built with:
- **Fcitx5** - Input Method Framework
- **C++20** - Modern C++ with chrono and optional
- **CMake** - Build system
- **Extra CMake Modules (ECM)** - KDE build tools

Thanks to [wengxt](https://github.com/wengxt) for creating Fcitx5 and for the guidance on building custom config widgets.

---

**Version:** 1.0.0
