#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Schnelle Umlaute - Uninstallation${NC}"
echo -e "${BLUE}========================================${NC}"
echo

# Check if addon is installed (also check /usr/local in case a previous
# install used the default CMake prefix)
FILES=(
    "/usr/lib/fcitx5/schnelle-umlaute.so"
    "/usr/share/fcitx5/addon/schnelle-umlaute.conf"
    "/usr/share/fcitx5/addon/schnelle-umlaute.conf.in"
    "/usr/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml"
    "/usr/share/fcitx5/inputmethod/schnelle-umlaute.conf"
    "/usr/local/lib/fcitx5/schnelle-umlaute.so"
    "/usr/local/share/fcitx5/addon/schnelle-umlaute.conf"
    "/usr/local/share/fcitx5/addon/schnelle-umlaute.conf.in"
    "/usr/local/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml"
    "/usr/local/share/fcitx5/inputmethod/schnelle-umlaute.conf"
)

FOUND_FILES=()
for file in "${FILES[@]}"; do
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
if [ ${#FOUND_FILES[@]} -gt 0 ]; then
    sudo rm -f "${FOUND_FILES[@]}"
    if [ $? -eq 0 ]; then
        for file in "${FOUND_FILES[@]}"; do
            echo -e "  ${GREEN}✓${NC} Removed: $file"
        done
    else
        echo -e "${RED}Error removing files${NC}"
        exit 1
    fi
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
        echo -e "${YELLOW}Note: Logout/login to apply changes${NC}"
    else
        echo -e "${YELLOW}Keeping environment configuration${NC}"
    fi
    echo
fi

# Restart Fcitx5
echo -e "${BLUE}Restarting Fcitx5...${NC}"
if pgrep -x fcitx5 > /dev/null; then
    killall fcitx5 2>/dev/null || true
    sleep 1
    fcitx5 -d 2>/dev/null
    sleep 2
    if pgrep -x fcitx5 > /dev/null; then
        echo -e "${GREEN}✓ Fcitx5 restarted successfully${NC}"
    else
        echo -e "${YELLOW}⚠ Fcitx5 stopped (will start on next login)${NC}"
    fi
else
    echo -e "${YELLOW}⚠ Fcitx5 not running (will start on next login)${NC}"
fi
echo

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Uninstallation Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo
echo -e "${YELLOW}Note:${NC} If you kept the environment configuration,"
echo "Fcitx5 will still be active as input method."
echo "To fully remove Fcitx5, delete $ENV_FILE"
echo "and logout/login."
echo
