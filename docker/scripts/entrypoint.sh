#!/usr/bin/env bash
set -euo pipefail

export SECTOR_INDEX="${SECTOR_INDEX:-${JOB_COMPLETION_INDEX:-0}}"
export OUTPUT_DIR="${OUTPUT_DIR:-/workspace/output}"

echo "======================================================"
echo "  GHOSTTAG APOCALYPSE // OFFLINE FIND-MESH HACKATHON"
echo "  nRF52840 + Zephyr + Renode + Kubernetes"
echo "======================================================"
echo "sector=$SECTOR_INDEX tags=${TAG_COUNT:-6} gateways=${GATEWAY_COUNT:-3}"
echo

mkdir -p "$OUTPUT_DIR"
rm -f "$OUTPUT_DIR"/gateway-*.log "$OUTPUT_DIR/tag-sample.log" \
    "$OUTPUT_DIR/renode.log" "$OUTPUT_DIR/validation.json" \
    "$OUTPUT_DIR/report.html"

set +e
/workspace/scripts/build_firmware.sh
BUILD_STATUS=$?
set -e

if [ "$BUILD_STATUS" -ne 0 ]; then
    echo
    echo "=== Writing failed validation report ==="
    python3 /workspace/scripts/validate_swarm.py || true
    if python3 /workspace/scripts/generate_report.py "$OUTPUT_DIR"; then
        echo "[PASS] report.html written to $OUTPUT_DIR"
    else
        echo "[FAIL] report generation failed"
    fi
    echo ">>> GHOSTTAG FLEET FAILED - complete the protocol TODOs <<<"
    exit 1
fi

echo
/workspace/scripts/run_test.sh
