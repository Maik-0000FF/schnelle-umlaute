#!/bin/bash
set -e

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

if [ -f /etc/os-release ]; then
    . /etc/os-release
    DISTRO_NAME="${PRETTY_NAME:-$ID}"
else
    DISTRO_NAME="Unknown"
fi

echo -e "${BLUE}Distribution:${NC} $DISTRO_NAME"
echo

# Check if running with sudo (should NOT be)
if [ "$EUID" -eq 0 ]; then
    echo -e "${RED}Error: Do not run this script with sudo!${NC}"
    echo "Run as regular user. Sudo will be requested when needed."
    exit 1
fi

# --- Find Installed Files ---

# Check ALL possible library paths regardless of detected distro.
# Searching paths that don't exist is harmless; missing paths is not.
LIB_PATHS=(
    /usr/lib/fcitx5                              # Arch, Manjaro, EndeavourOS
    /usr/lib64/fcitx5                            # Fedora, openSUSE, RHEL
    /usr/lib/x86_64-linux-gnu/fcitx5             # Debian, Ubuntu (multiarch x86_64)
    /usr/lib/aarch64-linux-gnu/fcitx5            # Debian, Ubuntu (multiarch arm64)
    /usr/local/lib/fcitx5                        # Source build
    /usr/local/lib64/fcitx5                      # Source build (lib64)
    /usr/local/lib/x86_64-linux-gnu/fcitx5       # Source build (multiarch x86_64)
    /usr/local/lib/aarch64-linux-gnu/fcitx5      # Source build (multiarch arm64)
)

FOUND_FILES=()

for lib_path in "${LIB_PATHS[@]}"; do
    [ -f "$lib_path/schnelle-umlaute.so" ] && \
        FOUND_FILES+=("$lib_path/schnelle-umlaute.so")
    [ -f "$lib_path/qt6/libschnelle-umlaute-config-editor.so" ] && \
        FOUND_FILES+=("$lib_path/qt6/libschnelle-umlaute-config-editor.so")
done

DATA_FILES=(
    /usr/share/fcitx5/addon/schnelle-umlaute.conf
    /usr/share/fcitx5/addon/schnelle-umlaute.conf.in
    /usr/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml
    /usr/share/fcitx5/inputmethod/schnelle-umlaute.conf
    /usr/local/share/fcitx5/addon/schnelle-umlaute.conf
    /usr/local/share/fcitx5/addon/schnelle-umlaute.conf.in
    /usr/local/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml
    /usr/local/share/fcitx5/inputmethod/schnelle-umlaute.conf
)

for file in "${DATA_FILES[@]}"; do
    [ -f "$file" ] && FOUND_FILES+=("$file")
done

# Fallback: catch anything we missed via wildcard search.
# Ensures any legacy file under fcitx5 directories gets caught.
SEARCH_ROOTS=(
    /usr/lib /usr/lib64 /usr/local/lib /usr/local/lib64
    /usr/share/fcitx5 /usr/local/share/fcitx5
)
for root in "${SEARCH_ROOTS[@]}"; do
    [ -d "$root" ] || continue
    while IFS= read -r found; do
        skip=0
        for existing in "${FOUND_FILES[@]}"; do
            [ "$existing" = "$found" ] && skip=1 && break
        done
        [ $skip -eq 0 ] && FOUND_FILES+=("$found")
    done < <(find "$root" \( -iname "*schnelle*umlaute*" -o -iname "*SchnelleUmlaute*" \) -type f 2>/dev/null)
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

read -p "Remove these files? [y/N] " -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}Uninstallation cancelled.${NC}"
    exit 0
fi

# --- Remove Files ---

echo -e "${BLUE}Removing files (requires sudo)...${NC}"
sudo rm -f "${FOUND_FILES[@]}"
for file in "${FOUND_FILES[@]}"; do
    echo -e "  ${GREEN}✓${NC} Removed: $file"
done
echo

# --- User Configuration ---

USER_CONFIG="$HOME/.config/fcitx5/conf/schnelle-umlaute.conf"
MAPPINGS_DIR="$HOME/.config/fcitx5/schnelle-umlaute"

if [ -f "$USER_CONFIG" ] || [ -d "$MAPPINGS_DIR" ]; then
    echo -e "${YELLOW}User configuration found:${NC}"
    [ -f "$USER_CONFIG" ] && echo "  - $USER_CONFIG (settings)"
    [ -d "$MAPPINGS_DIR" ] && echo "  - $MAPPINGS_DIR/ (mappings)"
    read -p "Remove user configuration? [y/N] " -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        [ -f "$USER_CONFIG" ] && rm -f "$USER_CONFIG"
        [ -d "$MAPPINGS_DIR" ] && rm -rf "$MAPPINGS_DIR"
        echo -e "${GREEN}✓ User configuration removed${NC}"
    else
        echo -e "${YELLOW}Keeping user configuration${NC}"
    fi
    echo
fi

# --- Environment Variables ---

ENV_FILE="$HOME/.config/environment.d/fcitx5.conf"
if [ -f "$ENV_FILE" ]; then
    echo -e "${YELLOW}Environment configuration found: $ENV_FILE${NC}"
    read -p "Remove environment configuration? [y/N] " -r
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

# --- Autostart (Debian) ---

AUTOSTART_FILES=(
    "$HOME/.config/autostart/org.fcitx.Fcitx5.desktop"
    "$HOME/.config/autostart/fcitx5.desktop"
)

for autostart in "${AUTOSTART_FILES[@]}"; do
    if [ -f "$autostart" ]; then
        echo -e "${YELLOW}Autostart configuration found: $autostart${NC}"
        read -p "Remove autostart configuration? [y/N] " -r
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

# --- Restart Fcitx5 ---

echo -e "${BLUE}Restarting Fcitx5...${NC}"
if pgrep -x fcitx5 > /dev/null; then
    if [ "$XDG_SESSION_TYPE" = "wayland" ] && [ "$XDG_CURRENT_DESKTOP" = "KDE" ]; then
        fcitx5-remote -r 2>/dev/null && \
            echo -e "${GREEN}✓ Fcitx5 config reloaded${NC}" || \
            echo -e "${YELLOW}Could not reload fcitx5 config${NC}"
        echo -e "${YELLOW}  Logout/login to fully apply changes${NC}"
    else
        killall fcitx5 2>/dev/null || pkill fcitx5 2>/dev/null || true
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
    echo -e "${YELLOW}Fcitx5 not running${NC}"
fi
echo

# --- Done ---

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Uninstallation Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo
echo -e "${YELLOW}Notes:${NC}"
case "$DISTRO" in
    arch)   echo "  - Fcitx5 packages are still installed (remove with: sudo pacman -R fcitx5)" ;;
    debian) echo "  - Fcitx5 packages are still installed (remove with: sudo apt remove fcitx5)" ;;
    fedora) echo "  - Fcitx5 packages are still installed (remove with: sudo dnf remove fcitx5)" ;;
    suse)   echo "  - Fcitx5 packages are still installed (remove with: sudo zypper remove fcitx5)" ;;
esac
echo "  - Logout/login to fully apply changes"
if [ -f "$ENV_FILE" ]; then
    echo "  - Environment config kept at: $ENV_FILE"
fi
echo
