#!/bin/bash
set -e

FIRMWARE_DIR=/workspace/firmware
BUILD_DIR=/workspace/build

build_target() {
    local name="$1"
    local src="$FIRMWARE_DIR/$name"
    local out="$BUILD_DIR/$name"

    echo "=== Building $name firmware ==="
    mkdir -p "$out"
    cd "$out"
    cmake "$src" \
        -DCMAKE_BUILD_TYPE=Release \
        -DPICO_BOARD=pico
    make -j$(nproc) "$name"
    echo "=== $name build complete ==="
    ls -la "$out/$name.elf" "$out/$name.uf2" 2>/dev/null || true
    echo ""
}

build_target node
build_target master
