# Instructor Notes: GhostTag Apocalypse

## Three-hour format

- 0:00-0:20: premise, privacy threat model, 28-byte BLE budget.
- 0:20-0:45: Renode machines, BLEMedium, positions, deterministic time.
- 0:45-1:05: Zephyr broadcaster/observer split on nRF52840.
- 1:05-1:25: SipHash round function and domain separation.
- 1:25-2:35: implementation in teams.
- 2:35-2:50: launch the indexed 252-board cluster showcase.
- 2:50-3:00: compare fleet coverage, rotation, rogue rejection, and runtime cost.

## Demo order

1. Run the starter with six tags. Show that it compiles but every shaped packet
   is treated as rogue.
2. Show `generate_swarm_resc.py`, especially the nRF52840 machine creation,
   position assignment, unique FICR BLE address, and per-device flash seed.
3. Copy the reference protocol implementation into a disposable firmware mount
   and show valid tag recovery without a stable radio ID.
4. Increase to 12 tags for the IDE-scale test.
5. Run `k8s/showcase/indexed-job.yaml` only after the runbook preflight is green.

## Guardrails

The showcase requests 1.8 CPU / 2200 MiB and limits each sector to 3 CPU /
2560 MiB, with parallelism four. This fits the single 12-core/15 GiB worker
observed on 2026-07-12 while leaving room for the challenge frontend and active
IDEs. Do not raise parallelism while `k8s-worker1` is `NotReady`.

The design is a hackathon digital twin, not a production tracking protocol.
SipHash is used as a short-input PRF/MAC and the simulated fleet seeds are
deterministic. Production devices require secure provisioning, protected key
storage, replay policy, radio certification, and a complete privacy review.
