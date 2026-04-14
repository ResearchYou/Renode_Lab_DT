#!/bin/bash
# Long-running daemon for the digital-twin service.
# Watches for a trigger file written by the IDE runner, executes the test
# pipeline, and writes output (including a final EXIT:<code> line) to the
# shared log file so the runner can stream it without needing Docker access.

TRIGGER=/workspace/output/.trigger
LOG=/workspace/output/run.log

mkdir -p /workspace/output
printf '[daemon] digital-twin ready\n'

while true; do
    if [ -f "$TRIGGER" ]; then
        rm -f "$TRIGGER"
        printf '[daemon] run triggered\n'

        : > "$LOG"   # truncate log for new run

        rc=0
        /workspace/scripts/entrypoint.sh >> "$LOG" 2>&1 || rc=$?

        printf 'EXIT:%d\n' "$rc" >> "$LOG"
        printf '[daemon] run finished (exit %d)\n' "$rc"
    fi
    sleep 0.2
done
