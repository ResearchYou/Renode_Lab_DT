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
TEST_RC=0
/workspace/scripts/run_test.sh || TEST_RC=$?

echo ""

# Step 3: Generate HTML report (always, even if tests fail)
echo "=== Generating report ==="
python3 /workspace/scripts/generate_report.py /workspace/output || true

# Propagate original test exit code
exit $TEST_RC
