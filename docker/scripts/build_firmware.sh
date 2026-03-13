#!/bin/bash
set -e

echo "=== Building RP2040 firmware ==="

BUILD_DIR=/workspace/build
FIRMWARE_DIR=/workspace/firmware

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$FIRMWARE_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPICO_BOARD=pico

make -j$(nproc) firmware

echo "=== Build complete ==="
ls -la "$BUILD_DIR/firmware.elf" "$BUILD_DIR/firmware.uf2" 2>/dev/null || true
