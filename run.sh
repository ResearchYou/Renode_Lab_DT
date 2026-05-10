#!/usr/bin/env bash
# run.sh — wrapper around docker compose that auto-detects the Docker socket.
#
# Usage:  ./run.sh [compose args...]
#   e.g.  ./run.sh up ide
#          ./run.sh up -d ide
#          ./run.sh down
#
# The Docker socket path varies by installation:
#   /var/run/docker.sock          — standard Docker Engine (root daemon)
#   /run/docker.sock              — alternative standard path
#   /run/user/<uid>/docker.sock   — rootless Docker
#   <context host>                — active Docker context (Desktop, remote, etc.)
#
# DOCKER_SOCKET_PATH can still be set manually to override auto-detection.

set -euo pipefail

detect_socket() {
    # 1. Explicit environment override
    if [ -n "${DOCKER_SOCKET_PATH:-}" ] && [ -S "$DOCKER_SOCKET_PATH" ]; then
        printf '%s' "$DOCKER_SOCKET_PATH"
        return
    fi

    # 2. Active Docker context (covers Docker Desktop, remote daemons)
    local ctx_host
    ctx_host=$(docker context inspect --format '{{.Endpoints.docker.Host}}' 2>/dev/null || true)
    local ctx_sock="${ctx_host#unix://}"
    if [ -n "$ctx_sock" ] && [ -S "$ctx_sock" ]; then
        printf '%s' "$ctx_sock"
        return
    fi

    # 3. Common fixed paths
    local uid
    uid=$(id -u)
    for candidate in \
        /var/run/docker.sock \
        /run/docker.sock \
        "${XDG_RUNTIME_DIR:-/run/user/$uid}/docker.sock" \
        "/run/user/$uid/docker.sock"
    do
        if [ -S "$candidate" ]; then
            printf '%s' "$candidate"
            return
        fi
    done

    # 4. Give up — let docker compose fail with a clear message
    printf '/var/run/docker.sock'
}

DOCKER_SOCKET_PATH="$(detect_socket)"
export DOCKER_SOCKET_PATH

HOST_PROJECT_DIR="${HOST_PROJECT_DIR:-$PWD}"
export HOST_PROJECT_DIR

echo "run.sh: Docker socket → $DOCKER_SOCKET_PATH"
exec docker compose "$@"
