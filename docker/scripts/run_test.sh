#!/bin/bash
set -e

OUTPUT_DIR=/workspace/output
UART_FILE="$OUTPUT_DIR/uart_output.txt"
RENODE_LOG="$OUTPUT_DIR/renode.log"
RESC_FILE=/workspace/renode/run_test.resc

mkdir -p "$OUTPUT_DIR"
rm -f "$UART_FILE" "$RENODE_LOG" "$OUTPUT_DIR/report.html"

echo "=== Running RP2040 sensor filtering scenario in Renode ==="

renode --disable-xwt --console "$RESC_FILE" || true

echo ""
echo "=== UART Output ==="
if [ ! -f "$UART_FILE" ]; then
    echo "[ERROR] No UART output file found"
    exit 1
fi

cat "$UART_FILE"
echo ""

PASS=true

check() {
    local pattern="$1"
    local label="$2"
    if grep -qF "$pattern" "$UART_FILE"; then
        echo "[PASS] $label"
    else
        echo "[FAIL] $label"
        PASS=false
    fi
}

count_check() {
    local pattern="$1"
    local expected="$2"
    local label="$3"
    local actual
    actual=$(grep -cF "$pattern" "$UART_FILE" || true)
    if [ "$actual" = "$expected" ]; then
        echo "[PASS] $label ($actual)"
    else
        echo "[FAIL] $label expected=$expected actual=$actual"
        PASS=false
    fi
}

check "BOOT: RP2040 sensor filter TinyML lab" "Boot message"
check "I2C sensor ready addr=0x52" "I2C sensor init"
count_check "SAMPLE seq=" 10 "Sensor sample count"
check "THRESHOLD seq=4 decision=DROP" "Threshold rejects high outlier"
check "THRESHOLD seq=6 decision=DROP" "Threshold rejects low outlier"
check "MODEL seq=2 decision=DROP" "Model rejects borderline jump"
check "MODEL seq=7 decision=DROP" "Model rejects second borderline jump"
count_check "DISAGREE seq=" 2 "Method disagreement count"
check "SUMMARY threshold_keep=8 threshold_drop=2 model_keep=6 model_drop=4 disagreements=2" "Summary counts"
check "TEST COMPLETE" "Test completion"

echo ""
echo "=== Generating report ==="
python3 /workspace/scripts/generate_report.py "$OUTPUT_DIR" || true
if [ -f "$OUTPUT_DIR/report.html" ]; then
    echo "[PASS] report.html written to $OUTPUT_DIR"
else
    echo "[WARN] Report generation failed"
fi

echo ""
if [ "$PASS" = true ]; then
    echo ">>> ALL CHECKS PASSED - sensor filters behave as expected <<<"
    exit 0
fi

echo ">>> SOME CHECKS FAILED - complete the firmware TODOs <<<"
exit 1
