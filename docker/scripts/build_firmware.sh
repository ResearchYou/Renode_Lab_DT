#!/usr/bin/env bash
set -euo pipefail

TAG_COUNT="${TAG_COUNT:-12}"
SECTOR_INDEX="${SECTOR_INDEX:-0}"
BUILD_DIR="${BUILD_DIR:-/workspace/build}"
OUTPUT_DIR="${OUTPUT_DIR:-/workspace/output}"
FIRMWARE_DIR="${FIRMWARE_DIR:-/workspace/firmware}"
SEED_DIR="/workspace/firmware_seed"
GATEWAY_DIR="/workspace/support/gateway"
TEST_FILE="$FIRMWARE_DIR/tests/test_protocol.c"

if ! [[ "$TAG_COUNT" =~ ^[0-9]+$ ]] || (( TAG_COUNT < 1 || TAG_COUNT > 64 )); then
    echo "[ERROR] TAG_COUNT must be an integer from 1 through 64" >&2
    exit 2
fi
if ! [[ "$SECTOR_INDEX" =~ ^[0-9]+$ ]]; then
    echo "[ERROR] SECTOR_INDEX must be a non-negative integer" >&2
    exit 2
fi
if [ ! -f "$FIRMWARE_DIR/CMakeLists.txt" ]; then
    echo "[INFO] writable firmware mount is empty; using image seed"
    FIRMWARE_DIR="$SEED_DIR"
fi
if [ ! -f "$TEST_FILE" ]; then
    TEST_FILE="$SEED_DIR/tests/test_protocol.c"
fi

export ZEPHYR_BASE="${ZEPHYR_BASE:-/opt/zephyrproject/zephyr}"
export ZEPHYR_TOOLCHAIN_VARIANT="${ZEPHYR_TOOLCHAIN_VARIANT:-zephyr}"
export ZEPHYR_SDK_INSTALL_DIR="${ZEPHYR_SDK_INSTALL_DIR:-/opt/zephyr-sdk}"
export PATH="/opt/zephyr-venv/bin:$PATH"

mkdir -p "$BUILD_DIR" "$OUTPUT_DIR"
rm -rf "$BUILD_DIR/tag" "$BUILD_DIR/gateway" "$BUILD_DIR/protocol-tests"
mkdir -p "$BUILD_DIR/protocol-tests"

echo "=== Host known-answer and tamper tests ==="
set +e
gcc -std=c17 -O2 -Wall -Wextra -Werror \
    -I "$FIRMWARE_DIR/include" \
    "$FIRMWARE_DIR/ghost_protocol.c" \
    "$TEST_FILE" \
    -o "$BUILD_DIR/protocol-tests/test_protocol" \
    >"$OUTPUT_DIR/protocol-build.log" 2>&1
HOST_BUILD_STATUS=$?
if [ "$HOST_BUILD_STATUS" -eq 0 ]; then
    "$BUILD_DIR/protocol-tests/test_protocol" \
        >"$OUTPUT_DIR/protocol-tests.log" 2>&1
    PROTOCOL_STATUS=$?
else
    cp "$OUTPUT_DIR/protocol-build.log" "$OUTPUT_DIR/protocol-tests.log"
    PROTOCOL_STATUS=$HOST_BUILD_STATUS
fi
set -e
printf '%s\n' "$PROTOCOL_STATUS" >"$OUTPUT_DIR/protocol-tests.status"
cat "$OUTPUT_DIR/protocol-tests.log"

echo "=== Building participant nRF52840 tag firmware (Zephyr) ==="
west build -p always -b nrf52840dk/nrf52840 \
    -d "$BUILD_DIR/tag" "$FIRMWARE_DIR"

TAG_ID_BASE=$((SECTOR_INDEX * TAG_COUNT))
echo "=== Building trusted observer gateway firmware ==="
west build -p always -b nrf52840dk/nrf52840 \
    -d "$BUILD_DIR/gateway" "$GATEWAY_DIR" -- \
    -DGHOST_TAG_COUNT="$TAG_COUNT" \
    -DGHOST_TAG_ID_BASE="$TAG_ID_BASE" \
    -DGHOST_SECTOR="$SECTOR_INDEX"

test -s "$BUILD_DIR/tag/zephyr/zephyr.elf"
test -s "$BUILD_DIR/gateway/zephyr/zephyr.elf"
echo "=== Firmware builds complete ==="
ls -lh "$BUILD_DIR/tag/zephyr/zephyr.elf" \
       "$BUILD_DIR/gateway/zephyr/zephyr.elf"
