#!/bin/bash
set -euo pipefail

CHALLENGE_ROOT="/home/coder/challenge"
SEED_ROOT="/opt/challenge-seed"
WORKSPACE_FILE="/home/coder/.config/code-server/challenge.code-workspace"

# Create directories and fix ownership (running as root)
mkdir -p "${CHALLENGE_ROOT}/firmware" \
         "${CHALLENGE_ROOT}/problem" \
         "${CHALLENGE_ROOT}/output" \
         "$(dirname "${WORKSPACE_FILE}")"
chown coder:coder "${CHALLENGE_ROOT}" \
                  "$(dirname "${WORKSPACE_FILE}")"
chown -R coder:coder "${CHALLENGE_ROOT}/firmware" \
                     "${CHALLENGE_ROOT}/problem" \
                     "${CHALLENGE_ROOT}/output" 2>/dev/null || true

seed_if_empty() {
    local source="$1"
    local target="$2"

    if [ ! -d "$source" ]; then
        return
    fi

    if [ -z "$(find "$target" -mindepth 1 -maxdepth 1 -print -quit)" ]; then
        cp -a "${source}/." "$target/"
        chown -R coder:coder "$target" 2>/dev/null || true
    fi
}

seed_if_empty "${SEED_ROOT}/firmware" "${CHALLENGE_ROOT}/firmware"
seed_if_empty "${SEED_ROOT}/problem" "${CHALLENGE_ROOT}/problem"

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
