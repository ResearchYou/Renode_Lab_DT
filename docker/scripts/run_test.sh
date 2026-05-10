#!/bin/bash
set -e

OUTPUT_DIR=/workspace/output
UART_FILE="$OUTPUT_DIR/token_uart.txt"
RENODE_LOG="$OUTPUT_DIR/renode.log"
RESC_FILE=/workspace/renode/run_test.resc

mkdir -p "$OUTPUT_DIR"
rm -f "$UART_FILE" "$RENODE_LOG" "$OUTPUT_DIR/report.html"

echo "=== Running STM32F4 mock HID token scenario in Renode ==="

renode --disable-xwt --console "$RESC_FILE" || true

echo ""
echo "=== Token UART Output ==="
if [ ! -f "$UART_FILE" ]; then
    echo "[ERROR] No token UART output captured"
    exit 1
fi
cat "$UART_FILE"

echo ""
echo "=== Validation ==="
FUNCTIONAL_PASS=true
SECURITY_PASS=true

check() {
    local file="$1"
    local pattern="$2"
    local label="$3"
    if [ -f "$file" ] && grep -qF "$pattern" "$file"; then
        echo "[PASS] $label"
    else
        echo "[FAIL] $label"
        FUNCTIONAL_PASS=false
    fi
}

check "$UART_FILE" "TOKEN: boot YK-MOCK challenge-response" "Token boot"
check "$UART_FILE" "TOKEN: GET_INFO seq=1 status=OK" "GET_INFO response"
check "$UART_FILE" "TOKEN: AUTH seq=2 touch=1" "Touch-present AUTH path"
check "$UART_FILE" "TOKEN: AUTH seq=3 touch=1" "Replay AUTH processed"
check "$UART_FILE" "TOKEN: AUTH seq=4 touch=1" "Fresh AUTH processed"
check "$UART_FILE" "TOKEN: SCRIPT COMPLETE" "Script completion"

check "$RENODE_LOG" "MOCK_USB_HOST: OUT seq=1 label=GET_INFO" "Host sent GET_INFO"
check "$RENODE_LOG" "MOCK_USB_HOST: CHECK GET_INFO OK" "Host validated GET_INFO"
check "$RENODE_LOG" "MOCK_USB_HOST: CHECK AUTH_FIRST OK" "Host validated first AUTH"
check "$RENODE_LOG" "MOCK_USB_HOST: CHECK AUTH_FRESH OK" "Host validated fresh AUTH"

if [ -f "$RENODE_LOG" ] && grep -qF "MOCK_USB_HOST: SECURITY_FAIL replay accepted" "$RENODE_LOG"; then
    echo "[FAIL] Replay rejection (intentional challenge bug exposed)"
    SECURITY_PASS=false
else
    echo "[PASS] Replay rejection"
fi

echo ""
echo "=== Generating HTML report ==="
python3 /workspace/scripts/generate_report.py "$OUTPUT_DIR" || true
if [ -f "$OUTPUT_DIR/report.html" ]; then
    echo "[PASS] report.html written to $OUTPUT_DIR"
else
    echo "[WARN] Report generation failed"
fi

echo ""
if [ "$FUNCTIONAL_PASS" = true ] && [ "$SECURITY_PASS" = true ]; then
    echo ">>> ALL TOKEN SECURITY CHECKS PASSED <<<"
    exit 0
fi

if [ "$FUNCTIONAL_PASS" = true ] && [ "$SECURITY_PASS" = false ]; then
    echo ">>> FUNCTIONAL CHECKS PASSED; SECURITY BUG REPRODUCED <<<"
    echo ">>> Student task: make replayed AUTH return ERR_REPLAY <<<"
    exit 1
fi

echo ">>> TOKEN SCENARIO FUNCTIONAL CHECKS FAILED <<<"
exit 1
