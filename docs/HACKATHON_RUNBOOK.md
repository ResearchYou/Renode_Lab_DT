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

Stop if the API server, registry secret, challenge frontend, or only schedulable
worker is unhealthy. `cloud_s2` / `k8s-worker1` was `NotReady` on 2026-07-12;
the manifests tolerate one missing worker, but not zero.

## 2. Build and validate the image

The scenario tag is the branch name with `/` replaced by `-`:

```text
nrf52840-swarm/ghosttag-apocalypse
-> nrf52840-swarm-ghosttag-apocalypse
```

Build and smoke-test with six tags before pushing:

```bash
podman build -f docker/Dockerfile \
  -t renode_dt-digital-twin:nrf52840-swarm-ghosttag-apocalypse .

mkdir -p output
podman run --rm \
  -e TAG_COUNT=6 -e SIMULATION_SECONDS=6 \
  -v "$PWD/reference/firmware:/workspace/firmware:ro,Z" \
  -v "$PWD/output:/workspace/output:Z" \
  renode_dt-digital-twin:nrf52840-swarm-ghosttag-apocalypse
```

Do not push unless the run ends with `GHOSTTAG FLEET SURVIVED THE APOCALYPSE`.
The validated runtime image is approximately 3.76 GB, so confirm registry and
node image-storage headroom before the event.

The repository's existing `push-platform-images.sh` still uses the Docker
daemon and registry tunnel. If Docker is running, use the explicit tag so the
currently active old scenario does not choose the wrong image name:

```bash
SCENARIO_TAG=nrf52840-swarm-ghosttag-apocalypse \
  ./push-platform-images.sh push digital-twin
```

After push, verify the manifest through the registry API or with a pull from a
cluster node. Do not treat a local tag as proof that the registry has it.

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
is created, `firmware/ghost_protocol.c` and the problem statement are seeded,
Run produces a queued Job, and `output/report.html` loads in the panel.

The starter must fail cleanly. A disposable reference mount must pass. This
proves both negative and positive challenge paths.

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
once and eventually completes all 12. Inspect a sector and the aggregate state:

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
