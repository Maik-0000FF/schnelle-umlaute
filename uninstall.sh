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

# Check if addon is installed
FILES=(
    "/usr/lib/fcitx5/schnelle-umlaute.so"
    "/usr/share/fcitx5/addon/schnelle-umlaute.conf"
    "/usr/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml"
    "/usr/share/fcitx5/inputmethod/schnelle-umlaute.conf"
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
fcitx5 -r 2>/dev/null || {
    echo -e "${YELLOW}Note: Fcitx5 not running${NC}"
}
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
