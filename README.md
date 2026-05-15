# Schnelle Umlaute | PowerToys Quick Accent Alternative for Linux

<p align="center"><img height="128" src="docs/assets/favicon-1024.png" alt="Schnelle Umlaute Icon"></p>

[![Website](https://img.shields.io/badge/Website-Landing%20Page-4ade80)](https://maik-0000ff.github.io/schnelle-umlaute/)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/License-GPL--3.0--or--later-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![CI](https://github.com/Maik-0000FF/schnelle-umlaute/actions/workflows/ci.yml/badge.svg)](https://github.com/Maik-0000FF/schnelle-umlaute/actions/workflows/ci.yml)
[![Arch Linux](https://img.shields.io/badge/Arch_Linux-1793D1?logo=arch-linux&logoColor=white)](https://aur.archlinux.org/packages/fcitx5-schnelle-umlaute-git)
![Ubuntu/Debian](https://img.shields.io/badge/Ubuntu%2FDebian-E95420?logo=ubuntu&logoColor=white)
![Linux Mint](https://img.shields.io/badge/Linux%20Mint-87CF3E?logo=linuxmint&logoColor=white)
![Fedora](https://img.shields.io/badge/Fedora-294172?logo=fedora&logoColor=white)
![openSUSE](https://img.shields.io/badge/openSUSE-73BA25?logo=opensuse&logoColor=white)
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
- Configurable leader keys (Space, Arrow ←→↑↓, Alt/AltGr, or custom keys with hand-split) — keep Space free for normal typing
- Unlimited mapping slots
- App blacklist/whitelist — disable in games, password managers, or apps with conflicting shortcuts
- Standalone QML editor (`schnelle-umlaute-editor`) for managing mappings, leader keys, app filter, and the cycle overlay
- Optional cycle overlay daemon (Wayland with wlr-layer-shell) showing accent variants on-screen
- No clipboard interference, no root permissions
- Works system-wide on X11 and Wayland

### Documentation

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

### Arch Linux (AUR)

Available on the [AUR](https://aur.archlinux.org/packages/fcitx5-schnelle-umlaute-git). Install with any AUR helper:

```bash
yay -S fcitx5-schnelle-umlaute-git
```

> **About `-git`:** This package builds from the `dev` branch and ships features ahead of the latest tagged release — the standalone QML editor (`schnelle-umlaute-editor`), the cycle overlay daemon, and the per-user setup script (`schnelle-umlaute-setup`). Run `schnelle-umlaute-setup` once after install to write your fcitx5 environment variables and set up autostart. As a safety net, the editor also detects missing input-method variables on startup and offers to write them — but it does not configure autostart, so running `schnelle-umlaute-setup` is still the complete path.
>
> A stable AUR package (`fcitx5-schnelle-umlaute`) tracking tagged releases will be added in a future update.

### Install from source (Arch Linux · Ubuntu/Debian · Linux Mint · Fedora · openSUSE)

```bash
git clone https://github.com/Maik-0000FF/schnelle-umlaute.git
cd schnelle-umlaute
./install.sh
```

After installation: **Logout and login**, then run `fcitx5-config-qt` to add "Schnelle Umlaute" to your input methods. See [Configuration](docs/CONFIGURATION.md) for details and screenshots.

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

> **Tip:** When typing fast, Space as leader key can cause accidental accents at word boundaries — e.g. "une pomme chaque" becomes "une pomméchaque" (the `e` is still held when Space is pressed, so Space gets consumed as the leader and the word separator is lost). Switch to an arrow key, Alt, or a custom leader (e.g. `f`, `j`) to keep Space free for normal typing. See [Configuration → Leader Key](docs/CONFIGURATION.md#leader-key) and [Troubleshooting](docs/TROUBLESHOOTING.md#accidental-accents-when-typing-fast).

> **Optional:** An on-screen cycling indicator is available on Wayland compositors with wlr-layer-shell support (KDE Plasma, sway, Hyprland, …). See [Configuration → Cycle Overlay](docs/CONFIGURATION.md#cycle-overlay).

## Requirements

- **Linux** with Fcitx5 (Arch, Ubuntu/Debian, Linux Mint, Fedora, openSUSE)
- **CMake** and **extra-cmake-modules**
- **GCC with C++20 support**

## Uninstallation

```bash
./uninstall.sh
```

See [Installation Guide](docs/INSTALLATION.md#uninstallation) for manual uninstallation.

## Feedback

Found a bug or have a feature idea? Open an [issue](https://github.com/Maik-0000FF/schnelle-umlaute/issues).

## Support

> If you find Schnelle Umlaute useful, you can support its development:
>
> <a href="https://github.com/sponsors/Maik-0000FF">
>   <img src="https://img.shields.io/badge/Sponsors-Support_this_project-ea4aaa?style=for-the-badge&logo=github" alt="GitHub Sponsors">
> </a>
> &nbsp;
> <a href="https://ko-fi.com/maik0000ff">
>   <img src="https://img.shields.io/badge/Ko--fi-Buy_me_a_coffee-ff5e5b?style=for-the-badge&logo=ko-fi&logoColor=white" alt="Ko-fi">
> </a>
>
> A star also helps — it makes this project easier to discover.

## License

GPL-3.0-or-later

## Author

Created by [Maik-0000FF](https://github.com/Maik-0000FF)

## Credits

Inspired by [PowerAccent](https://github.com/damienleroy/PowerAccent) by Damien Leroy, which was later integrated into [Windows PowerToys](https://learn.microsoft.com/en-us/windows/powertoys/quick-accent) as Quick Accent.

Built with:
- **Fcitx5** - Input Method Framework
- **C++20** - Modern C++ with chrono and optional
- **CMake** - Build system
- **Extra CMake Modules (ECM)** - KDE build tools

Thanks to [wengxt](https://github.com/wengxt) for creating Fcitx5 and for the guidance on building custom config widgets.

---

**Version:** 1.2.1
