#!/bin/bash
# Build script for Rigor Consensus C++ Test Harness
# =================================================
#
# This script compiles the test harness against the existing
# Rigor consensus codebase for golden vector export.
#
# Usage: ./build_harness.sh

set -e

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CORE_DIR="$PROJECT_ROOT/core"
BUILD_DIR="$SCRIPT_DIR/bin"

echo "Building Rigor Consensus C++ Test Harness"
echo "========================================="
echo "Script dir: $SCRIPT_DIR"
echo "Core dir:   $CORE_DIR"
echo "Build dir:  $BUILD_DIR"
echo ""

# Create build directory
mkdir -p "$BUILD_DIR"

# Compiler flags
CXXFLAGS="-std=c++17 -Wall -Wextra -O2"

# Build the test harness
echo "Compiling standalone_harness.cpp..."
g++ $CXXFLAGS "$SCRIPT_DIR/standalone_harness.cpp" -o "$BUILD_DIR/test_harness" -lcrypto

echo ""
echo "Build successful!"
echo "Executable: $BUILD_DIR/test_harness"
echo ""
echo "To run the test harness:"
echo "  $BUILD_DIR/test_harness"
echo ""
echo "To run cross-validation:"
echo "  cd ../tests"
echo "  python3 cross_validate_cpp.py"
