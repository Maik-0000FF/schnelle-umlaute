#!/bin/bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Schnelle Umlaute - Ubuntu Installation${NC}"
echo -e "${YELLOW}           (Experimental)${NC}"
echo -e "${BLUE}========================================${NC}"
echo

# Check if running on Debian-based distro
if ! command -v apt >/dev/null 2>&1; then
    echo -e "${RED}Error: This installer is designed for Ubuntu/Debian.${NC}"
    echo "For Arch Linux, use: ./install.sh"
    exit 1
fi

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

# Function to check if package is installed
is_installed() {
    dpkg -l "$1" 2>/dev/null | grep -q "^ii"
}

# Dependencies for Ubuntu/Debian
DEPS=(
    fcitx5
    fcitx5-config-qt
    fcitx5-frontend-gtk3
    fcitx5-frontend-gtk4
    fcitx5-frontend-qt5
    libfcitx5core-dev
    cmake
    extra-cmake-modules
    g++
    gettext
)

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

# Install missing dependencies
if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo -e "${YELLOW}Missing dependencies: ${MISSING_DEPS[*]}${NC}"
    read -p "Install missing dependencies? [Y/n] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Nn]$ ]]; then
        echo -e "${BLUE}Updating package list...${NC}"
        sudo apt update
        echo -e "${BLUE}Installing dependencies...${NC}"
        sudo apt install -y "${MISSING_DEPS[@]}"
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

# Build the addon
echo -e "${BLUE}Building addon...${NC}"
cd addon

# Create build directory
echo "Creating build directory..."
rm -rf build
mkdir -p build
cd build

# Configure with CMake (Ubuntu uses /usr prefix)
echo "Configuring with CMake..."
cmake -DCMAKE_INSTALL_PREFIX=/usr ..

# Build
echo "Building..."
make -j$(nproc)

echo -e "${GREEN}✓ Build successful!${NC}"
echo

# Install
echo -e "${BLUE}Installing addon...${NC}"
sudo make install
echo -e "${GREEN}✓ Addon installed${NC}"
echo

# Go back to project root
cd ../..

# Setup environment variables
ENV_FILE="$HOME/.config/environment.d/fcitx5.conf"
echo -e "${BLUE}Setting up environment variables...${NC}"

mkdir -p "$HOME/.config/environment.d"

if [ -f "$ENV_FILE" ]; then
    echo -e "${YELLOW}Environment file already exists: $ENV_FILE${NC}"
    echo "Contents:"
    cat "$ENV_FILE"
    echo
    read -p "Overwrite with fcitx5 settings? [Y/n] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Nn]$ ]]; then
        echo -e "${YELLOW}Skipping environment setup.${NC}"
    else
        cat > "$ENV_FILE" << 'EOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
EOF
        echo -e "${GREEN}✓ Environment variables configured${NC}"
    fi
else
    cat > "$ENV_FILE" << 'EOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
EOF
    echo -e "${GREEN}✓ Environment variables configured${NC}"
fi
echo

# GNOME-specific: Set Fcitx5 as input method
echo -e "${BLUE}Configuring GNOME to use Fcitx5...${NC}"

# Install im-config if not present
if ! command -v im-config >/dev/null 2>&1; then
    echo -e "${YELLOW}Installing im-config...${NC}"
    sudo apt install -y im-config
fi

# Set fcitx5 as default input method
im-config -n fcitx5 2>/dev/null || true
echo -e "${GREEN}✓ Fcitx5 set as default input method${NC}"
echo

# Setup Fcitx5 config
echo -e "${BLUE}Configuring Fcitx5 hotkeys...${NC}"
CONFIG_DIR="$HOME/.config/fcitx5"
CONFIG_FILE="$CONFIG_DIR/config"

mkdir -p "$CONFIG_DIR"

if [ ! -f "$CONFIG_FILE" ]; then
    cat > "$CONFIG_FILE" << 'EOF'
[Hotkey]
TriggerKeys=Control+space

[Behavior]
ShareInputState=No
EOF
    echo -e "${GREEN}✓ Fcitx5 configured (Ctrl+Space to switch)${NC}"
else
    echo -e "${GREEN}✓ Fcitx5 config already exists${NC}"
fi
echo

# Check for old configuration format
USER_CONFIG="$HOME/.config/fcitx5/conf/schnelle-umlaute.conf"
if [ -f "$USER_CONFIG" ] && grep -q "^\[Mapping1\]" "$USER_CONFIG"; then
    echo -e "${YELLOW}Detected old configuration format${NC}"
    read -p "Remove old config and use defaults? [Y/n] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Nn]$ ]]; then
        mv "$USER_CONFIG" "$USER_CONFIG.backup"
        echo -e "${GREEN}✓ Old config backed up${NC}"
    fi
fi
echo

# Restart Fcitx5
echo -e "${BLUE}Starting Fcitx5...${NC}"
pkill fcitx5 2>/dev/null || true
sleep 1
fcitx5 -d 2>/dev/null &
sleep 2

if pgrep -x fcitx5 > /dev/null; then
    echo -e "${GREEN}✓ Fcitx5 started successfully${NC}"
else
    echo -e "${YELLOW}⚠ Fcitx5 will start after login${NC}"
fi
echo

# Setup autostart
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
    echo -e "${YELLOW}⚠ Could not find Fcitx5 desktop file for autostart${NC}"
    echo "  You may need to manually add Fcitx5 to startup applications"
fi
echo

# Final instructions
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Installation Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo
echo -e "${YELLOW}IMPORTANT: Next Steps${NC}"
echo
echo -e "1. ${RED}LOGOUT AND LOGIN${NC} (or reboot) for changes to take effect"
echo
echo "2. After login, configure Fcitx5:"
echo "   ${BLUE}fcitx5-config-qt${NC}"
echo
echo "3. In the configuration window:"
echo "   - Go to 'Input Method' tab"
echo "   - Click '+' to add"
echo "   - Uncheck 'Only Show Current Language'"
echo "   - Search for 'Schnelle Umlaute'"
echo "   - Add it to your input methods"
echo
echo "4. Switch to 'Schnelle Umlaute' using ${BLUE}Ctrl+Space${NC}"
echo
echo "5. Test it:"
echo "   - Hold 'a' and press Space → ä"
echo "   - Hold 'o' and press Space → ö"
echo "   - Hold 'u' and press Space → ü"
echo "   - Hold 's' and press Space → ß"
echo
echo -e "${YELLOW}Troubleshooting:${NC}"
echo "  - Run 'fcitx5-diagnose' to check setup"
echo "  - Make sure IBus is not running: pkill ibus-daemon"
echo "  - Check env vars after login: echo \$GTK_IM_MODULE"
echo
echo -e "${BLUE}GNOME Users:${NC}"
echo "  If Fcitx5 doesn't work in GNOME apps, try:"
echo "  gsettings set org.gnome.settings-daemon.plugins.xsettings overrides \"{'Gtk/IMModule':<'fcitx'>}\""
echo
