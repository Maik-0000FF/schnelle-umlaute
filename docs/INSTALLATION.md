# Installation

## Quick Install (supported distributions)

```bash
git clone https://github.com/Maik-0000FF/schnelle-umlaute.git
cd schnelle-umlaute
./install.sh
```

The installer automatically detects your distribution (Arch, Debian, Ubuntu, Fedora, openSUSE) and will:
- Check and install dependencies via the appropriate package manager
- Build and install the addon
- Configure environment variables automatically
- Detect Shift trigger key conflicts that break uppercase mappings
- Guide you through the setup

**After installation:** Logout and login, then run `fcitx5-config-qt` to add "Schnelle Umlaute" to your input methods.

**Note:** Make sure to uncheck "Only Show Current Language" when searching for the addon.

---

## Manual Installation (Arch Linux)

**1. Install Dependencies**

```bash
sudo pacman -S fcitx5 fcitx5-configtool fcitx5-qt fcitx5-gtk cmake extra-cmake-modules gcc
```

**2. Build the Addon**

```bash
cd addon
./build.sh
```

**3. Install**

```bash
cd build
sudo cmake --install .
```

**4. Configure Environment Variables**

For the addon to work in ALL applications (GTK, Qt, browsers, terminals, etc.), set up environment variables:

```bash
mkdir -p ~/.config/environment.d
cat > ~/.config/environment.d/fcitx5.conf << 'EOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
GLFW_IM_MODULE=ibus
EOF
```

**Note:** `GLFW_IM_MODULE=ibus` is required for Kitty terminal and other GLFW-based applications.

**5. Logout and Login**

**IMPORTANT:** You must logout and login again for the environment variables to take effect!

```bash
# After logout/login, verify Fcitx5 is running:
fcitx5 -r
```

---

## Manual Installation (Ubuntu / Debian)

**1. Install Dependencies**

```bash
sudo apt update
sudo apt install fcitx5 fcitx5-config-qt fcitx5-frontend-gtk3 fcitx5-frontend-gtk4 \
    fcitx5-frontend-qt5 libfcitx5core-dev fcitx5-modules-dev qt6-base-dev \
    libfcitx5-qt6-dev cmake extra-cmake-modules g++ gettext
```

**2. Build the Addon**

```bash
cd addon
./build.sh
```

**3. Install**

```bash
cd build
sudo cmake --install .
```

**4. Configure Environment Variables**

```bash
mkdir -p ~/.config/environment.d
cat > ~/.config/environment.d/fcitx5.conf << 'EOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
GLFW_IM_MODULE=ibus
EOF
```

**Note:** `GLFW_IM_MODULE=ibus` is required for Kitty terminal and other GLFW-based applications.

**5. Set Fcitx5 as Default Input Method**

```bash
im-config -n fcitx5
```

**6. Setup Autostart (GNOME only)**

> Skip this step on KDE Wayland — KWin starts Fcitx5 automatically. Adding autostart causes a dual-instance conflict.

```bash
mkdir -p ~/.config/autostart
cp /usr/share/applications/org.fcitx.Fcitx5.desktop ~/.config/autostart/
```

**7. Logout and Login**

**IMPORTANT:** You must logout and login again for the environment variables to take effect!

```bash
# After logout/login, verify Fcitx5 is running:
fcitx5 -r
```

**GNOME Users:** If Fcitx5 doesn't work in GNOME apps, run:
```bash
gsettings set org.gnome.settings-daemon.plugins.xsettings overrides "{'Gtk/IMModule':<'fcitx'>}"
```

---

## Manual Installation (Fedora)

**1. Install Dependencies**

```bash
sudo dnf install fcitx5 fcitx5-configtool fcitx5-gtk fcitx5-qt6 \
    fcitx5-devel fcitx5-qt-devel qt6-qtbase-devel \
    cmake extra-cmake-modules gcc-c++ gettext
```

**2. Build the Addon**

```bash
cd addon
./build.sh
```

**3. Install**

```bash
cd build
sudo cmake --install .
```

**4. Configure Environment Variables**

```bash
mkdir -p ~/.config/environment.d
cat > ~/.config/environment.d/fcitx5.conf << 'EOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
GLFW_IM_MODULE=ibus
EOF
```

**5. Logout and Login**

**IMPORTANT:** You must logout and login again for the environment variables to take effect!

---

## Manual Installation (openSUSE)

**1. Install Dependencies**

```bash
sudo zypper install fcitx5 fcitx5-configtool fcitx5-gtk fcitx5-qt6 \
    fcitx5-devel fcitx5-qt-devel qt6-base-devel \
    cmake extra-cmake-modules gcc-c++ gettext
```

**2. Build the Addon**

```bash
cd addon
./build.sh
```

**3. Install**

```bash
cd build
sudo cmake --install .
```

**4. Configure Environment Variables**

```bash
mkdir -p ~/.config/environment.d
cat > ~/.config/environment.d/fcitx5.conf << 'EOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
GLFW_IM_MODULE=ibus
EOF
```

**5. Logout and Login**

**IMPORTANT:** You must logout and login again for the environment variables to take effect!

---

## Uninstallation

### Quick Uninstall

```bash
./uninstall.sh
```

The uninstaller automatically detects your distribution and will:
- Remove all installed addon files
- Ask if you want to remove environment/autostart configuration
- Restart Fcitx5

### Manual Uninstallation

If the build directory still exists, use the install manifest:
```bash
cd addon/build
sudo xargs rm -f < install_manifest.txt
fcitx5 -r
```

Otherwise, remove files manually. The addon library path depends on your distribution:
```bash
# Arch Linux
sudo rm /usr/lib/fcitx5/schnelle-umlaute.so

# Ubuntu/Debian (x86_64)
sudo rm /usr/lib/x86_64-linux-gnu/fcitx5/schnelle-umlaute.so

# Fedora / openSUSE (x86_64)
sudo rm /usr/lib64/fcitx5/schnelle-umlaute.so

# Common data files (all distributions)
sudo rm /usr/share/fcitx5/addon/schnelle-umlaute.conf
sudo rm /usr/share/fcitx5/addon/schnelle-umlaute.conf.in
sudo rm /usr/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml
sudo rm /usr/share/fcitx5/inputmethod/schnelle-umlaute.conf
rm ~/.config/environment.d/fcitx5.conf  # Optional: remove environment config
fcitx5 -r
```
