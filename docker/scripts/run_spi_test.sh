#!/bin/bash
set -e

OUTPUT_DIR=/workspace/output
RESC_FILE=/workspace/renode/scripts/spi_test.resc

mkdir -p "$OUTPUT_DIR"
rm -f "$OUTPUT_DIR/master_uart.txt" \
      "$OUTPUT_DIR/slave_uart.txt"  \
      "$OUTPUT_DIR/renode.log"

echo "=== Running dual-board STM32F746 STMP v1 test in Renode ==="

renode --disable-xwt --console "$RESC_FILE" || true

echo ""
echo "=== MASTER UART Output ==="
if [ -f "$OUTPUT_DIR/master_uart.txt" ]; then
    cat "$OUTPUT_DIR/master_uart.txt"
else
    echo "[ERROR] No master UART output captured"
fi

echo ""
echo "=== SLAVE UART Output ==="
if [ -f "$OUTPUT_DIR/slave_uart.txt" ]; then
    cat "$OUTPUT_DIR/slave_uart.txt"
else
    echo "[ERROR] No slave UART output captured"
fi

echo ""
echo "=== Validation ==="
PASS=true

check() {
    local file="$1" pattern="$2" label="$3"
    if [ -f "$file" ] && grep -qF "$pattern" "$file"; then
        echo "[PASS] $label"
    else
        echo "[FAIL] $label"
        PASS=false
    fi
}

check_absent() {
    local file="$1" pattern="$2" label="$3"
    if [ -f "$file" ] && grep -qF "$pattern" "$file"; then
        echo "[FAIL] $label (unexpected content found)"
        PASS=false
    else
        echo "[PASS] $label"
    fi
}

# ── Boot messages ─────────────────────────────────────────────────────────
check "$OUTPUT_DIR/master_uart.txt" "MASTER: STMP v1 test start" "Master boot"
check "$OUTPUT_DIR/slave_uart.txt"  "SLAVE: STMP v1 ready"       "Slave boot"
check "$OUTPUT_DIR/master_uart.txt" "MASTER: ALL TESTS PASSED"   "Master all tests passed"
check "$OUTPUT_DIR/slave_uart.txt"  "SLAVE: ALL DONE"            "Slave all done"

# ── Per-exchange result lines ─────────────────────────────────────────────
check "$OUTPUT_DIR/master_uart.txt" "MASTER: [1] PING OK"                 "Master [1] PING"
check "$OUTPUT_DIR/master_uart.txt" "MASTER: [2] PING OK"                 "Master [2] PING"
check "$OUTPUT_DIR/master_uart.txt" "MASTER: [3] REG_WR[0]=CAFEBABE OK"   "Master [3] REG_WR[0]"
check "$OUTPUT_DIR/master_uart.txt" "MASTER: [4] REG_RD[0]=CAFEBABE OK"   "Master [4] REG_RD[0]"
check "$OUTPUT_DIR/master_uart.txt" "MASTER: [5] REG_WR[1]=DEAD1234 OK"   "Master [5] REG_WR[1]"
check "$OUTPUT_DIR/master_uart.txt" "MASTER: [6] REG_RD[1]=DEAD1234 OK"   "Master [6] REG_RD[1]"
check "$OUTPUT_DIR/master_uart.txt" "MASTER: [7] STAT rx=7 tx=6 OK"       "Master [7] STAT"
check "$OUTPUT_DIR/master_uart.txt" "MASTER: [8] PING OK"                 "Master [8] PING"
check "$OUTPUT_DIR/master_uart.txt" "MASTER: [9] REG_RD[0]=CAFEBABE OK"   "Master [9] REG_RD[0] persist"
check "$OUTPUT_DIR/master_uart.txt" "MASTER: [10] STAT rx=10 tx=9 OK"     "Master [10] STAT"

check "$OUTPUT_DIR/slave_uart.txt"  "SLAVE: [1] PING"            "Slave [1] PING"
check "$OUTPUT_DIR/slave_uart.txt"  "SLAVE: [2] PING"            "Slave [2] PING"
check "$OUTPUT_DIR/slave_uart.txt"  "SLAVE: [3] REG_WR[0]"       "Slave [3] REG_WR[0]"
check "$OUTPUT_DIR/slave_uart.txt"  "SLAVE: [4] REG_RD[0]"       "Slave [4] REG_RD[0]"
check "$OUTPUT_DIR/slave_uart.txt"  "SLAVE: [5] REG_WR[1]"       "Slave [5] REG_WR[1]"
check "$OUTPUT_DIR/slave_uart.txt"  "SLAVE: [6] REG_RD[1]"       "Slave [6] REG_RD[1]"
check "$OUTPUT_DIR/slave_uart.txt"  "SLAVE: [7] STAT"            "Slave [7] STAT"
check "$OUTPUT_DIR/slave_uart.txt"  "SLAVE: [8] PING"            "Slave [8] PING"
check "$OUTPUT_DIR/slave_uart.txt"  "SLAVE: [9] REG_RD[0]"       "Slave [9] REG_RD[0]"
check "$OUTPUT_DIR/slave_uart.txt"  "SLAVE: [10] STAT"           "Slave [10] STAT"

# ── Negative checks ───────────────────────────────────────────────────────
check_absent "$OUTPUT_DIR/master_uart.txt" "FAIL"    "No master failures"
check_absent "$OUTPUT_DIR/slave_uart.txt"  "ERR"     "No slave errors"

# ── Renode bridge log: exact STMP v1 frame verification ──────────────────
#
# Expected frames (computed with CRC-8/SMBUS, poly=0x07):
#
#  PKT  1 MOSI: A5 01 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 DD 55  PING_REQ seq=1
#  PKT  1 MISO: A5 01 11 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 66 55  PING_RSP seq=1 echo=1
#  PKT  3 MOSI: A5 03 20 00 00 BE BA FE CA 00 00 00 00 00 00 00 00 00 00 D3 55  REG_WR reg=0 val=CAFEBABE
#  PKT  3 MISO: A5 03 21 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 A0 55  REG_WR_ACK reg=0 status=OK
#  PKT  4 MOSI: A5 04 30 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 98 55  REG_RD reg=0
#  PKT  4 MISO: A5 04 31 00 00 BE BA FE CA 00 00 00 00 00 00 00 00 00 00 EB 55  REG_RD_RSP reg=0 val=CAFEBABE
#  PKT  5 MOSI: A5 05 20 00 01 34 12 AD DE 00 00 00 00 00 00 00 00 00 00 82 55  REG_WR reg=1 val=DEAD1234
#  PKT  6 MISO: A5 06 31 00 01 34 12 AD DE 00 00 00 00 00 00 00 00 00 00 12 55  REG_RD_RSP reg=1 val=DEAD1234
#  PKT  7 MISO: A5 07 41 00 07 00 00 00 06 00 00 00 00 00 00 00 00 00 00 06 55  STAT_RSP rx=7 tx=6 err=0
#  PKT 10 MISO: A5 0A 41 00 0A 00 00 00 09 00 00 00 00 00 00 00 00 00 00 AA 55  STAT_RSP rx=10 tx=9 err=0
#
if [ -f "$OUTPUT_DIR/renode.log" ]; then
    delivered=$(grep -c "SPI_LINE: TX_PACKET" "$OUTPUT_DIR/renode.log" 2>/dev/null || echo 0)
    echo "[INFO] SPI bridge delivered $delivered packets"

    # ── Frame integrity: MOSI requests ──
    check "$OUTPUT_DIR/renode.log" \
        "SPI_BUS: MOSI_REQ seq=1 frame=A5 01 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 DD 55" \
        "MOSI [1] PING_REQ frame"
    check "$OUTPUT_DIR/renode.log" \
        "SPI_BUS: MOSI_REQ seq=3 frame=A5 03 20 00 00 BE BA FE CA 00 00 00 00 00 00 00 00 00 00 D3 55" \
        "MOSI [3] REG_WR[0]=CAFEBABE frame"
    check "$OUTPUT_DIR/renode.log" \
        "SPI_BUS: MOSI_REQ seq=4 frame=A5 04 30 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 98 55" \
        "MOSI [4] REG_RD[0] frame"
    check "$OUTPUT_DIR/renode.log" \
        "SPI_BUS: MOSI_REQ seq=5 frame=A5 05 20 00 01 34 12 AD DE 00 00 00 00 00 00 00 00 00 00 82 55" \
        "MOSI [5] REG_WR[1]=DEAD1234 frame"
    check "$OUTPUT_DIR/renode.log" \
        "SPI_BUS: MOSI_REQ seq=7 frame=A5 07 40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 48 55" \
        "MOSI [7] STAT_REQ frame"
    check "$OUTPUT_DIR/renode.log" \
        "SPI_BUS: MOSI_REQ seq=10 frame=A5 0A 40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 9D 55" \
        "MOSI [10] STAT_REQ frame"

    # ── Frame integrity: MISO responses ──
    check "$OUTPUT_DIR/renode.log" \
        "SPI_BUS: MISO_RSP seq=1 frame=A5 01 11 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 66 55" \
        "MISO [1] PING_RSP frame"
    check "$OUTPUT_DIR/renode.log" \
        "SPI_BUS: MISO_RSP seq=3 frame=A5 03 21 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 A0 55" \
        "MISO [3] REG_WR_ACK frame"
    check "$OUTPUT_DIR/renode.log" \
        "SPI_BUS: MISO_RSP seq=4 frame=A5 04 31 00 00 BE BA FE CA 00 00 00 00 00 00 00 00 00 00 EB 55" \
        "MISO [4] REG_RD_RSP[0]=CAFEBABE frame"
    check "$OUTPUT_DIR/renode.log" \
        "SPI_BUS: MISO_RSP seq=6 frame=A5 06 31 00 01 34 12 AD DE 00 00 00 00 00 00 00 00 00 00 12 55" \
        "MISO [6] REG_RD_RSP[1]=DEAD1234 frame"
    check "$OUTPUT_DIR/renode.log" \
        "SPI_BUS: MISO_RSP seq=7 frame=A5 07 41 00 07 00 00 00 06 00 00 00 00 00 00 00 00 00 00 06 55" \
        "MISO [7] STAT_RSP rx=7 tx=6 frame"
    check "$OUTPUT_DIR/renode.log" \
        "SPI_BUS: MISO_RSP seq=10 frame=A5 0A 41 00 0A 00 00 00 09 00 00 00 00 00 00 00 00 00 00 AA 55" \
        "MISO [10] STAT_RSP rx=10 tx=9 frame"

    # ── Packet counts ─────────────────────────────────────────────────────
    tx_count=$(grep -c "SPI_LINE: TX_PACKET"         "$OUTPUT_DIR/renode.log" || true)
    rx_count=$(grep -c "SPI_LINE: RX_TRANSFER"       "$OUTPUT_DIR/renode.log" || true)
    mosi_cnt=$(grep -c "SPI_BUS: MOSI_REQ"           "$OUTPUT_DIR/renode.log" || true)
    miso_cnt=$(grep -c "SPI_BUS: MISO_RSP"           "$OUTPUT_DIR/renode.log" || true)

    check_count() {
        local name="$1" count="$2" expected="$3"
        if [ "$count" -eq "$expected" ]; then
            echo "[PASS] $name = $count"
        else
            echo "[FAIL] $name = $count (expected $expected)"
            PASS=false
        fi
    }
    check_count "SPI TX packets"   "$tx_count"  10
    check_count "SPI RX transfers" "$rx_count"  10
    check_count "MOSI frames"      "$mosi_cnt"  10
    check_count "MISO frames"      "$miso_cnt"  10
else
    echo "[WARN] No Renode log found; frame verification skipped"
fi

# ── Generate HTML report ──────────────────────────────────────────────────
echo ""
echo "=== Generating HTML report ==="
python3 /workspace/scripts/generate_spi_report.py || true

if [ -f "$OUTPUT_DIR/report.html" ]; then
    echo "[PASS] report.html written to $OUTPUT_DIR"
else
    echo "[WARN] Report generation failed"
fi

echo ""
if [ "$PASS" = true ]; then
    echo ">>> ALL STMP v1 PROTOCOL CHECKS PASSED <<<"
    exit 0
else
    echo ">>> SOME STMP v1 PROTOCOL CHECKS FAILED <<<"
    exit 1
fi
