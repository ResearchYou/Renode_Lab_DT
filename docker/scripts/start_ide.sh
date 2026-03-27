#!/bin/bash
set -euo pipefail

CHALLENGE_ROOT="/home/coder/challenge"
WORKSPACE_FILE="/home/coder/.config/code-server/challenge.code-workspace"

# Create directories and fix ownership (running as root)
mkdir -p "${CHALLENGE_ROOT}/firmware" \
         "${CHALLENGE_ROOT}/problem" \
         "$(dirname "${WORKSPACE_FILE}")"
chown coder:coder "${CHALLENGE_ROOT}" \
                  "${CHALLENGE_ROOT}/firmware" \
                  "${CHALLENGE_ROOT}/problem" \
                  "$(dirname "${WORKSPACE_FILE}")"

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

# Start runner (as root — needs access to the Docker socket)
python3 /usr/local/bin/runner.py &

# Start nginx (all temp paths are under /tmp per nginx.conf)
nginx -c /home/coder/nginx.conf

# Drop to coder for code-server
exec su - coder -s /bin/bash -c "exec code-server '${WORKSPACE_FILE}'"
