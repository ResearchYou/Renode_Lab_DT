# Architecture and threat model

## Product idea

GhostTag is “AirTag 2 after civilization loses the cloud”: an offline fleet of
tiny BLE broadcasters found by sparse rescue gateways. It is intentionally
wild in scale and deep enough for an eight-hour firmware hackathon.

The improvement target is not radio range alone. It is verifiable privacy:

- no stable device ID in advertisements;
- a keyed ID changes every two seconds of virtual time;
- the packet is authenticated before a gateway accepts a sighting;
- unauthorized devices can look syntactically correct but remain untrusted;
- captured authorized packets are rejected when replayed from another address;
- a persistent key ratchet survives a forced reset without epoch reuse;
- flash wear and advertising cost remain inside a fixed energy budget;
- one deterministic simulation can prove behavior across many full SoCs.

## Trust boundaries

Participant-controlled:

- tag protocol implementation and tag ELF;
- every advertisement emitted by authorized and rogue tag machines.

Harness-controlled:

- provisioned per-tag seeds and expected fleet range;
- gateway firmware and key search;
- Renode topology, power cut, replay injection, simulation time, and UART capture;
- pass/fail validator and Kubernetes resource envelope.

The gateway never receives the stable tag number. It enumerates the authorized
sector fleet, derives each candidate seed, and validates the EID/MAC. At 16 tags
this is intentionally computationally wasteful but very visible and easy to
reason about during a hackathon.

## Protocol-v3 state machine

Each tag starts from a seed-derived root key and ratchets once per epoch. Before
emitting, it persists the key and epoch at the far end of a 16-epoch lease. A
reboot resumes there and durably reserves the next lease, so RAM state lost in
the power cut cannot cause an on-air epoch to repeat.

The journal has two 4096-byte pages and 40-byte append-only records. Each record
contains generation, future epoch, future key, erase count, and CRC32. The
commit word is written separately and last. Torn or corrupt records are ignored;
an uncertain newest slot causes an extra two-lease skip before the next commit.
Page rollover erases only the page that does not contain the newest valid
record.

The trusted gateway binds the first accepted packet for a tag to its BLE source
address and latest epoch. A lower epoch or a valid packet from another address
is counted as replay. This address binding is a simulation acceptance policy,
not a complete production anti-relay protocol.

## Live infrastructure evidence

Live acceptance on 2026-07-17 found Kubernetes v1.30.14 with:

- `cloud_s1` / `k8s-master`: Ready control plane, unschedulable to ordinary
  workloads because of its control-plane taint;
- `cloud_s2` / `k8s-worker1`: 12 CPU, 15 GiB, Ready;
- `cloud_s3` / `k8s-worker2`: 12 CPU, 15 GiB, Ready;
- local-path storage, nginx Ingress, MetalLB, and the existing
  `challenge-platform` plus per-user namespaces;
- a queue-based runtime Job path with 4 CPU / 4096 MiB requests and limits per
  participant run.

The showcase uses soft topology spread (`ScheduleAnyway`) so one unavailable
worker does not deadlock the event. Resource requests, not optimistic live
usage, control placement across both healthy workers.

## Empirical capacity boundary

A 32-tag, three-gateway, two-rogue sector was exercised locally under a
3 CPU / 3 GiB limit. The protocol worked and gateways observed
rotated authorized identities, but Renode reached 99.97% of the memory limit
and progressed too slowly for a live event. The committed showcase therefore
uses 16 tags per sector: 21 nRF52840 machines per pod, 84 concurrently, and 252
across all indexed completions. This is a measured safety correction, not a
paper estimate.

Each sector requests 1.8 CPU / 4096 MiB and is limited to 3 CPU / 5120 MiB.
With parallelism four, topology spread placed two sectors on each 15 GiB
worker. The runtime caches the nRF52840 SVD locally, so isolated Jobs do not
wait for network downloads or accumulate timeout output in Renode. The final
showcase observes eight virtual seconds and uses no retry, so every indexed
sector must satisfy coverage and rotation on its first execution. The measured
seed-11 RF schedule caused pathological host-time growth, so the 12th sector
reuses the validated seed-0 radio schedule while retaining its own sector ID,
device IDs, addresses and cryptographic seeds.

The final live acceptance completed all indexes `0-11` in about 24 minutes.
All 12 pods used the same showcase digest, exited 0 on their first attempt, and
reported 16/16 tags seen and rotated; the aggregate Job had zero failures and
zero restarts.
