#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Schnelle Umlaute - Ubuntu Uninstall${NC}"
echo -e "${BLUE}========================================${NC}"
echo

# Check possible library paths (Ubuntu uses multiarch, also check /usr/local
# in case a previous install used the default CMake prefix)
LIB_PATHS=(
    "/usr/lib/fcitx5"
    "/usr/lib/x86_64-linux-gnu/fcitx5"
    "/usr/lib/aarch64-linux-gnu/fcitx5"
    "/usr/local/lib/fcitx5"
    "/usr/local/lib/x86_64-linux-gnu/fcitx5"
    "/usr/local/lib/aarch64-linux-gnu/fcitx5"
)

# Find addon files
FOUND_FILES=()

# Check library in all possible paths
for lib_path in "${LIB_PATHS[@]}"; do
    if [ -f "$lib_path/schnelle-umlaute.so" ]; then
        FOUND_FILES+=("$lib_path/schnelle-umlaute.so")
    fi
done

# Check data files (both /usr and /usr/local prefixes)
DATA_FILES=(
    "/usr/share/fcitx5/addon/schnelle-umlaute.conf"
    "/usr/share/fcitx5/addon/schnelle-umlaute.conf.in"
    "/usr/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml"
    "/usr/share/fcitx5/inputmethod/schnelle-umlaute.conf"
    "/usr/local/share/fcitx5/addon/schnelle-umlaute.conf"
    "/usr/local/share/fcitx5/addon/schnelle-umlaute.conf.in"
    "/usr/local/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml"
    "/usr/local/share/fcitx5/inputmethod/schnelle-umlaute.conf"
)

for file in "${DATA_FILES[@]}"; do
    if [ -f "$file" ]; then
        FOUND_FILES+=("$file")
    fi
done

if [ ${#FOUND_FILES[@]} -eq 0 ]; then
    echo -e "${YELLOW}No installation found. Nothing to uninstall.${NC}"
    exit 0
fi

echo -e "${YELLOW}Found installed files:${NC}"
for file in "${FOUND_FILES[@]}"; do
    echo "  - $file"
done
echo

read -p "Remove these files? [y/N] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}Uninstallation cancelled.${NC}"
    exit 0
fi

# Remove files
echo -e "${BLUE}Removing files (requires sudo)...${NC}"
sudo rm -f "${FOUND_FILES[@]}"
if [ $? -eq 0 ]; then
    for file in "${FOUND_FILES[@]}"; do
        echo -e "  ${GREEN}✓${NC} Removed: $file"
    done
else
    echo -e "${RED}Error removing files${NC}"
    exit 1
fi
echo

# Ask about user configuration
USER_CONFIG="$HOME/.config/fcitx5/conf/schnelle-umlaute.conf"
if [ -f "$USER_CONFIG" ]; then
    echo -e "${YELLOW}User configuration found: $USER_CONFIG${NC}"
    read -p "Remove user configuration? [y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -f "$USER_CONFIG"
        echo -e "${GREEN}✓ User configuration removed${NC}"
    else
        echo -e "${YELLOW}Keeping user configuration${NC}"
    fi
    echo
fi

# Ask about environment variables
ENV_FILE="$HOME/.config/environment.d/fcitx5.conf"
if [ -f "$ENV_FILE" ]; then
    echo -e "${YELLOW}Environment configuration found: $ENV_FILE${NC}"
    read -p "Remove environment configuration? [y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -f "$ENV_FILE"
        echo -e "${GREEN}✓ Environment configuration removed${NC}"
    else
        echo -e "${YELLOW}Keeping environment configuration${NC}"
    fi
    echo
fi

# Ask about autostart
AUTOSTART_FILES=(
    "$HOME/.config/autostart/org.fcitx.Fcitx5.desktop"
    "$HOME/.config/autostart/fcitx5.desktop"
)

for autostart in "${AUTOSTART_FILES[@]}"; do
    if [ -f "$autostart" ]; then
        echo -e "${YELLOW}Autostart configuration found: $autostart${NC}"
        read -p "Remove autostart configuration? [y/N] " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            rm -f "$autostart"
            echo -e "${GREEN}✓ Autostart configuration removed${NC}"
        else
            echo -e "${YELLOW}Keeping autostart configuration${NC}"
        fi
        echo
        break
    fi
done

# Restart Fcitx5
echo -e "${BLUE}Restarting Fcitx5...${NC}"
if pgrep -x fcitx5 > /dev/null; then
    fcitx5 -r 2>/dev/null &
    sleep 2
    if pgrep -x fcitx5 > /dev/null; then
        echo -e "${GREEN}✓ Fcitx5 restarted successfully${NC}"
    else
        echo -e "${YELLOW}⚠ Fcitx5 stopped${NC}"
    fi
else
    echo -e "${YELLOW}⚠ Fcitx5 not running${NC}"
fi
echo

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Uninstallation Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo
echo -e "${YELLOW}Notes:${NC}"
echo "  - Fcitx5 packages are still installed (remove with: sudo apt remove fcitx5)"
echo "  - Logout/login to fully apply changes"
echo
