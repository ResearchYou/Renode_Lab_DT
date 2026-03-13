#!/bin/bash
set -e

echo "============================================"
echo "  RP2040 Digital Twin POC"
echo "  Renode-based microcontroller emulation"
echo "============================================"
echo ""

# Step 1: Build firmware
/workspace/scripts/build_firmware.sh

echo ""

# Step 2: Run in Renode and validate
/workspace/scripts/run_test.sh
