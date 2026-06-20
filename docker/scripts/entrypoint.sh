#!/bin/bash
set -e

echo "============================================"
echo "  STM32F4 HMAC-SHA1 Functional Validation"
echo "  Renode-based microcontroller emulation"
echo "============================================"
echo ""

/workspace/scripts/build_firmware.sh

echo ""

/workspace/scripts/run_test.sh
