#!/bin/bash
# Option D — Renode built-in metrics collection + analysis
# Runs the firmware with metrics enabled, then invokes the Renode
# metrics-analyzer to produce an interactive HTML visualization.
set -e

OUTPUT_DIR=/workspace/output
METRICS_FILE="$OUTPUT_DIR/metrics.bin"
METRICS_OUT="$OUTPUT_DIR/metrics"
RESC_FILE=/workspace/renode/run_metrics.resc

mkdir -p "$OUTPUT_DIR"
rm -f "$METRICS_FILE"
rm -rf "$METRICS_OUT"

echo "=== Running RP2040 firmware in Renode (metrics mode) ==="

renode --disable-xwt --console "$RESC_FILE" || true

echo ""

if [ ! -f "$METRICS_FILE" ]; then
    echo "[ERROR] metrics.bin not produced — 'machine EnableMetrics' may not be"
    echo "        available in this Renode build. Check Renode version."
    exit 1
fi

echo "=== Analyzing metrics ==="

# The metrics-analyzer ships with the Renode portable package
ANALYZER=""
for candidate in \
    /opt/renode/tools/metrics-analyzer/metrics_analyzer.py \
    /opt/renode/tools/metrics-analyzer/main.py \
    /opt/renode/metrics-analyzer/metrics_analyzer.py
do
    if [ -f "$candidate" ]; then
        ANALYZER="$candidate"
        break
    fi
done

if [ -z "$ANALYZER" ]; then
    echo "[ERROR] metrics-analyzer not found in /opt/renode/tools/"
    echo "        Available files under /opt/renode/tools/:"
    find /opt/renode/tools/ -name "*.py" 2>/dev/null | head -20 || echo "  (none)"
    exit 1
fi

mkdir -p "$METRICS_OUT"
python3 "$ANALYZER" "$METRICS_FILE" --output-dir "$METRICS_OUT"

echo ""
echo "=== Metrics report generated ==="
ls "$METRICS_OUT"
echo ""
echo ">>> Open output/metrics/index.html in a browser to explore CPU execution metrics <<<"
