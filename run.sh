#!/usr/bin/env bash
# run.sh — wrapper around docker compose that auto-detects the Docker socket.
#
# Usage:  ./run.sh [compose args...]
#   e.g.  ./run.sh up ide
#          ./run.sh up -d ide
#          COMPOSE_PROJECT_NAME=user1 IDE_PORT=8444 ./run.sh up -d ide
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

DOCKER_HOST="${DOCKER_HOST:-unix://$DOCKER_SOCKET_PATH}"
export DOCKER_HOST

COMPOSE_PROJECT_NAME="${COMPOSE_PROJECT_NAME:-renode_dt}"
export COMPOSE_PROJECT_NAME

IDE_PORT="${IDE_PORT:-8443}"
export IDE_PORT

echo "run.sh: Docker socket → $DOCKER_SOCKET_PATH"
echo "run.sh: Docker host → $DOCKER_HOST"
echo "run.sh: Compose project → $COMPOSE_PROJECT_NAME"
echo "run.sh: IDE port → $IDE_PORT"
exec docker compose "$@"
