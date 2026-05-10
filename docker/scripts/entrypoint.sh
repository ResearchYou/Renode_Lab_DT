#!/bin/bash
set -e

echo "============================================"
echo "  STM32F4 Mock HID Security Token Challenge"
echo "  Renode-based microcontroller emulation"
echo "============================================"
echo ""

/workspace/scripts/build_firmware.sh

echo ""

/workspace/scripts/run_test.sh
