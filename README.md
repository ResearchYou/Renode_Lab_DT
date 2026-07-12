# GhostTag Apocalypse

GhostTag Apocalypse is a hackathon environment for building the lost-device
network that still works after cellular service, GPS, and the cloud are gone.
It turns the `cloud_s1`-`cloud_s3` Kubernetes cluster into a factory for
hundreds of deterministic nRF52840 digital twins.

Each simulated city sector contains:

- 16 Nordic nRF52840 tags running participant-built Zephyr firmware;
- three nRF52840 observer gateways on a position-aware Renode BLE medium;
- two unregistered clone devices that must be rejected;
- rotating, authenticated 28-byte advertisements with no stable device ID;
- repeatable RF range, placement, timing, and adversarial traffic.

The full indexed Kubernetes showcase is 12 sectors: **192 authorized tags, 36
gateways, and 24 attackers — 252 emulated boards**. Four sectors run at once so
the current 12-core worker can survive the demo; Kubernetes spreads them when
the second worker returns.

## Participant challenge

Only `firmware/ghost_protocol.c` contains the three required TODOs:

1. implement SipHash-2-4 against the authors' known-answer vectors;
2. build a rotating ephemeral identity and domain-separated authentication tag;
3. verify the packet without accepting tampering or an untrusted fleet key.

The same code is first attacked by native known-answer/tamper tests, then built
as real Zephyr nRF52840 firmware and booted across the Renode swarm. The
observer firmware is immutable and tries every authorized fleet seed; it never
receives a stable ID over BLE.

The detailed task is in [ide/problem/problem.md](ide/problem/problem.md). The
completed implementation is under `reference/firmware/`.

## Local smoke run

The runtime image includes the pinned Zephyr workspace, Arm-only SDK, and
Renode; the validated slim build is 3.76 GB. On this host, Podman is available
even when the Docker daemon is stopped:

```bash
podman build -f docker/Dockerfile -t renode_dt-digital-twin:ghosttag .
mkdir -p output
podman run --rm \
  -e TAG_COUNT=6 -e SIMULATION_SECONDS=6 \
  -v "$PWD/firmware:/workspace/firmware:ro,Z" \
  -v "$PWD/output:/workspace/output:Z" \
  renode_dt-digital-twin:ghosttag
```

Open `output/report.html` after the run. The starter is expected to fail; the
reference validation command is in [reference/README.md](reference/README.md).

## Existing hackathon platform

This branch is directly compatible with the live `eg106-platform` contract:

- scenario image tag: `nrf52840-swarm-ghosttag-apocalypse`;
- firmware seed: `/workspace/firmware_seed`;
- problem seed: `/workspace/problem_seed`;
- runtime entrypoint: `/workspace/scripts/entrypoint.sh`;
- report: `/workspace/output/report.html`.

Use [docs/HACKATHON_RUNBOOK.md](docs/HACKATHON_RUNBOOK.md) for the image push,
active-scenario switch, per-user test, showcase launch, rollback, and degraded
node behavior. Do not apply the cluster manifests blindly: the runbook starts
with preflight checks because `cloud_s2` was `NotReady` during development.

## Why these exact boards and tools

The scenario uses Renode's built-in nRF52840 platform and BLE medium, both
documented and demonstrated by Renode's official two-node Zephyr BLE example.
Zephyr 4.4.1 and Renode 1.16.1 are pinned rather than tracking moving branches.
Every nontrivial external choice and primary source is recorded in
[docs/ONLINE_SOURCES.md](docs/ONLINE_SOURCES.md).
