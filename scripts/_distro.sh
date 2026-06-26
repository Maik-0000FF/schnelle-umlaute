# shellcheck shell=bash
# Shared distribution detection for install.sh and uninstall.sh.
# Source from a script that has already set "set -e" if desired.
#
# After calling detect_distro_info, the caller can rely on:
#   $DISTRO         — normalized family: arch | debian | fedora | suse | unknown
#   $FAMILY_LABEL   — human-readable label for status messages
#   $INVOKING_USER  — real invoking user, robust where $USER is empty
#
# Keeping the mapping in one place prevents the install/uninstall pair
# from drifting when a new derivative needs to be supported.

# shellcheck disable=SC2034
# DISTRO, FAMILY_LABEL and INVOKING_USER are read by the sourcing script,
# not in this file.
detect_distro_info() {
    # $USER is empty in some cron/su contexts, which would make `pkill -u ""`
    # error out and silently no-op via `|| true`, leaving a stale daemon.
    INVOKING_USER="$(id -un)"
    local id=""
    local id_like=""
    if [ -f /etc/os-release ]; then
        # shellcheck source=/dev/null
        . /etc/os-release
        id="${ID:-}"
        id_like="${ID_LIKE:-}"
    fi

    case "$id" in
        arch)                 DISTRO=arch;   FAMILY_LABEL="Arch Linux" ;;
        manjaro)              DISTRO=arch;   FAMILY_LABEL="Manjaro" ;;
        endeavouros)          DISTRO=arch;   FAMILY_LABEL="EndeavourOS" ;;
        garuda)               DISTRO=arch;   FAMILY_LABEL="Garuda" ;;
        artix)                DISTRO=arch;   FAMILY_LABEL="Artix" ;;
        cachyos)              DISTRO=arch;   FAMILY_LABEL="CachyOS" ;;
        debian)               DISTRO=debian; FAMILY_LABEL="Debian" ;;
        ubuntu)               DISTRO=debian; FAMILY_LABEL="Ubuntu" ;;
        linuxmint)            DISTRO=debian; FAMILY_LABEL="Linux Mint" ;;
        pop)                  DISTRO=debian; FAMILY_LABEL="Pop!_OS" ;;
        kali)                 DISTRO=debian; FAMILY_LABEL="Kali Linux" ;;
        elementary)           DISTRO=debian; FAMILY_LABEL="elementary OS" ;;
        zorin)                DISTRO=debian; FAMILY_LABEL="Zorin OS" ;;
        mx)                   DISTRO=debian; FAMILY_LABEL="MX Linux" ;;
        neon)                 DISTRO=debian; FAMILY_LABEL="KDE neon" ;;
        fedora)               DISTRO=fedora; FAMILY_LABEL="Fedora" ;;
        nobara)               DISTRO=fedora; FAMILY_LABEL="Nobara" ;;
        opensuse-tumbleweed)  DISTRO=suse;   FAMILY_LABEL="openSUSE Tumbleweed" ;;
        opensuse-leap)        DISTRO=suse;   FAMILY_LABEL="openSUSE Leap" ;;
        opensuse*|suse)       DISTRO=suse;   FAMILY_LABEL="openSUSE" ;;
        *)
            case "$id_like" in
                *arch*)    DISTRO=arch;   FAMILY_LABEL="Arch Linux derivative" ;;
                *debian*|*ubuntu*)
                           DISTRO=debian; FAMILY_LABEL="Debian/Ubuntu derivative" ;;
                *fedora*)  DISTRO=fedora; FAMILY_LABEL="Fedora derivative" ;;
                *suse*)    DISTRO=suse;   FAMILY_LABEL="openSUSE derivative" ;;
                *)
                    # Last resort: fall back to package-manager probe so a
                    # /etc/os-release-less system still gets a working family.
                    if command -v pacman >/dev/null 2>&1; then
                        DISTRO=arch;   FAMILY_LABEL="Arch-like (pacman)"
                    elif command -v apt >/dev/null 2>&1; then
                        DISTRO=debian; FAMILY_LABEL="Debian-like (apt)"
                    elif command -v dnf >/dev/null 2>&1; then
                        DISTRO=fedora; FAMILY_LABEL="Fedora-like (dnf)"
                    elif command -v zypper >/dev/null 2>&1; then
                        DISTRO=suse;   FAMILY_LABEL="openSUSE-like (zypper)"
                    else
                        DISTRO=unknown; FAMILY_LABEL="unknown distribution"
                    fi
                    ;;
            esac
            ;;
    esac
}
