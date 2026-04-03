#!/bin/bash
set -e

echo "===  Schnelle Umlaute Fcitx5 Addon - Build Script ==="
echo

# Check basic build tools
echo "Checking build tools..."
command -v cmake >/dev/null 2>&1 || { echo "Error: cmake not found."; exit 1; }
command -v g++ >/dev/null 2>&1 || command -v gcc >/dev/null 2>&1 || { echo "Error: g++/gcc not found."; exit 1; }
echo "All build tools found"
echo

# Create build directory
echo "Creating build directory..."
rm -rf build
mkdir -p build
cd build

# Configure with CMake (cmake will check for fcitx5, Qt6, ECM etc.)
echo "Configuring with CMake..."
cmake ..

# Build
echo "Building..."
make -j"$(nproc)"

echo
echo "Build successful!"
echo
echo "To install, run:"
echo "  cd build && sudo cmake --install ."
