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

# Check if running on Arch-based distro
if ! command -v pacman >/dev/null 2>&1; then
    echo -e "${RED}Error: This installer is designed for Arch Linux and derivatives.${NC}"
    echo "Please install dependencies manually and use the build.sh script in addon/"
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
    pacman -Q "$1" >/dev/null 2>&1
}

# Check dependencies
MISSING_DEPS=()
DEPS=(fcitx5 fcitx5-configtool fcitx5-qt fcitx5-gtk cmake extra-cmake-modules gcc)

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
        echo -e "${BLUE}Installing dependencies...${NC}"
        sudo pacman -S --needed "${MISSING_DEPS[@]}"
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
./build.sh

# Install
echo
echo -e "${BLUE}Installing addon...${NC}"
cd build
sudo make install
echo -e "${GREEN}✓ Addon installed${NC}"
echo

# Setup environment variables
ENV_FILE="$HOME/.config/environment.d/fcitx5.conf"
echo -e "${BLUE}Setting up environment variables...${NC}"

if [ -f "$ENV_FILE" ]; then
    echo -e "${YELLOW}Environment file already exists: $ENV_FILE${NC}"
    echo "Contents:"
    cat "$ENV_FILE"
    echo
    read -p "Overwrite existing file? [y/N] " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${YELLOW}Skipping environment setup. Make sure GTK_IM_MODULE, QT_IM_MODULE, and XMODIFIERS are set correctly.${NC}"
    else
        mkdir -p "$HOME/.config/environment.d"
        cat > "$ENV_FILE" << 'EOF'
GTK_IM_MODULE=fcitx5
QT_IM_MODULE=fcitx5
XMODIFIERS=@im=fcitx5
GLFW_IM_MODULE=ibus
EOF
        echo -e "${GREEN}✓ Environment variables configured${NC}"
    fi
else
    mkdir -p "$HOME/.config/environment.d"
    cat > "$ENV_FILE" << 'EOF'
GTK_IM_MODULE=fcitx5
QT_IM_MODULE=fcitx5
XMODIFIERS=@im=fcitx5
GLFW_IM_MODULE=ibus
EOF
    echo -e "${GREEN}✓ Environment variables configured${NC}"
fi
echo

# Restart Fcitx5
echo -e "${BLUE}Checking Fcitx5 status...${NC}"
if pgrep -x fcitx5 > /dev/null; then
    echo -e "${YELLOW}Fcitx5 is running, restarting...${NC}"
    fcitx5 -r > /dev/null 2>&1 &
    sleep 2
    echo -e "${GREEN}✓ Fcitx5 restarted${NC}"
else
    echo -e "${YELLOW}Note: Fcitx5 not running yet.${NC}"
    echo -e "${YELLOW}It will start automatically on next login.${NC}"
fi
echo

# Fix Shift+L conflict
echo -e "${BLUE}Configuring Fcitx5 to avoid Shift conflicts...${NC}"
CONFIG_DIR="$HOME/.config/fcitx5"
CONFIG_FILE="$CONFIG_DIR/config"

mkdir -p "$CONFIG_DIR"

if [ ! -f "$CONFIG_FILE" ]; then
    # Create new config with only Ctrl+Space
    cat > "$CONFIG_FILE" << 'EOF'
[Hotkey]
TriggerKeys=Control+space

[Behavior]
ShareInputState=No
EOF
    echo -e "${GREEN}✓ Fcitx5 configured (Shift_L disabled)${NC}"
else
    # Check if TriggerKeys contains Shift_L
    if grep -q "TriggerKeys.*Shift" "$CONFIG_FILE"; then
        # Remove Shift from TriggerKeys, keep only Control+space
        sed -i 's/^TriggerKeys=.*/TriggerKeys=Control+space/' "$CONFIG_FILE"
        echo -e "${GREEN}✓ Shift conflict resolved (switched to Ctrl+Space only)${NC}"
    else
        echo -e "${GREEN}✓ No Shift conflict detected${NC}"
    fi
fi
echo

# Final instructions
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Installation Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo
echo -e "${YELLOW}IMPORTANT: Next Steps${NC}"
echo
echo -e "1. ${RED}LOGOUT AND LOGIN${NC} for environment variables to take effect"
echo
echo "2. After login, configure Fcitx5:"
echo "   ${BLUE}fcitx5-config-qt${NC}"
echo
echo "3. In the configuration window:"
echo "   - Go to 'Input Method' tab"
echo "   - Click '+' to add"
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
echo "  - See README.md for more help"
echo
echo -e "${BLUE}For KDE Wayland users:${NC}"
echo "  Set 'System Settings → Virtual Keyboard' to 'Fcitx 5'"
echo "  (This eliminates KWin warnings)"
echo
