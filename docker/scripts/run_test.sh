#!/usr/bin/env bash
set -uo pipefail

OUTPUT_DIR="${OUTPUT_DIR:-/workspace/output}"
RESC_FILE="${RESC_FILE:-/workspace/build/ghosttag.resc}"
MONO_GC_PARAMS="${MONO_GC_PARAMS:-soft-heap-limit=2g}"
export MONO_GC_PARAMS
mkdir -p "$OUTPUT_DIR"
rm -f "$OUTPUT_DIR"/gateway-*.log "$OUTPUT_DIR/tag-sample.log" \
      "$OUTPUT_DIR/renode.log" "$OUTPUT_DIR/validation.json" \
      "$OUTPUT_DIR/report.html"

echo "=== Generating deterministic BLE city sector ==="
RESC_OUTPUT="$RESC_FILE" python3 /workspace/scripts/generate_swarm_resc.py

echo "=== Running position-aware BLE fleet in Renode ==="
echo "[INFO] Renode Mono GC parameters: $MONO_GC_PARAMS"
set +e
renode --disable-xwt --console "$RESC_FILE" >"$OUTPUT_DIR/renode.log" 2>&1
RENODE_STATUS=$?
set -e
echo "Renode exit status: $RENODE_STATUS"

if ls "$OUTPUT_DIR"/gateway-*.log >/dev/null 2>&1; then
    echo "=== Gateway evidence (condensed) ==="
    grep -hE 'GHOST_(GATEWAY_READY|SIGHT|ROGUE|REPLAY|SUMMARY)|FATAL' \
        "$OUTPUT_DIR"/gateway-*.log | tail -120 || true
else
    echo "[ERROR] no gateway UART evidence was produced"
fi

echo "=== Validating fleet behavior ==="
set +e
python3 /workspace/scripts/validate_swarm.py
VALIDATION_STATUS=$?
python3 /workspace/scripts/generate_report.py "$OUTPUT_DIR"
REPORT_STATUS=$?
set -e

if [ "$RENODE_STATUS" -ne 0 ]; then
    echo "[FAIL] Renode did not exit cleanly; inspect $OUTPUT_DIR/renode.log"
fi
if [ "$REPORT_STATUS" -eq 0 ]; then
    echo "[PASS] report.html written to $OUTPUT_DIR"
else
    echo "[FAIL] report generation failed"
fi

if [ "$RENODE_STATUS" -eq 0 ] && [ "$VALIDATION_STATUS" -eq 0 ]; then
    echo ">>> GHOSTTAG FLEET SURVIVED THE APOCALYPSE <<<"
    exit 0
fi

echo ">>> GHOSTTAG FLEET FAILED - complete the protocol TODOs <<<"
exit 1
