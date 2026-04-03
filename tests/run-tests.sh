#!/bin/bash
# Run unit tests for schnelle-umlaute.
# Usage: ./tests/run-tests.sh

set -e

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ADDON_DIR="$REPO_DIR/addon"

cd "$ADDON_DIR"
cmake -B build -DBUILD_TESTING=ON 2>&1 | tail -1
cmake --build build 2>&1

cd build
ctest --output-on-failure

echo ""
echo "Tests done."
