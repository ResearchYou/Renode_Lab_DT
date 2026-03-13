#!/bin/bash
set -e

OUTPUT_DIR=/workspace/output
UART_FILE="$OUTPUT_DIR/uart_output.txt"
RESC_FILE=/workspace/renode/run_test.resc

mkdir -p "$OUTPUT_DIR"
rm -f "$UART_FILE"

echo "=== Running RP2040 firmware in Renode ==="

renode --disable-xwt --console "$RESC_FILE" || true

echo ""
echo "=== UART Output ==="
if [ ! -f "$UART_FILE" ]; then
    echo "[ERROR] No UART output file found"
    exit 1
fi

cat "$UART_FILE"
echo ""

# Basic validation (sets exit code)
PASS=true

grep -q "BOOT: RP2040 Digital Twin POC"  "$UART_FILE" && echo "[PASS] Boot message"        || { echo "[FAIL] Boot message missing";        PASS=false; }
grep -q "UART initialized successfully"  "$UART_FILE" && echo "[PASS] UART init"            || { echo "[FAIL] UART init missing";            PASS=false; }
grep -q "LED ON  - cycle 0"              "$UART_FILE" && echo "[PASS] LED cycle output"      || { echo "[FAIL] LED cycle output missing";      PASS=false; }
grep -q "TEST COMPLETE"                  "$UART_FILE" && echo "[PASS] Test complete"         || { echo "[FAIL] Test complete missing";         PASS=false; }

echo ""

# Generate waveform PNG + HTML report
if command -v python3 &>/dev/null; then
    echo "=== Generating report ==="
    python3 /workspace/scripts/generate_report.py "$OUTPUT_DIR" && \
        echo "    → output/report.html" || \
        echo "[WARN] Report generation failed (non-fatal)"
fi

echo ""
if [ "$PASS" = true ]; then
    echo ">>> ALL CHECKS PASSED - Digital twin behaves as expected <<<"
    exit 0
else
    echo ">>> SOME CHECKS FAILED <<<"
    exit 1
fi
