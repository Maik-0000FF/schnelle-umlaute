# Installation Guide - Schnelle Umlaute (Fcitx5)

Complete step-by-step installation guide for **Schnelle Umlaute** Fcitx5 addon.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Installation Steps](#installation-steps)
3. [Configuration](#configuration)
4. [Verification](#verification)
5. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### System Requirements

- **Linux Distribution**: Arch Linux (or compatible)
- **Display Server**: X11 or Wayland
- **Input Method Framework**: Fcitx5

### Required Packages

Before installation, ensure you have the following packages installed:

```bash
sudo pacman -S fcitx5 fcitx5-configtool fcitx5-qt fcitx5-gtk cmake extra-cmake-modules gcc
```

**Package Breakdown:**
- `fcitx5` - Input Method Framework (core)
- `fcitx5-configtool` - GUI configuration tool
- `fcitx5-qt` - Qt integration (required for Qt/KDE apps)
- `fcitx5-gtk` - GTK integration (required for Wayland and GTK apps)
- `cmake` - Build system
- `extra-cmake-modules` - KDE CMake modules (required by Fcitx5)
- `gcc` - C++ compiler with C++20 support

### Check Installation

Verify packages are installed:

```bash
pacman -Q fcitx5 fcitx5-configtool cmake extra-cmake-modules gcc
```

---

## Installation Steps

### Step 1: Clone the Repository

```bash
cd ~
git clone https://github.com/Maik-0000FF/schnelle-umlaute.git
cd schnelle-umlaute/addon
```

### Step 2: Build the Addon

The `build.sh` script automatically configures CMake with the correct install prefix (`/usr`):

```bash
./build.sh
```

**Expected output:**
```
===  Schnelle Umlaute Fcitx5 Addon - Build Script ===
Checking dependencies...
✓ All dependencies found
Creating build directory...
Configuring with CMake...
Building...
[100%] Built target schnelle-umlaute
✓ Build successful!
```

### Step 3: Install the Addon

```bash
cd build
sudo make install
```

**Expected output:**
```
-- Installing: /usr/lib/fcitx5/schnelle-umlaute.so
-- Installing: /usr/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml
-- Installing: /usr/share/fcitx5/addon/schnelle-umlaute.conf
-- Installing: /usr/share/fcitx5/inputmethod/schnelle-umlaute.conf
```

**Important:** All four files must be installed for the addon to work correctly!

### Step 4: Configure Environment Variables

For the addon to work in **all applications** (not just terminals), you must set environment variables:

```bash
mkdir -p ~/.config/environment.d
cat > ~/.config/environment.d/fcitx5.conf << 'EOF'
GTK_IM_MODULE=fcitx5
QT_IM_MODULE=fcitx5
XMODIFIERS=@im=fcitx5
GLFW_IM_MODULE=ibus
EOF
```

**What these do:**
- `GTK_IM_MODULE=fcitx5` - Enables Fcitx5 in GTK apps (Firefox, GNOME apps)
- `QT_IM_MODULE=fcitx5` - Enables Fcitx5 in Qt apps (Kate, Dolphin, KDE apps)
- `XMODIFIERS=@im=fcitx5` - Enables Fcitx5 in X11 apps (legacy support)
- `GLFW_IM_MODULE=ibus` - Enables Fcitx5 in GLFW apps (Kitty terminal, etc.)

### Step 5: Logout and Login

**CRITICAL:** Environment variables only take effect after logging out and back in!

```bash
# Save your work, then logout:
loginctl terminate-user $USER

# Or simply reboot:
sudo reboot
```

**Why?** Environment variables set in `~/.config/environment.d/` are loaded by systemd during login, not during shell sessions.

### Step 6: Restart Fcitx5 (After Login)

After logging back in:

```bash
fcitx5 -r
```

This restarts Fcitx5 and loads the new addon.

---

## Configuration

### Step 1: Open Fcitx5 Configuration

```bash
fcitx5-config-qt
```

A GUI window will open.

### Step 2: Add "Schnelle Umlaute" Input Method

1. Click on **"Input Method"** tab (or **"Eingabemethode"** in German)
2. In the "Current Input Method" section, you'll see a **"+"** button at the bottom
3. Click the **"+"** button
4. A search dialog will open
5. Type **"Schnelle"** or **"Umlaute"** in the search field
6. You should see **"Schnelle Umlaute"** with:
   - **Label:** ä
   - **Language:** de (German)
7. Select it and click **"OK"**
8. **"Schnelle Umlaute"** should now appear in your "Current Input Method" list
9. Click **"Apply"** or **"OK"** to save

### Step 3: Switch Between Input Methods

Use `Ctrl+Space` (default hotkey) to cycle between input methods:
- Keyboard (normal typing)
- Schnelle Umlaute (umlaut mode)

You can see which input method is active in the Fcitx5 system tray icon.

---

## Verification

### Test 1: Check Fcitx5 is Running

```bash
ps aux | grep fcitx5
```

Should show a running `fcitx5` process.

### Test 2: Check Addon is Loaded

```bash
fcitx5-diagnose 2>&1 | grep -i "schnelle"
```

**Expected output:**
```
Schnelle Umlaute
```

### Test 3: Check Environment Variables

```bash
echo $GTK_IM_MODULE
echo $QT_IM_MODULE
echo $XMODIFIERS
```

**Expected output:**
```
fcitx5
fcitx5
@im=fcitx5
```

If empty, you need to **logout and login again**.

### Test 4: Test Umlaut Input

1. Open any text editor (Kate, gedit, Firefox address bar, etc.)
2. Switch to "Schnelle Umlaute" input method (`Ctrl+Space`)
3. **Hold the `a` key**
4. **Press `Space`** (while still holding `a`)
5. **Expected result:** `ä` appears

Try all umlauts:

| Hold | Press Space | Result |
|------|-------------|--------|
| a    | Space       | ä      |
| o    | Space       | ö      |
| u    | Space       | ü      |
| s    | Space       | ß      |
| A (Shift+a) | Space | Ä |
| O (Shift+o) | Space | Ö |
| U (Shift+u) | Space | Ü |

---

## Troubleshooting

### Problem: Addon doesn't show in fcitx5-config-qt

**Diagnosis:**

```bash
ls -lh /usr/lib/fcitx5/schnelle-umlaute.so
ls -lh /usr/share/fcitx5/addon/schnelle-umlaute.conf
ls -lh /usr/share/fcitx5/inputmethod/schnelle-umlaute.conf
```

If any file is missing, reinstall:

```bash
cd ~/schnelle-umlaute/addon/build
sudo make install
fcitx5 -r
```

**Most common issue:** Missing `/usr/share/fcitx5/inputmethod/schnelle-umlaute.conf`

This file registers the addon as an **Input Method**. Without it, the addon won't appear in the input method list.

### Problem: Works in terminal but not in Firefox/Kate/other apps

**Diagnosis:**

```bash
echo $GTK_IM_MODULE
echo $QT_IM_MODULE
```

If both are empty, environment variables are not set.

**Solution:**

1. Re-run environment variable setup:
   ```bash
   mkdir -p ~/.config/environment.d
   cat > ~/.config/environment.d/fcitx5.conf << 'EOF'
   GTK_IM_MODULE=fcitx5
   QT_IM_MODULE=fcitx5
   XMODIFIERS=@im=fcitx5
   GLFW_IM_MODULE=ibus
   EOF
   ```

2. **Logout and login** (not just restart terminal!)

3. Verify after login:
   ```bash
   echo $GTK_IM_MODULE  # Should output: fcitx5
   ```

### Problem: Umlauts not appearing

**Check 1:** Are you on the correct input method?

```bash
# Check current IM
fcitx5-remote -n
```

Should show `schnelle-umlaute` or similar. If not, press `Ctrl+Space` to switch.

**Check 2:** Is Fcitx5 running?

```bash
ps aux | grep fcitx5
```

If not running:
```bash
fcitx5 &
```

**Check 3:** Timing issue

The Hold & Wait pattern requires pressing Space **within 400ms** of holding the key.

Try:
1. Press and hold `a`
2. **Immediately** press `Space` (don't wait!)
3. Release both keys

### Problem: Addon is visible but not activatable / Fcitx5 not responding

If you can see "Schnelle Umlaute" in fcitx5-config-qt but cannot activate it, or if Fcitx5 stopped working after a system crash:

1. **Check Fcitx5 status:**
   ```bash
   fcitx5-remote
   ```
   - Should show: `1` (inactive) or `2` (active)
   - If it shows: `0` → Fcitx5 not initialized

2. **Restart Fcitx5:**
   ```bash
   fcitx5 -r
   ```

3. **Activate addon:**
   ```bash
   fcitx5-remote -s schnelle-umlaute
   ```

4. **Verify:**
   ```bash
   fcitx5-remote -n  # Should show: schnelle-umlaute
   fcitx5-remote     # Should show: 2 (active)
   ```

This is common after system crashes or unexpected shutdowns.

### Problem: Build fails with C++ errors

**Error:**
```
error: 'span' in namespace 'std' does not name a template type
```

**Solution:** You need GCC 11+ with C++20 support:

```bash
gcc --version  # Should be 11.0 or higher
```

If older, update:
```bash
sudo pacman -Syu gcc
```

### Problem: KDE Wayland - Input method not working optimally

If you're using **KDE Plasma on Wayland**, configure virtual keyboard:

1. Open **System Settings** → **Virtual Keyboard**
2. Select **"Fcitx 5"** from the dropdown
3. Click **"Apply"**
4. Logout/login

This enables native Wayland input method protocol for better integration.

---

## Additional Configuration

### Change Default Hotkey

To change the input method switch hotkey from `Ctrl+Space`:

1. Open `fcitx5-config-qt`
2. Go to **"Global Options"** → **"Hotkey"**
3. Find **"Trigger Input Method"**
4. Click and press your desired hotkey
5. Click **"Apply"**

### Autostart Fcitx5

Fcitx5 should autostart by default. If not:

```bash
cp /usr/share/applications/org.fcitx.Fcitx5.desktop ~/.config/autostart/
```

Or use your desktop environment's autostart settings.

---

## Uninstallation

### Remove the Addon

```bash
cd ~/schnelle-umlaute/addon/build
sudo make uninstall
```

Or manually:

```bash
sudo rm /usr/lib/fcitx5/schnelle-umlaute.so
sudo rm /usr/share/fcitx5/addon/schnelle-umlaute.conf
sudo rm /usr/share/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml
sudo rm /usr/share/fcitx5/inputmethod/schnelle-umlaute.conf
```

### Remove Environment Variables (Optional)

If you want to remove Fcitx5 entirely:

```bash
rm ~/.config/environment.d/fcitx5.conf
```

Then logout/login.

### Restart Fcitx5

```bash
fcitx5 -r
```

---

## Support

For issues, please:
1. Check this troubleshooting guide
2. Run `fcitx5-diagnose` and check output
3. Open an issue at: https://github.com/Maik-0000FF/schnelle-umlaute/issues

---

**Last Updated:** 2025-10-25
**Version:** 0.1.1
