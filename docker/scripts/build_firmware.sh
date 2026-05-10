#!/bin/bash
set -e

echo "=== Building STM32F4 mock HID token firmware ==="

SRC_DIR=/workspace/firmware
BUILD_DIR=/workspace/build

mkdir -p "$BUILD_DIR"
make -C "$SRC_DIR" BUILDDIR="$BUILD_DIR" -j"$(nproc)"

echo "=== Build complete ==="
ls -la "$BUILD_DIR/token.elf"
