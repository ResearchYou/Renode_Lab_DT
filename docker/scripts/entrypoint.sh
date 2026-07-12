#!/usr/bin/env bash
set -euo pipefail

export SECTOR_INDEX="${SECTOR_INDEX:-${JOB_COMPLETION_INDEX:-0}}"

echo "======================================================"
echo "  GHOSTTAG APOCALYPSE // OFFLINE FIND-MESH HACKATHON"
echo "  nRF52840 + Zephyr + Renode + Kubernetes"
echo "======================================================"
echo "sector=$SECTOR_INDEX tags=${TAG_COUNT:-12} gateways=${GATEWAY_COUNT:-3}"
echo

/workspace/scripts/build_firmware.sh
echo
/workspace/scripts/run_test.sh
