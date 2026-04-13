#!/bin/bash
set -e

OUTPUT_DIR=/workspace/output
RESC_FILE=/workspace/renode/run_scenario.resc
MASTER_UART="$OUTPUT_DIR/master_uart.txt"
NODE_UART="$OUTPUT_DIR/node_uart.txt"

mkdir -p "$OUTPUT_DIR"
rm -f "$MASTER_UART" "$NODE_UART"

echo "=== Running dual-RP2040 scenario in Renode ==="

renode --disable-xwt --console "$RESC_FILE" || true

echo ""
echo "=== Node UART ==="
if [ ! -f "$NODE_UART" ]; then
    echo "[ERROR] Node UART output not found"
    exit 1
fi
cat "$NODE_UART"

echo ""
echo "=== Master UART ==="
if [ ! -f "$MASTER_UART" ]; then
    echo "[ERROR] Master UART output not found"
    exit 1
fi
cat "$MASTER_UART"

echo ""
echo "=== Validation ==="
PASS=true

# Node checks
grep -q "\[NODE\] Boot:"         "$NODE_UART"  && echo "[PASS] Node boot"          || { echo "[FAIL] Node boot missing";          PASS=false; }
grep -q "\[NODE\] HDC1080:"      "$NODE_UART"  && echo "[PASS] Node sensor read"   || { echo "[FAIL] Node sensor read missing";   PASS=false; }
grep -q "\[NODE\] LoRa TX:"      "$NODE_UART"  && echo "[PASS] Node LoRa TX"       || { echo "[FAIL] Node LoRa TX missing";       PASS=false; }

# Master checks
grep -q "\[MASTER\] Boot:"       "$MASTER_UART" && echo "[PASS] Master boot"       || { echo "[FAIL] Master boot missing";        PASS=false; }
grep -q "\[MASTER\] Poll #"      "$MASTER_UART" && echo "[PASS] Master poll"       || { echo "[FAIL] Master poll missing";        PASS=false; }
grep -q "humidity="              "$MASTER_UART" && echo "[PASS] Master humidity"   || { echo "[FAIL] Master humidity missing";    PASS=false; }

echo ""
if [ "$PASS" = true ]; then
    echo ">>> ALL CHECKS PASSED <<<"
    exit 0
else
    echo ">>> SOME CHECKS FAILED <<<"
    exit 1
fi
