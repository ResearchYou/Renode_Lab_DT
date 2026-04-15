#!/bin/bash
set -euo pipefail

CHALLENGE_ROOT="/home/coder/challenge"
WORKSPACE_FILE="/home/coder/.config/code-server/challenge.code-workspace"

# Create directories and fix ownership (running as root)
mkdir -p "${CHALLENGE_ROOT}/firmware" \
         "${CHALLENGE_ROOT}/problem" \
         "$(dirname "${WORKSPACE_FILE}")"
chown    coder:coder "${CHALLENGE_ROOT}" \
                     "$(dirname "${WORKSPACE_FILE}")"
# Recursively fix bind-mounted content so coder can edit files
chown -R coder:coder "${CHALLENGE_ROOT}/firmware" \
                     "${CHALLENGE_ROOT}/problem" \
                     "${CHALLENGE_ROOT}/output" 2>/dev/null || true

# Write workspace file
cat > "${WORKSPACE_FILE}" <<'JSON'
{
  "folders": [
    { "path": "/home/coder/challenge/firmware" },
    { "path": "/home/coder/challenge/problem" }
  ],
  "settings": {
    "workbench.colorTheme": "Solarized Dark",
    "files.exclude": {
      "**/.git": true,
      "**/.DS_Store": true
    }
  }
}
JSON
chown coder:coder "${WORKSPACE_FILE}"

# Start runner (communicates with digital-twin via shared output volume)
python3 /usr/local/bin/runner.py &

# Start nginx (all temp paths are under /tmp per nginx.conf)
nginx -c /home/coder/nginx.conf

# Drop to coder for code-server
exec su - coder -s /bin/bash -c "exec code-server '${WORKSPACE_FILE}'"
