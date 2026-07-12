# Architecture and threat model

## Product idea

GhostTag is “AirTag 2 after civilization loses the cloud”: an offline fleet of
tiny BLE broadcasters found by sparse rescue gateways. It is intentionally
wild in scale but narrow enough for a three-hour firmware hackathon.

The improvement target is not radio range alone. It is verifiable privacy:

- no stable device ID in advertisements;
- a keyed ID changes every two seconds of virtual time;
- the packet is authenticated before a gateway accepts a sighting;
- unauthorized devices can look syntactically correct but remain untrusted;
- one deterministic simulation can prove behavior across many full SoCs.

## Trust boundaries

Participant-controlled:

- tag protocol implementation and tag ELF;
- every advertisement emitted by authorized and rogue tag machines.

Harness-controlled:

- provisioned per-tag seeds and expected fleet range;
- gateway firmware and key search;
- Renode topology, simulation time, and UART capture;
- pass/fail validator and Kubernetes resource envelope.

The gateway never receives the stable tag number. It enumerates the authorized
sector fleet, derives each candidate seed, and validates the EID/MAC. At 16 tags
this is intentionally computationally wasteful but very visible and easy to
reason about during a hackathon.

## Live infrastructure evidence

Read-only inspection on 2026-07-12 found Kubernetes v1.30.14 with:

- `cloud_s1` / `k8s-master`: Ready control plane, unschedulable to ordinary
  workloads because of its control-plane taint;
- `cloud_s2` / `k8s-worker1`: 12 CPU, 15 GiB, `NotReady` since 2026-07-03;
- `cloud_s3` / `k8s-worker2`: 12 CPU, 15 GiB, Ready;
- local-path storage, nginx Ingress, MetalLB, and the existing
  `challenge-platform` plus per-user namespaces;
- an existing queue-based runtime Job path with 2 CPU / 2560 MiB limits per
  participant run.

The showcase uses soft topology spread (`ScheduleAnyway`) so a dead worker does
not deadlock the event. Resource requests, not optimistic live usage, are sized
to the currently healthy worker.

## Empirical capacity boundary

A 32-tag, three-gateway, two-rogue sector was exercised locally under a
3 CPU / 3 GiB limit. The protocol worked and gateways observed
rotated authorized identities, but Renode reached 99.97% of the memory limit
and progressed too slowly for a live event. The committed showcase therefore
uses 16 tags per sector: 21 nRF52840 machines per pod, 84 concurrently, and 252
across all indexed completions. This is a measured safety correction, not a
paper estimate.

The committed pod limit is 3 CPU / 2560 MiB, so four concurrent sectors can use
at most 10 GiB on the healthy 15 GiB worker. A full reference sector passed
under that exact cap in the final acceptance run.
