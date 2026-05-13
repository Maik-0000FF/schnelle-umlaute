#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Schnelle Umlaute - Installation${NC}"
echo -e "${BLUE}========================================${NC}"
echo

# --- Distribution Detection ---

detect_distro() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        case "$ID" in
            arch|manjaro|endeavouros|garuda|artix|cachyos)
                echo "arch" ;;
            debian|ubuntu|linuxmint|pop|kali|elementary|zorin|mx|neon)
                echo "debian" ;;
            fedora|nobara)
                echo "fedora" ;;
            opensuse*|suse)
                echo "suse" ;;
            *)
                # Fallback to ID_LIKE
                case "${ID_LIKE:-}" in
                    *arch*)                 echo "arch" ;;
                    *debian*|*ubuntu*)      echo "debian" ;;
                    *fedora*)               echo "fedora" ;;
                    *suse*)                 echo "suse" ;;
                    *)                      echo "unknown" ;;
                esac ;;
        esac
    elif command -v pacman >/dev/null 2>&1; then
        echo "arch"
    elif command -v apt >/dev/null 2>&1; then
        echo "debian"
    elif command -v dnf >/dev/null 2>&1; then
        echo "fedora"
    elif command -v zypper >/dev/null 2>&1; then
        echo "suse"
    else
        echo "unknown"
    fi
}

DISTRO=$(detect_distro)
PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"

# Show detected distro
if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO_NAME="${PRETTY_NAME:-$ID}"
else
    DISTRO_NAME="Unknown"
fi

echo -e "${BLUE}Distribution:${NC} $DISTRO_NAME"
echo

# Map os-release ID to a user-facing label per derivative. The DISTRO
# family below still drives the package-manager paths; this only affects
# the displayed "X installer" line so e.g. a Mint user sees "Linux Mint
# installer" instead of the generic "Debian/Ubuntu installer".
case "${ID:-}" in
    arch)                 FAMILY_LABEL="Arch Linux" ;;
    manjaro)              FAMILY_LABEL="Manjaro" ;;
    endeavouros)          FAMILY_LABEL="EndeavourOS" ;;
    garuda)               FAMILY_LABEL="Garuda" ;;
    artix)                FAMILY_LABEL="Artix" ;;
    cachyos)              FAMILY_LABEL="CachyOS" ;;
    debian)               FAMILY_LABEL="Debian" ;;
    ubuntu)               FAMILY_LABEL="Ubuntu" ;;
    linuxmint)            FAMILY_LABEL="Linux Mint" ;;
    pop)                  FAMILY_LABEL="Pop!_OS" ;;
    kali)                 FAMILY_LABEL="Kali Linux" ;;
    elementary)           FAMILY_LABEL="elementary OS" ;;
    zorin)                FAMILY_LABEL="Zorin OS" ;;
    mx)                   FAMILY_LABEL="MX Linux" ;;
    neon)                 FAMILY_LABEL="KDE neon" ;;
    fedora)               FAMILY_LABEL="Fedora" ;;
    nobara)               FAMILY_LABEL="Nobara" ;;
    opensuse-tumbleweed)  FAMILY_LABEL="openSUSE Tumbleweed" ;;
    opensuse-leap)        FAMILY_LABEL="openSUSE Leap" ;;
    opensuse*)            FAMILY_LABEL="openSUSE" ;;
    *)
        case "$DISTRO" in
            arch)    FAMILY_LABEL="Arch Linux derivative" ;;
            debian)  FAMILY_LABEL="Debian/Ubuntu derivative" ;;
            fedora)  FAMILY_LABEL="Fedora derivative" ;;
            suse)    FAMILY_LABEL="openSUSE derivative" ;;
        esac
        ;;
esac

case "$DISTRO" in
    arch)
        echo -e "${GREEN}${FAMILY_LABEL} installer${NC}"
        ;;
    debian)
        echo -e "${GREEN}${FAMILY_LABEL} installer${NC}"
        echo -e "${YELLOW}Supported: Ubuntu 24.04+, Debian Trixie (13)+, Linux Mint 22+, Kali Linux (rolling)${NC}"
        echo -e "${YELLOW}Debian Bookworm (12) requires bookworm-backports enabled.${NC}"
        ;;
    fedora)
        echo -e "${GREEN}${FAMILY_LABEL} installer${NC}"
        ;;
    suse)
        echo -e "${GREEN}${FAMILY_LABEL} installer${NC}"
        ;;
    *)
        echo -e "${RED}Error: Unsupported distribution: $DISTRO_NAME${NC}"
        echo
        echo -e "${YELLOW}Supported distributions:${NC}"
        echo "  - Arch Linux and derivatives (Manjaro, EndeavourOS, Garuda, CachyOS, ...)"
        echo "  - Debian and derivatives (Ubuntu, Linux Mint, Pop!_OS, Kali, ...)"
        echo "  - Fedora and derivatives (Nobara, ...)"
        echo "  - openSUSE (Tumbleweed, Leap)"
        echo
        echo "Manual build:"
        echo "  1. Install: fcitx5, fcitx5 dev libraries, cmake, extra-cmake-modules, g++"
        echo "  2. cd addon && mkdir build && cd build && cmake .. && make -j\$(nproc)"
        echo "  3. sudo cmake --install ."
        exit 1
        ;;
esac
echo

# Check if running with sudo (should NOT be)
if [ "$EUID" -eq 0 ]; then
    echo -e "${RED}Error: Do not run this script with sudo!${NC}"
    echo "Run as regular user. Sudo will be requested when needed."
    exit 1
fi

# Warn about sudo requirement
echo -e "${YELLOW}Note: This script will require sudo access for:${NC}"
echo "  - Installing dependencies (if missing)"
echo "  - Installing the addon to system directories"
echo "You may be prompted for your password."
echo

# --- Dependency Management ---

is_installed() {
    case "$DISTRO" in
        arch)           pacman -Q "$1" >/dev/null 2>&1 ;;
        debian)         dpkg -l "$1" 2>/dev/null | grep -q "^ii" ;;
        fedora|suse)    rpm -q "$1" >/dev/null 2>&1 ;;
    esac
}

install_deps() {
    case "$DISTRO" in
        arch)
            sudo pacman -S --needed "$@"
            ;;
        debian)
            echo -e "${BLUE}Updating package list...${NC}"
            sudo apt update
            sudo apt install -y "$@"
            ;;
        fedora)
            sudo dnf install -y "$@"
            ;;
        suse)
            sudo zypper install -y "$@"
            ;;
    esac
}

case "$DISTRO" in
    arch)
        DEPS=(fcitx5 fcitx5-configtool fcitx5-qt fcitx5-gtk cmake extra-cmake-modules gcc pkgconf)
        ;;
    debian)
        DEPS=(fcitx5 fcitx5-config-qt fcitx5-frontend-gtk3 fcitx5-frontend-gtk4
              fcitx5-frontend-qt5 libfcitx5core-dev fcitx5-modules-dev qt6-base-dev
              libfcitx5-qt6-dev cmake extra-cmake-modules g++ gettext pkg-config
              libxkbcommon-dev)
        ;;
    fedora)
        DEPS=(fcitx5 fcitx5-configtool fcitx5-gtk fcitx5-qt6
              fcitx5-devel fcitx5-qt-devel qt6-qtbase-devel
              cmake extra-cmake-modules gcc-c++ gettext)
        ;;
    suse)
        DEPS=(fcitx5 fcitx5-configtool fcitx5-gtk3 fcitx5-gtk4 fcitx5-qt6
              fcitx5-devel fcitx5-qt-devel qt6-base-devel libxkbcommon-devel
              cmake extra-cmake-modules gcc-c++ gettext)
        ;;
esac

MISSING_DEPS=()

echo -e "${YELLOW}Checking dependencies...${NC}"
for dep in "${DEPS[@]}"; do
    if is_installed "$dep"; then
        echo -e "  ${GREEN}✓${NC} $dep"
    else
        echo -e "  ${RED}✗${NC} $dep (missing)"
        MISSING_DEPS+=("$dep")
    fi
done
echo

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo -e "${YELLOW}Missing dependencies: ${MISSING_DEPS[*]}${NC}"
    read -p "Install missing dependencies? [Y/n] " -r
    echo
    if [[ ! $REPLY =~ ^[Nn]$ ]]; then
        echo -e "${BLUE}Installing dependencies...${NC}"
        install_deps "${MISSING_DEPS[@]}"
        echo -e "${GREEN}✓ Dependencies installed${NC}"
        echo
    else
        echo -e "${RED}Cannot proceed without dependencies.${NC}"
        exit 1
    fi
else
    echo -e "${GREEN}✓ All dependencies already installed${NC}"
    echo
fi

# --- Build ---

echo -e "${BLUE}Building addon...${NC}"
cd "$PROJECT_ROOT/addon" || { echo -e "${RED}Error: addon directory not found${NC}"; exit 1; }

rm -rf build
mkdir -p build
cd build

echo "Configuring with CMake..."
cmake ..

echo "Building..."
make -j"$(nproc)"

echo -e "${GREEN}✓ Build successful!${NC}"
echo

# --- Remove Stale Installations ---

# Check ALL possible paths regardless of detected distro.
# Searching paths that don't exist is harmless; missing paths is not.
STALE_CANDIDATES=(
    /usr/lib/fcitx5/schnelle-umlaute.so
    /usr/lib/fcitx5/qt6/libschnelle-umlaute-config-editor.so
    /usr/lib64/fcitx5/schnelle-umlaute.so
    /usr/lib64/fcitx5/qt6/libschnelle-umlaute-config-editor.so
    /usr/lib/x86_64-linux-gnu/fcitx5/schnelle-umlaute.so
    /usr/lib/x86_64-linux-gnu/fcitx5/qt6/libschnelle-umlaute-config-editor.so
    /usr/lib/aarch64-linux-gnu/fcitx5/schnelle-umlaute.so
    /usr/lib/aarch64-linux-gnu/fcitx5/qt6/libschnelle-umlaute-config-editor.so
    /usr/share/fcitx5/addon/schnelle-umlaute.conf
    /usr/share/fcitx5/addon/schnelle-umlaute.conf.in
    /usr/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml
    /usr/share/fcitx5/inputmethod/schnelle-umlaute.conf
    /usr/local/lib/fcitx5/schnelle-umlaute.so
    /usr/local/lib/fcitx5/qt6/libschnelle-umlaute-config-editor.so
    /usr/local/lib64/fcitx5/schnelle-umlaute.so
    /usr/local/lib64/fcitx5/qt6/libschnelle-umlaute-config-editor.so
    /usr/local/lib/x86_64-linux-gnu/fcitx5/schnelle-umlaute.so
    /usr/local/lib/x86_64-linux-gnu/fcitx5/qt6/libschnelle-umlaute-config-editor.so
    /usr/local/lib/aarch64-linux-gnu/fcitx5/schnelle-umlaute.so
    /usr/local/lib/aarch64-linux-gnu/fcitx5/qt6/libschnelle-umlaute-config-editor.so
    /usr/local/share/fcitx5/addon/schnelle-umlaute.conf
    /usr/local/share/fcitx5/addon/schnelle-umlaute.conf.in
    /usr/local/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml
    /usr/local/share/fcitx5/inputmethod/schnelle-umlaute.conf
)

STALE_FILES=()
for stale in "${STALE_CANDIDATES[@]}"; do
    if [ -f "$stale" ]; then
        STALE_FILES+=("$stale")
    fi
done

# Fallback: find any file containing "schnelle-umlaute" or "SchnelleUmlaute"
# under fcitx5 system directories. Catches any legacy files we missed.
SEARCH_ROOTS=(
    /usr/lib /usr/lib64 /usr/local/lib /usr/local/lib64
    /usr/share/fcitx5 /usr/local/share/fcitx5
)
for root in "${SEARCH_ROOTS[@]}"; do
    [ -d "$root" ] || continue
    while IFS= read -r found; do
        skip=0
        for existing in "${STALE_FILES[@]}"; do
            [ "$existing" = "$found" ] && skip=1 && break
        done
        [ $skip -eq 0 ] && STALE_FILES+=("$found")
    done < <(find "$root" -path "*/fcitx5/*" \( -iname "*schnelle*umlaute*" -o -iname "*SchnelleUmlaute*" \) -type f 2>/dev/null)
done

if [ ${#STALE_FILES[@]} -ne 0 ]; then
    echo -e "${YELLOW}Found previous installation files:${NC}"
    for file in "${STALE_FILES[@]}"; do
        echo "  - $file"
    done
    echo
    read -p "Remove before reinstalling? [Y/n] " -r
    echo
    if [[ ! $REPLY =~ ^[Nn]$ ]]; then
        sudo rm -f "${STALE_FILES[@]}"
        echo -e "${GREEN}✓ Old installation removed${NC}"
    else
        echo -e "${RED}Warning: Old files may conflict with the new installation!${NC}"
    fi
    echo
fi

# --- Install ---

echo -e "${BLUE}Installing addon...${NC}"
sudo cmake --install .
echo -e "${GREEN}✓ Addon installed${NC}"
echo

# --- Environment Variables ---

cd "$PROJECT_ROOT"

ENV_FILE="$HOME/.config/environment.d/fcitx5.conf"
echo -e "${BLUE}Setting up environment variables...${NC}"

mkdir -p "$HOME/.config/environment.d"

write_env_file() {
    cat > "$ENV_FILE" << 'EOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
GLFW_IM_MODULE=ibus
EOF
    echo -e "${GREEN}✓ Environment variables configured${NC}"
}

if [ -f "$ENV_FILE" ]; then
    echo -e "${YELLOW}Environment file already exists: $ENV_FILE${NC}"
    echo "Contents:"
    cat "$ENV_FILE"
    echo
    read -p "Overwrite with fcitx5 settings? [Y/n] " -r
    echo
    if [[ $REPLY =~ ^[Nn]$ ]]; then
        echo -e "${YELLOW}Skipping environment setup. Make sure GTK_IM_MODULE, QT_IM_MODULE, and XMODIFIERS are set to fcitx.${NC}"
    else
        write_env_file
    fi
else
    write_env_file
fi
echo

# --- Debian: im-config ---

if [ "$DISTRO" = "debian" ]; then
    echo -e "${BLUE}Configuring input method framework...${NC}"
    if ! command -v im-config >/dev/null 2>&1; then
        echo -e "${YELLOW}Installing im-config...${NC}"
        sudo apt install -y im-config
    fi
    im-config -n fcitx5 2>/dev/null || true
    echo -e "${GREEN}✓ Fcitx5 set as default input method${NC}"
    echo
fi

# --- Config Status ---

echo -e "${BLUE}Checking configuration...${NC}"
CONFIG_FILE="$HOME/.config/fcitx5/conf/schnelle-umlaute.conf"
if [ -f "$CONFIG_FILE" ] && grep -q -E "^\[Mapping1\]|^DelayLowercase=|^LeaderSpace=|^Mapping1Input=" "$CONFIG_FILE"; then
    echo -e "${YELLOW}Old v0.x config found: $CONFIG_FILE${NC}"
    echo -e "${YELLOW}This config is not used by v1.0+ and can be safely deleted.${NC}"
elif [ -f "$CONFIG_FILE" ]; then
    echo -e "${GREEN}✓ Configuration found${NC}"
else
    echo -e "${GREEN}✓ No existing config (defaults will be used on first start)${NC}"
fi
echo

# --- Fix Shift+L Conflict ---

echo -e "${BLUE}Checking Fcitx5 hotkey configuration...${NC}"
CONFIG_DIR="$HOME/.config/fcitx5"
FCITX_CONFIG="$CONFIG_DIR/config"

if [ -f "$FCITX_CONFIG" ] && sed -n '/\[Hotkey\/TriggerKeys\]/,/^\[/p' "$FCITX_CONFIG" | grep -qE "^[0-9]+=.*Shift"; then
    echo -e "${YELLOW}Shift is configured as an input method trigger key.${NC}"
    echo -e "${YELLOW}This conflicts with Schnelle Umlaute, which uses Shift for uppercase${NC}"
    echo -e "${YELLOW}mappings (e.g. Shift+A → Ä). With Shift as trigger, fcitx5 will${NC}"
    echo -e "${YELLOW}switch input methods instead.${NC}"
    echo
    read -p "Replace trigger key with Ctrl+Space? [Y/n] " -r
    echo
    if [[ ! $REPLY =~ ^[Nn]$ ]]; then
        if sed '/\[Hotkey\/TriggerKeys\]/,/^\[/{/^[0-9]\+=/d}' "$FCITX_CONFIG" > "$FCITX_CONFIG.tmp" \
           && sed -i '/\[Hotkey\/TriggerKeys\]/a 0=Control+space' "$FCITX_CONFIG.tmp" \
           && mv "$FCITX_CONFIG.tmp" "$FCITX_CONFIG"; then
            echo -e "${GREEN}✓ Trigger key replaced with Ctrl+Space${NC}"
        else
            echo -e "${RED}✗ Could not update trigger key config${NC}"
        fi
    else
        echo -e "${YELLOW}Keeping current trigger key. Shift+key mappings may not work.${NC}"
    fi
else
    echo -e "${GREEN}✓ No Shift conflict detected${NC}"
fi
echo

# --- KDE Wayland: Disable Duplicate Autostart ---

if [ "$XDG_SESSION_TYPE" = "wayland" ] && [ "$XDG_CURRENT_DESKTOP" = "KDE" ]; then
    AUTOSTART_FILE="$HOME/.config/autostart/org.fcitx.Fcitx5.desktop"
    if [ ! -f "$AUTOSTART_FILE" ] || ! grep -q "Hidden=true" "$AUTOSTART_FILE"; then
        echo -e "${BLUE}Disabling redundant fcitx5 autostart (KWin handles this)...${NC}"
        mkdir -p "$HOME/.config/autostart"
        cat > "$AUTOSTART_FILE" << 'EOF'
[Desktop Entry]
Hidden=true
EOF
        echo -e "${GREEN}✓ Duplicate autostart disabled${NC}"
    fi
fi

# --- Non-Arch (non-KDE-Wayland): Setup Autostart ---

if [ "$DISTRO" != "arch" ]; then
    if ! ([ "$XDG_SESSION_TYPE" = "wayland" ] && [ "$XDG_CURRENT_DESKTOP" = "KDE" ]); then
        echo -e "${BLUE}Setting up autostart...${NC}"
        AUTOSTART_DIR="$HOME/.config/autostart"
        mkdir -p "$AUTOSTART_DIR"

        if [ -f /usr/share/applications/org.fcitx.Fcitx5.desktop ]; then
            cp /usr/share/applications/org.fcitx.Fcitx5.desktop "$AUTOSTART_DIR/"
            echo -e "${GREEN}✓ Fcitx5 will autostart on login${NC}"
        elif [ -f /usr/share/applications/fcitx5.desktop ]; then
            cp /usr/share/applications/fcitx5.desktop "$AUTOSTART_DIR/"
            echo -e "${GREEN}✓ Fcitx5 will autostart on login${NC}"
        else
            echo -e "${YELLOW}Could not find Fcitx5 desktop file for autostart${NC}"
            echo "  You may need to manually add Fcitx5 to startup applications"
        fi
    fi
fi
echo

# --- Restart Fcitx5 ---

echo -e "${BLUE}Checking Fcitx5 status...${NC}"
if pgrep -x fcitx5 > /dev/null; then
    if [ "$XDG_SESSION_TYPE" = "wayland" ] && [ "$XDG_CURRENT_DESKTOP" = "KDE" ]; then
        fcitx5-remote -r 2>/dev/null && \
            echo -e "${GREEN}✓ Fcitx5 config reloaded${NC}" || \
            echo -e "${YELLOW}Could not reload fcitx5 config${NC}"
        echo -e "${YELLOW}  To fully restart fcitx5: right-click the system tray icon → Exit${NC}"
        echo -e "${YELLOW}  KWin will restart it automatically${NC}"
    else
        killall fcitx5 2>/dev/null || true
        sleep 1
        fcitx5 -d 2>/dev/null &
        sleep 2
        if pgrep -x fcitx5 > /dev/null; then
            echo -e "${GREEN}✓ Fcitx5 restarted successfully${NC}"
        else
            echo -e "${YELLOW}Fcitx5 stopped (will start on next login)${NC}"
        fi
    fi
else
    echo -e "${YELLOW}Fcitx5 not running yet${NC}"
    echo -e "${YELLOW}  It will start automatically on next login${NC}"
fi
echo

# --- Final Instructions ---

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Installation Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo
echo -e "${YELLOW}IMPORTANT: Next Steps${NC}"
echo
echo -e "1. ${RED}LOGOUT AND LOGIN${NC} for environment variables to take effect"
echo
echo "2. After login, configure Fcitx5:"
echo -e "   ${BLUE}fcitx5-config-qt${NC}"
echo
echo "3. In the configuration window:"
echo "   - Go to 'Input Method' tab"
echo "   - Click '+' to add"
echo "   - Search for 'Schnelle Umlaute'"
echo "   - Add it to your input methods"
echo
echo "4. Switch to 'Schnelle Umlaute' using your configured trigger key (default: Ctrl+Space)"
echo
echo "5. Test it:"
echo "   - Hold 'a' and press Space → ä"
echo "   - Hold 'o' and press Space → ö"
echo "   - Hold 'u' and press Space → ü"
echo "   - Hold 's' and press Space → ß"
echo
echo -e "${YELLOW}Troubleshooting:${NC}"
echo "  - Run 'fcitx5-diagnose' to check setup"
if [ "$DISTRO" != "arch" ]; then
    echo "  - Make sure IBus is not running: pkill ibus-daemon"
    echo "  - Check env vars after login: echo \$GTK_IM_MODULE"
fi
echo "  - See README.md for more help"
echo
if [ "$XDG_SESSION_TYPE" = "wayland" ] && [ "$XDG_CURRENT_DESKTOP" = "KDE" ]; then
    echo -e "${BLUE}For KDE Wayland users:${NC}"
    echo "  Set 'System Settings → Virtual Keyboard' to 'Fcitx 5'"
    echo "  (This eliminates KWin warnings)"
    echo
fi
if [ "$XDG_CURRENT_DESKTOP" = "GNOME" ]; then
    echo -e "${BLUE}GNOME Users:${NC}"
    echo "  If Fcitx5 doesn't work in GNOME apps, try:"
    echo "  gsettings set org.gnome.settings-daemon.plugins.xsettings overrides \"{'Gtk/IMModule':<'fcitx'>}\""
    echo
fi
