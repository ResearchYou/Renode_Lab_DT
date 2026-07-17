#!/usr/bin/env bash
# Build, tag, and push the platform images through an SSH tunnel to Zot.
#
# This bypasses Cloudflare upload limits by pushing to localhost:<port>, which
# is forwarded to the registry LXC's Zot service.

set -euo pipefail

if [ -z "${DOCKER_HOST:-}" ] && [ -S /var/run/docker.sock ]; then
    DOCKER_HOST="unix:///var/run/docker.sock"
    export DOCKER_HOST
fi

SSH_HOST="${SSH_HOST:-server}"
PLATFORM_HOST="${PLATFORM_HOST:-cloud_s1}"

REGISTRY_LXC_ID="${REGISTRY_LXC_ID:-215}"
REMOTE_REGISTRY_HOST="${REMOTE_REGISTRY_HOST:-10.10.10.215}"
REMOTE_REGISTRY_PORT="${REMOTE_REGISTRY_PORT:-8080}"
REMOTE_REGISTRY="${REMOTE_REGISTRY_HOST}:${REMOTE_REGISTRY_PORT}"

LOCAL_BIND_HOST="${LOCAL_BIND_HOST:-127.0.0.1}"
LOCAL_REGISTRY_HOST="${LOCAL_REGISTRY_HOST:-127.0.0.1}"
LOCAL_REGISTRY_PORT="${LOCAL_REGISTRY_PORT:-5001}"

PLATFORM_REGISTRY="${PLATFORM_REGISTRY:-oci.cyb3rhell.com}"
PLATFORM_NAMESPACE="${PLATFORM_NAMESPACE:-challenge-platform}"

BUILD="${BUILD:-1}"

MODE="${1:-push}"
LOCAL_REGISTRY="${LOCAL_REGISTRY_HOST}:${LOCAL_REGISTRY_PORT}"
LOCAL_FORWARD="${LOCAL_BIND_HOST}:${LOCAL_REGISTRY_PORT}"
TUNNEL_PID=""
TEMP_DOCKER_CONFIG=""
SSH_TUNNEL_LOG=""

usage() {
    cat <<EOF
Usage:
  ./push-platform-images.sh [mode] [image...]

Modes:
  push        (default) build, tag, and push through a temporary SSH tunnel
  rsync-push  build, save to tar, rsync to server, push from server
  login       open a temporary SSH tunnel and run docker login
  tunnel      keep an SSH tunnel open for manual docker commands

Images (default: all):
  digital-twin
  showcase
  ide

For normal pushes, authenticate once with:
  docker login oci.cyb3rhell.com

rsync-push prerequisites (one-time on the server):
  Add {"insecure-registries":["10.10.10.215:8080"]} to /etc/docker/daemon.json
  then: systemctl restart docker

Environment overrides:
  BUILD=0                         skip docker compose build
  SCENARIO_TAG=tag                 override detected active scenario tag
  LOCAL_REGISTRY_PORT=5002         use a different local tunnel port
  SSH_HOST=server                  SSH host that can reach the registry LXC
  PLATFORM_HOST=cloud_s1           host used to detect the active scenario tag
  REMOTE_REGISTRY_HOST=10.10.10.215
  REMOTE_REGISTRY_PORT=8080
  DOCKER_HOST=unix:///...          Docker daemon to build/push with
EOF
}

if [ "$MODE" = "-h" ] || [ "$MODE" = "--help" ]; then
    usage
    exit 0
fi

case "$MODE" in
    push|rsync-push|login|tunnel) ;;
    *)
        usage >&2
        exit 2
        ;;
esac

if [ $# -le 1 ]; then
    IMAGES=(digital-twin ide)
else
    IMAGES=("${@:2}")
fi

for _img in "${IMAGES[@]}"; do
    case "$_img" in
        digital-twin|showcase|ide) ;;
        *)
            echo "push-platform-images: unknown image '${_img}'; valid: digital-twin showcase ide" >&2
            exit 2
            ;;
    esac
done
unset _img

has_image() { local i; for i in "${IMAGES[@]}"; do [ "$i" = "$1" ] && return 0; done; return 1; }

cleanup() {
    if [ -n "$TUNNEL_PID" ] && kill -0 "$TUNNEL_PID" 2>/dev/null; then
        kill "$TUNNEL_PID"
        wait "$TUNNEL_PID" 2>/dev/null || true
    fi
    if [ -n "$TEMP_DOCKER_CONFIG" ]; then
        rm -rf "$TEMP_DOCKER_CONFIG"
    fi
    if [ -n "$SSH_TUNNEL_LOG" ]; then
        rm -f "$SSH_TUNNEL_LOG"
    fi
    rm -f /tmp/push-platform-digital-twin.tar.gz \
          /tmp/push-platform-showcase.tar.gz \
          /tmp/push-platform-ide.tar.gz \
          /tmp/push-platform-creds.json
}

port_is_open() {
    timeout 1 bash -c "</dev/tcp/${LOCAL_BIND_HOST}/${LOCAL_REGISTRY_PORT}" 2>/dev/null
}

registry_is_reachable() {
    local status
    status="$(
        curl -sS -o /dev/null -w '%{http_code}' --max-time 2 "http://${LOCAL_REGISTRY}/v2/" 2>/dev/null || true
    )"
    [ "$status" = "200" ] || [ "$status" = "401" ]
}

detect_scenario_tag() {
    if [ -n "${SCENARIO_TAG:-}" ]; then
        printf '%s\n' "$SCENARIO_TAG"
        return
    fi

    local detected
    detected="$(
        ssh "$PLATFORM_HOST" \
            "cd ~/eg106-platform && awk '/^[[:space:]]*tag:/ { print \$2; exit }' k8s/base/frontend/configmap-active-scenario.yaml" \
            2>/dev/null || true
    )"

    if [ -n "$detected" ]; then
        printf '%s\n' "$detected"
    else
        printf '%s\n' "nrf52840-swarm-ghosttag-apocalypse"
    fi
}

check_remote_registry() {
    ssh -o BatchMode=yes -o ConnectTimeout=5 "$SSH_HOST" \
        "bash -lc 'timeout 2 bash -c \"</dev/tcp/${REMOTE_REGISTRY_HOST}/${REMOTE_REGISTRY_PORT}\"'" \
        >/dev/null 2>&1
}

start_tunnel() {
    if registry_is_reachable; then
        echo "push-platform-images: using existing ${LOCAL_REGISTRY} registry listener"
        return
    elif port_is_open; then
        echo "push-platform-images: ${LOCAL_FORWARD} is already in use but is not a Docker registry" >&2
        echo "push-platform-images: set LOCAL_REGISTRY_PORT=5002 or stop the process using that port" >&2
        exit 1
    fi

    echo "push-platform-images: opening tunnel ${LOCAL_REGISTRY} -> ${REMOTE_REGISTRY} via ${SSH_HOST}"
    SSH_TUNNEL_LOG="$(mktemp)"
    ssh \
        -n \
        -o ExitOnForwardFailure=yes \
        -o ServerAliveInterval=30 \
        -o ServerAliveCountMax=3 \
        -N \
        -L "${LOCAL_FORWARD}:${REMOTE_REGISTRY}" \
        "$SSH_HOST" 2>"$SSH_TUNNEL_LOG" &
    TUNNEL_PID="$!"
    trap cleanup EXIT INT TERM

    for _ in $(seq 1 30); do
        if ! kill -0 "$TUNNEL_PID" 2>/dev/null; then
            wait "$TUNNEL_PID" 2>/dev/null || true
            echo "push-platform-images: ssh tunnel exited before becoming ready" >&2
            if [ -s "$SSH_TUNNEL_LOG" ]; then
                sed 's/^/ssh: /' "$SSH_TUNNEL_LOG" >&2
            fi
            exit 1
        fi

        if registry_is_reachable; then
            echo "push-platform-images: tunnel is ready"
            return
        fi
        sleep 0.2
    done

    echo "push-platform-images: tunnel did not become ready" >&2
    exit 1
}

build_images() {
    if [ "$BUILD" = "0" ]; then
        echo "push-platform-images: skipping build because BUILD=0"
        return
    fi

    local compose_images=()
    has_image digital-twin && compose_images+=(digital-twin)
    has_image ide && compose_images+=(ide)
    if [ "${#compose_images[@]}" -gt 0 ]; then
        ./run.sh build "${compose_images[@]}"
    fi

    if has_image showcase; then
        if ! docker image inspect renode_dt-digital-twin:latest >/dev/null 2>&1; then
            echo "push-platform-images: showcase requires local renode_dt-digital-twin:latest" >&2
            echo "push-platform-images: build/select digital-twin first" >&2
            exit 1
        fi
        docker build --network host \
            --build-arg PARTICIPANT_IMAGE=renode_dt-digital-twin:latest \
            -f docker/Dockerfile.showcase \
            -t renode_dt-digital-twin-showcase:latest .
    fi
}

tag_images() {
    if has_image digital-twin; then
        docker tag renode_dt-digital-twin:latest "${PLATFORM_REGISTRY}/${PLATFORM_NAMESPACE}/digital-twin:${SCENARIO_TAG}"
        docker tag renode_dt-digital-twin:latest "${LOCAL_REGISTRY}/${PLATFORM_NAMESPACE}/digital-twin:${SCENARIO_TAG}"
    fi
    if has_image showcase; then
        docker tag renode_dt-digital-twin-showcase:latest "${PLATFORM_REGISTRY}/${PLATFORM_NAMESPACE}/digital-twin:${SCENARIO_TAG}-showcase"
        docker tag renode_dt-digital-twin-showcase:latest "${LOCAL_REGISTRY}/${PLATFORM_NAMESPACE}/digital-twin:${SCENARIO_TAG}-showcase"
    fi
    if has_image ide; then
        docker tag renode_dt-ide:latest "${PLATFORM_REGISTRY}/${PLATFORM_NAMESPACE}/ide:latest"
        docker tag renode_dt-ide:latest "${LOCAL_REGISTRY}/${PLATFORM_NAMESPACE}/ide:latest"
    fi
}

use_tunnel_auth() {
    local source_config="${DOCKER_CONFIG:-$HOME/.docker}/config.json"

    if [ ! -f "$source_config" ]; then
        echo "push-platform-images: missing Docker auth config: ${source_config}" >&2
        echo "push-platform-images: run docker login ${PLATFORM_REGISTRY} first" >&2
        exit 1
    fi

    TEMP_DOCKER_CONFIG="$(mktemp -d)"
    python - "$source_config" "${TEMP_DOCKER_CONFIG}/config.json" "$PLATFORM_REGISTRY" "$LOCAL_REGISTRY" <<'PY'
import json
import sys

source_path, dest_path, platform_registry, local_registry = sys.argv[1:]

with open(source_path, "r", encoding="utf-8") as f:
    config = json.load(f)

auths = config.setdefault("auths", {})
source_auth = auths.get(platform_registry) or auths.get(local_registry)

if not source_auth:
    raise SystemExit(f"no Docker auth found for {platform_registry} or {local_registry}")

auths[local_registry] = source_auth

with open(dest_path, "w", encoding="utf-8") as f:
    json.dump(config, f)
PY
    export DOCKER_CONFIG="$TEMP_DOCKER_CONFIG"
}

push_images() {
    use_tunnel_auth
    has_image digital-twin && docker push "${LOCAL_REGISTRY}/${PLATFORM_NAMESPACE}/digital-twin:${SCENARIO_TAG}"
    has_image showcase     && docker push "${LOCAL_REGISTRY}/${PLATFORM_NAMESPACE}/digital-twin:${SCENARIO_TAG}-showcase"
    has_image ide           && docker push "${LOCAL_REGISTRY}/${PLATFORM_NAMESPACE}/ide:latest"
}

login_registry() {
    docker login "$LOCAL_REGISTRY"
}

hold_tunnel() {
    echo "push-platform-images: tunnel is ready; press Ctrl-C to close it"
    while :; do
        sleep 3600
    done
}

rsync_save_images() {
    if has_image digital-twin; then
        echo "push-platform-images: saving digital-twin (may take a few minutes)..."
        docker save renode_dt-digital-twin:latest | gzip > /tmp/push-platform-digital-twin.tar.gz
    fi
    if has_image showcase; then
        echo "push-platform-images: saving showcase image (may take a few minutes)..."
        docker save renode_dt-digital-twin-showcase:latest | gzip > /tmp/push-platform-showcase.tar.gz
    fi
    if has_image ide; then
        echo "push-platform-images: saving ide..."
        docker save renode_dt-ide:latest | gzip > /tmp/push-platform-ide.tar.gz
    fi
}

rsync_save_creds() {
    local source_config="${DOCKER_CONFIG:-$HOME/.docker}/config.json"

    if [ ! -f "$source_config" ]; then
        echo "push-platform-images: missing Docker auth config: ${source_config}" >&2
        echo "push-platform-images: run docker login ${PLATFORM_REGISTRY} first" >&2
        exit 1
    fi

    python - "$source_config" /tmp/push-platform-creds.json "$PLATFORM_REGISTRY" "$REMOTE_REGISTRY" <<'PY'
import json, sys
src, dest, plat, remote = sys.argv[1:]
cfg = json.load(open(src))
auth = cfg.get("auths", {}).get(plat) or cfg.get("auths", {}).get(remote)
if not auth:
    raise SystemExit(f"no Docker auth found for {plat}")
with open(dest, "w") as f:
    json.dump({"auths": {remote: auth}}, f)
PY
}

rsync_transfer() {
    local files=(/tmp/push-platform-creds.json)
    has_image digital-twin && files+=(/tmp/push-platform-digital-twin.tar.gz)
    has_image showcase && files+=(/tmp/push-platform-showcase.tar.gz)
    has_image ide           && files+=(/tmp/push-platform-ide.tar.gz)

    echo "push-platform-images: rsyncing to ${SSH_HOST}..."
    rsync -avz --partial --progress "${files[@]}" "${SSH_HOST}:/tmp/"
}

rsync_remote_push() {
    ssh "$SSH_HOST" bash -s "$REMOTE_REGISTRY" "$PLATFORM_NAMESPACE" "$SCENARIO_TAG" "${IMAGES[@]}" <<'REMOTE'
set -euo pipefail
REMOTE_REGISTRY="$1" NS="$2" TAG="$3"
shift 3
IMAGES=("$@")
has_image() { local i; for i in "${IMAGES[@]}"; do [ "$i" = "$1" ] && return 0; done; return 1; }

DOCKER_CONFIG="$(mktemp -d)"
cp /tmp/push-platform-creds.json "${DOCKER_CONFIG}/config.json"
export DOCKER_CONFIG

has_image digital-twin && docker load < /tmp/push-platform-digital-twin.tar.gz
has_image showcase     && docker load < /tmp/push-platform-showcase.tar.gz
has_image ide           && docker load < /tmp/push-platform-ide.tar.gz

if has_image digital-twin; then
    docker tag renode_dt-digital-twin:latest "${REMOTE_REGISTRY}/${NS}/digital-twin:${TAG}"
    docker push --quiet "${REMOTE_REGISTRY}/${NS}/digital-twin:${TAG}"
    docker rmi "${REMOTE_REGISTRY}/${NS}/digital-twin:${TAG}" 2>/dev/null || true
fi
if has_image showcase; then
    docker tag renode_dt-digital-twin-showcase:latest "${REMOTE_REGISTRY}/${NS}/digital-twin:${TAG}-showcase"
    docker push --quiet "${REMOTE_REGISTRY}/${NS}/digital-twin:${TAG}-showcase"
    docker rmi "${REMOTE_REGISTRY}/${NS}/digital-twin:${TAG}-showcase" 2>/dev/null || true
fi
if has_image ide; then
    docker tag renode_dt-ide:latest "${REMOTE_REGISTRY}/${NS}/ide:latest"
    docker push --quiet "${REMOTE_REGISTRY}/${NS}/ide:latest"
    docker rmi "${REMOTE_REGISTRY}/${NS}/ide:latest" 2>/dev/null || true
fi

rm -rf "$DOCKER_CONFIG" /tmp/push-platform-*.tar.gz /tmp/push-platform-creds.json
REMOTE
}

rsync_push_flow() {
    trap cleanup EXIT INT TERM
    build_images
    rsync_save_images
    rsync_save_creds
    rsync_transfer
    rsync_remote_push
}

SCENARIO_TAG="$(detect_scenario_tag)"

cat <<EOF
push-platform-images:
  mode:              ${MODE}
  images:            ${IMAGES[*]}
  scenario tag:      ${SCENARIO_TAG}
  public registry:   ${PLATFORM_REGISTRY}/${PLATFORM_NAMESPACE}
  push endpoint:     ${LOCAL_REGISTRY}
  tunnel target:     ${SSH_HOST} / CT ${REGISTRY_LXC_ID} -> ${REMOTE_REGISTRY}
  docker host:       ${DOCKER_HOST:-default context}
EOF

check_remote_registry
echo "push-platform-images: remote registry is reachable from ${SSH_HOST}"

case "$MODE" in
    login)
        start_tunnel
        login_registry
        ;;
    tunnel)
        start_tunnel
        hold_tunnel
        ;;
    push)
        start_tunnel
        build_images
        tag_images
        push_images
        ;;
    rsync-push)
        rsync_push_flow
        ;;
esac

echo "push-platform-images: done"
