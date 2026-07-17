# GhostTag hackathon runbook

This runbook targets the live `eg106-platform` deployment on the Kubernetes
cluster reached through `cloud_s1`-`cloud_s3`. It separates participant rollout
from the optional 252-board showcase so one can be rolled back without the
other.

## 1. Preflight

The local machine's `cloud_s*` SSH aliases use `cloudflared`, which was absent
during development. The verified no-install workaround runs the proxy on
`server`, where `/usr/local/bin/cloudflared` exists:

```bash
ssh -o 'ProxyCommand=ssh server /usr/local/bin/cloudflared access ssh --hostname %h' \
  cloud_s1 'kubectl get nodes -o wide'
```

On `cloud_s1`, check the control path before any rollout:

```bash
kubectl get nodes
kubectl get pods -n challenge-platform -o wide
kubectl get secret oci-registry -n challenge-platform
kubectl get cm active-scenario -n challenge-platform -o yaml
kubectl get jobs -A
```

Stop if the API server, registry secret, challenge frontend, or both workers
are unhealthy. Both workers were `Ready` in the 2026-07-17 acceptance run; the
manifests can queue work with one missing worker, but capacity is reduced.

## 2. Build and validate the image

The scenario tag is the branch name with `/` replaced by `-`:

```text
nrf52840-swarm/ghosttag-apocalypse
-> nrf52840-swarm-ghosttag-apocalypse
```

Build both images from the exact commit. The participant image does not contain
the reference source. The organizer-only showcase image replaces only the
firmware seed:

```bash
VCS_REF="$(git rev-parse HEAD)"
podman build -f docker/Dockerfile \
  --build-arg VCS_REF="$VCS_REF" \
  -t renode_dt-digital-twin:nrf52840-swarm-ghosttag-apocalypse .

podman build -f docker/Dockerfile.showcase \
  --build-arg PARTICIPANT_IMAGE=renode_dt-digital-twin:nrf52840-swarm-ghosttag-apocalypse \
  --build-arg VCS_REF="$VCS_REF" \
  -t renode_dt-digital-twin-showcase:nrf52840-swarm-ghosttag-apocalypse .
```

Run the mutation matrix, then the complete reference smoke test:

```bash
pytest -q tests/test_protocol_contract.py

mkdir -p output
podman run --rm \
  --network none --cpus=4 --memory=4g \
  -e TAG_COUNT=6 -e SIMULATION_SECONDS=6 \
  -v "$PWD/reference/firmware:/workspace/firmware:ro,Z" \
  -v "$PWD/output:/workspace/output:Z" \
  renode_dt-digital-twin:nrf52840-swarm-ghosttag-apocalypse
```

The pytest matrix requires the reference to pass and the starter plus all
invalid protocol mutations to fail. Also run the participant image with the
starter and require a non-zero exit. Do not push unless the reference run ends
with `GHOSTTAG FLEET SURVIVED THE APOCALYPSE`.

Smoke-test the organizer-only seed without a firmware mount:

```bash
podman run --rm \
  -e TAG_COUNT=6 -e SIMULATION_SECONDS=6 \
  -v "$PWD/output:/workspace/output:Z" \
  renode_dt-digital-twin-showcase:nrf52840-swarm-ghosttag-apocalypse
```

The validated runtime image is approximately 3.76 GB, so confirm registry and
node image-storage headroom before the event.

`push-platform-images.sh` detects the live scenario before repository defaults,
supports Podman/Skopeo, and pushes through the registry tunnel:

```bash
PUSH_ENGINE=podman ./push-platform-images.sh push digital-twin showcase
```

After push, verify both manifests through the registry API or with pulls from a
cluster node. Do not treat local tags as proof that the registry has them.

## 3. Server-side dry run

Copy or pull this branch onto `cloud_s1`, then validate both manifests against
the live v1.30 API without persisting them:

```bash
kubectl apply --dry-run=server -f k8s/platform/active-scenario.yaml
kubectl apply --dry-run=server -f k8s/showcase/indexed-job.yaml
```

## 4. Activate participant challenge

Capture rollback state first:

```bash
kubectl get cm active-scenario -n challenge-platform -o yaml \
  > /tmp/active-scenario.before-ghosttag.yaml
kubectl apply -f k8s/platform/active-scenario.yaml
kubectl get cm active-scenario -n challenge-platform -o yaml
```

Log in with one disposable participant. Confirm that a fresh scenario archive
is created, `firmware/ghost_protocol.c`, the problem statement, and the public
`renode/` assets are seeded, Run produces a queued Job, and
`output/report.html` loads in the panel. The Explorer roots must be exactly
`firmware/`, `problem/`, and `renode/`; the participant surface must not contain
the reference source or the private seed-provisioning helper.

The starter must fail cleanly. A disposable reference mount must pass. This
proves both negative and positive challenge paths.

Remove all disposable logins and namespaces after the test. Recreate the event
user batch only when the organizers are ready to open access.

## 5. Launch the showcase

Do this only after checking available requested resources:

```bash
kubectl describe node k8s-worker1 | sed -n '/Allocated resources:/,/Events:/p'
kubectl describe node k8s-worker2 | sed -n '/Allocated resources:/,/Events:/p'
kubectl apply -f k8s/showcase/indexed-job.yaml
kubectl get pods -n challenge-platform \
  -l app.kubernetes.io/name=ghosttag-apocalypse -o wide -w
```

Each indexed completion is an independent sector. The Job runs four sectors at
once, observes eight seconds of virtual time, and eventually completes all 12.
The committed `backoffLimit: 0` makes any failed sector fail acceptance instead
of hiding it behind a retry. Each pod also has a 30-minute hard deadline, so a
stalled emulator becomes an explicit failure. Inspect a sector and the
aggregate state:

```bash
kubectl logs -n challenge-platform \
  -l app.kubernetes.io/name=ghosttag-apocalypse \
  --all-containers=true --prefix=true --max-log-requests=12
kubectl get job ghosttag-apocalypse -n challenge-platform -o yaml
```

Success requires 12 successful indexes. Pod logs must each contain
`GHOST_VALIDATION passed=1`.

## 6. Stop or rollback

Deleting the showcase does not affect participant IDEs:

```bash
kubectl delete job ghosttag-apocalypse -n challenge-platform
```

Restore the captured active scenario if participant rollout fails:

```bash
kubectl apply -f /tmp/active-scenario.before-ghosttag.yaml
kubectl get cm active-scenario -n challenge-platform -o yaml
```

Do not delete participant PVCs during rollback. The platform archives scenario
files and those PVCs are the only durable participant work surface.
