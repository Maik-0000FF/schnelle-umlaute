# Schnelle Umlaute | PowerToys Quick Accent Alternative for Linux

<p align="center"><img height="128" src="docs/assets/favicon-1024.png" alt="Schnelle Umlaute Icon"></p>

[![Website](https://img.shields.io/badge/Website-Landing%20Page-4ade80)](https://maik-0000ff.github.io/schnelle-umlaute_Website/)
[![License: GPL-3.0-or-later](https://img.shields.io/badge/License-GPL--3.0--or--later-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![CI](https://github.com/Maik-0000FF/schnelle-umlaute/actions/workflows/ci.yml/badge.svg)](https://github.com/Maik-0000FF/schnelle-umlaute/actions/workflows/ci.yml)
[![Arch Linux](https://img.shields.io/badge/Arch_Linux-1793D1?logo=arch-linux&logoColor=white)](https://aur.archlinux.org/packages/fcitx5-schnelle-umlaute-git)
[![NixOS](https://img.shields.io/badge/NixOS-Flake-5277C3?logo=nixos&logoColor=white)](#nixos)
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
- Configurable leader keys (Space, Arrow ←→↑↓, Alt/AltGr, or custom keys with hand-split), keep Space free for normal typing
- Unlimited mapping slots
- App blacklist/whitelist, disable in games, password managers, or apps with conflicting shortcuts
- Standalone QML editor (`schnelle-umlaute-editor`) for managing mappings, leader keys, app filter, and the cycle overlay
- Optional cycle overlay daemon (Wayland with wlr-layer-shell) showing accent variants on-screen
- No clipboard interference, no root permissions
- Works system-wide on X11 and Wayland

> [!NOTE]
> The on-screen **cycle overlay** needs the Wayland `wlr-layer-shell` protocol,
> so it is unavailable on **GNOME/Mutter** and on **X11**. Every other feature
> (the core accent input, cycling, snippets, the app filter and the editor)
> works everywhere, GNOME and X11 included.

## Demo

**Overlay at the mouse cursor** — the accent indicator follows your pointer while you cycle through variants.

https://github.com/user-attachments/assets/49cd0811-7b50-4e0d-8221-f8c18fe39872

**Overlay at the text caret** — the indicator appears right where you type, so it also works on X11.

https://github.com/user-attachments/assets/5c8e691c-a9b8-4f2b-bddb-98124294d622

### Documentation

- **[Quick Start](#quick-start)**
- **[Usage](#usage)**
- **[How It Works](docs/HOW-IT-WORKS.md)**
- **[Installation](docs/INSTALLATION.md)**
- **[Configuration](docs/CONFIGURATION.md)**
- **[Troubleshooting](docs/TROUBLESHOOTING.md)**
- **[Architecture](docs/ARCHITECTURE.md)**
- **[Upgrading](docs/UPGRADING.md)**
- **[Contributing](CONTRIBUTING.md)**

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

> **`-git`:** Builds the latest code at install time. Run `schnelle-umlaute-setup` once afterwards to set your fcitx5 environment variables and enable autostart, which a package install cannot do in your home directory.

### NixOS

Run it straight from the flake:

```bash
nix run github:Maik-0000FF/schnelle-umlaute
```

On NixOS, add the flake as an input and import the all-in-one module (it enables fcitx5 with the addon and sets the input-method environment variables):

```nix
# flake inputs
schnelle-umlaute.url = "github:Maik-0000FF/schnelle-umlaute";

# in your configuration
imports = [ inputs.schnelle-umlaute.nixosModules.default ];
programs.schnelle-umlaute.enable = true;
```

Already configure fcitx5 yourself? Skip the module and add just the package:

```nix
i18n.inputMethod.fcitx5.addons = [ inputs.schnelle-umlaute.packages.${system}.default ];
```

### Install from source (Arch Linux · Ubuntu/Debian · Linux Mint · Fedora · openSUSE)

```bash
git clone https://github.com/Maik-0000FF/schnelle-umlaute.git
cd schnelle-umlaute
./install.sh
```

After installation: **Logout and login**, then run `fcitx5-config-qt` to add "Schnelle Umlaute" to your input methods. See [Configuration](docs/CONFIGURATION.md) for details and screenshots.

### Previous version (without the overlay and editor)

The version from before the cycle overlay and the standalone editor is kept on the [`legacy`](https://github.com/Maik-0000FF/schnelle-umlaute/tree/legacy) branch. Build it from source the same way as the main branch.

## Usage

1. **Switch to Schnelle Umlaute** input method (<kbd>Ctrl</kbd> + <kbd>Space</kbd>), tray icon shows **"Ää"**
2. **Type umlauts:** Hold a mapped key, then press <kbd>Space</kbd> within the time window
3. **Type normally:** Release the key without pressing <kbd>Space</kbd>, the normal letter appears

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

All mappings are fully customizable, add French, Spanish, Emoji, Braille, or any Unicode character. See [Configuration](docs/CONFIGURATION.md).

> **Tip:** When typing fast, Space as leader key can cause accidental accents at word boundaries, e.g. "une pomme chaque" becomes "une pomméchaque" (the `e` is still held when Space is pressed, so Space gets consumed as the leader and the word separator is lost). Switch to an arrow key, Alt, or a custom leader (e.g. `f`, `j`) to keep Space free for normal typing. See [Configuration → Leader Key](docs/CONFIGURATION.md#leader-key) and [Troubleshooting](docs/TROUBLESHOOTING.md#accidental-accents-when-typing-fast).

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

## Contributing

Contributions are welcome, especially improving the **character presets**. If you are a **native speaker**, your help making your language's preset complete and correct is greatly appreciated. The presets are plain text files that are quick to edit, no build required. See the [Contributing guide](CONTRIBUTING.md) for the preset format and how to submit a change.

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
> A star also helps, it makes this project easier to discover.

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

**Version:** 1.4.0
