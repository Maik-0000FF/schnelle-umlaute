#!/bin/bash
set -e

echo "===  Schnelle Umlaute Fcitx5 Addon - Build Script ==="
echo

# Check dependencies
echo "Checking dependencies..."
command -v cmake >/dev/null 2>&1 || { echo "Error: cmake not found. Install with: sudo pacman -S cmake"; exit 1; }
pacman -Q extra-cmake-modules >/dev/null 2>&1 || { echo "Error: extra-cmake-modules not found. Install with: sudo pacman -S extra-cmake-modules"; exit 1; }
pacman -Q fcitx5 >/dev/null 2>&1 || { echo "Error: fcitx5 not found. Install with: sudo pacman -S fcitx5"; exit 1; }

echo "✓ All dependencies found"
echo

# Create build directory
echo "Creating build directory..."
rm -rf build
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake -DCMAKE_INSTALL_PREFIX=/usr ..

# Build
echo "Building..."
make -j$(nproc)

echo
echo "✓ Build successful!"
echo
echo "To install, run:"
echo "  cd build && sudo make install"
