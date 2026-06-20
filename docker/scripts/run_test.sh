#!/bin/bash
set -e

OUTPUT_DIR=/workspace/output
UART_FILE="$OUTPUT_DIR/token_uart.txt"
RENODE_LOG="$OUTPUT_DIR/renode.log"
RESC_FILE=/workspace/renode/run_test.resc

mkdir -p "$OUTPUT_DIR"
rm -f "$UART_FILE" "$RENODE_LOG" "$OUTPUT_DIR/report.html"

echo "=== Running STM32F4 HMAC-SHA1 validation scenario in Renode ==="

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

check "$UART_FILE" "TOKEN: boot HMAC-SHA1 functional validation" "Token boot"
check "$UART_FILE" "TOKEN: GET_INFO seq=1 status=OK" "GET_INFO response"
check "$UART_FILE" "TOKEN: HMAC_SHA1 vector=1 status=OK digest=B617318655057264E28BC0B6FB378C8EF146BE00" "RFC 2202 test case 1 UART digest"
check "$UART_FILE" "TOKEN: HMAC_SHA1 vector=2 status=OK digest=EFFCDF6AE5EB2FA2D27416D5F184DF9C259A7C79" "RFC 2202 test case 2 UART digest"
check "$UART_FILE" "TOKEN: HMAC_SHA1 vector=3 status=OK digest=125D7342B9AC11CD91A39AF48AA17B4F63F175D3" "RFC 2202 test case 3 UART digest"
check "$UART_FILE" "TOKEN: SCRIPT COMPLETE" "Script completion"

check "$RENODE_LOG" "MOCK_USB_HOST: OUT seq=1 label=GET_INFO" "Host sent GET_INFO"
check "$RENODE_LOG" "MOCK_USB_HOST: CHECK GET_INFO OK" "Host validated GET_INFO"
check "$RENODE_LOG" "MOCK_USB_HOST: CHECK RFC2202_TC1 OK" "Host validated RFC 2202 test case 1"
check "$RENODE_LOG" "MOCK_USB_HOST: CHECK RFC2202_TC2 OK" "Host validated RFC 2202 test case 2"
check "$RENODE_LOG" "MOCK_USB_HOST: CHECK RFC2202_TC3 OK" "Host validated RFC 2202 test case 3"

echo ""
echo "=== Generating HTML report ==="
python3 /workspace/scripts/generate_report.py "$OUTPUT_DIR" || true
if [ -f "$OUTPUT_DIR/report.html" ]; then
    echo "[PASS] report.html written to $OUTPUT_DIR"
else
    echo "[WARN] Report generation failed"
fi

echo ""
if [ "$FUNCTIONAL_PASS" = true ]; then
    echo ">>> HMAC-SHA1 FUNCTIONAL VALIDATION PASSED <<<"
    exit 0
fi

echo ">>> HMAC-SHA1 FUNCTIONAL VALIDATION FAILED <<<"
exit 1
