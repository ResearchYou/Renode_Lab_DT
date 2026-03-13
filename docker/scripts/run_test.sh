#!/bin/bash
set -e

OUTPUT_DIR=/workspace/output
UART_FILE="$OUTPUT_DIR/uart_output.txt"
RESC_FILE=/workspace/renode/run_test.resc

mkdir -p "$OUTPUT_DIR"
rm -f "$UART_FILE"

echo "=== Running RP2040 firmware in Renode ==="

# Run Renode headless with the test script
renode --disable-xwt --console "$RESC_FILE" || true

echo ""
echo "=== UART Output ==="
if [ -f "$UART_FILE" ]; then
    cat "$UART_FILE"
    echo ""
    echo "=== Validating output ==="

    PASS=true

    if grep -q "BOOT: RP2040 Digital Twin POC" "$UART_FILE"; then
        echo "[PASS] Boot message detected"
    else
        echo "[FAIL] Boot message missing"
        PASS=false
    fi

    if grep -q "UART initialized successfully" "$UART_FILE"; then
        echo "[PASS] UART init message detected"
    else
        echo "[FAIL] UART init message missing"
        PASS=false
    fi

    if grep -q "LED ON  - cycle 0" "$UART_FILE"; then
        echo "[PASS] LED cycle output detected"
    else
        echo "[FAIL] LED cycle output missing"
        PASS=false
    fi

    if grep -q "TEST COMPLETE" "$UART_FILE"; then
        echo "[PASS] Test completion message detected"
    else
        echo "[FAIL] Test completion message missing"
        PASS=false
    fi

    if [ "$PASS" = true ]; then
        echo ""
        echo ">>> ALL CHECKS PASSED - Digital twin behaves as expected <<<"
        exit 0
    else
        echo ""
        echo ">>> SOME CHECKS FAILED <<<"
        exit 1
    fi
else
    echo "[ERROR] No UART output file found"
    exit 1
fi
