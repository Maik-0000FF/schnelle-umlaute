# schnelle-umlaute-fcitx5

Quick German umlaut input for Fcitx5 using hold and wait gesture.

## Description

Schnelle Umlaute is a native Fcitx5 addon that enables fast German umlaut input on US keyboard layouts. Simply hold a letter key and press Space within the time window to get the corresponding umlaut.

**Example:** Hold `a` + press Space → `ä`

## Features

- No clipboard interference
- No root permissions required
- Works on X11 and Wayland
- Adaptive timing (400ms lowercase, 700ms uppercase)
- Native Fcitx5 integration using direct text insertion

## Installation

```bash
yay -S schnelle-umlaute-fcitx5
# or
paru -S schnelle-umlaute-fcitx5
```

## Post-Installation Setup

**1. Configure environment variables:**

```bash
mkdir -p ~/.config/environment.d
cat > ~/.config/environment.d/fcitx5.conf << 'EOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
GLFW_IM_MODULE=ibus
EOF
```

**2. Logout and login** for changes to take effect.

**3. Add input method:**

```bash
fcitx5-config-qt
```

- Go to "Input Method" tab
- Click "+" to add
- Search for "Schnelle Umlaute"
- Add it and apply

**4. Switch to it** using `Ctrl+Space` and test by holding `a` and pressing Space.

## Usage

| Want | Hold        | Press | Result |
|------|-------------|-------|--------|
| ä    | a           | Space | ä      |
| ö    | o           | Space | ö      |
| ü    | u           | Space | ü      |
| ß    | s           | Space | ß      |
| Ä    | A (Shift+a) | Space | Ä      |
| Ö    | O (Shift+o) | Space | Ö      |
| Ü    | U (Shift+u) | Space | Ü      |

## Troubleshooting

Run diagnostics:
```bash
fcitx5-diagnose
```

See full documentation: https://github.com/Maik-0000FF/schnelle-umlaute

## Links

- GitHub: https://github.com/Maik-0000FF/schnelle-umlaute
- Issues: https://github.com/Maik-0000FF/schnelle-umlaute/issues
