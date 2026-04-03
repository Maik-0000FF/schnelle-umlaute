#!/bin/bash
# Run unit tests from private/test/ without polluting the public repo.
# Usage: ./private/run-tests.sh

set -e

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ADDON_DIR="$REPO_DIR/addon"
PRIVATE_TEST="$REPO_DIR/private/test"

# Temporarily link test files into addon/
if [ -d "$ADDON_DIR/test" ]; then
    echo "Error: addon/test/ already exists. Remove it first."
    exit 1
fi

ln -s "$PRIVATE_TEST" "$ADDON_DIR/test"

# Add test subdirectory to CMakeLists.txt
if ! grep -q "add_subdirectory(test)" "$ADDON_DIR/CMakeLists.txt"; then
    echo -e "\nenable_testing()\nadd_subdirectory(test)" >> "$ADDON_DIR/CMakeLists.txt"
fi

# Reconfigure and build
cd "$ADDON_DIR"
cmake -B build 2>&1 | tail -1
cmake --build build 2>&1

# Run tests
cd build
./test/testschnelleumlaute 2>&1 | grep -E "=== |PASSED|FAILED"

# Cleanup: remove symlink and CMakeLists changes
rm -f "$ADDON_DIR/test"
cd "$ADDON_DIR"
sed -i '/^enable_testing()$/d' CMakeLists.txt
sed -i '/^add_subdirectory(test)$/d' CMakeLists.txt
# Remove trailing empty lines
sed -i -e :a -e '/^\n*$/{$d;N;ba' -e '}' CMakeLists.txt

echo ""
echo "Tests done. addon/ is clean."
