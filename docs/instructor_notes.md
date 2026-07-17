# Instructor Notes: GhostTag Apocalypse

## Three-hour format

- 0:00-0:20: premise, privacy threat model, 28-byte BLE budget.
- 0:20-0:45: Renode machines, BLEMedium, positions, deterministic time.
- 0:45-1:05: Zephyr broadcaster/observer split on nRF52840.
- 1:05-1:25: SipHash round function and domain separation.
- 1:25-2:20: implementation in teams.
- 2:20-2:50: launch the indexed 252-board cluster showcase while teams finish.
- 2:50-3:00: compare fleet coverage, rotation, rogue rejection, and runtime cost.

## Demo order

1. Run the starter with six tags. Show that immutable contract tests reject it
   before the expensive firmware build and still produce the HTML report.
2. Show `generate_swarm_resc.py`, especially the nRF52840 machine creation,
   position assignment, unique FICR BLE address, and per-device flash seed.
3. Copy the reference protocol implementation into a disposable firmware mount
   and show valid tag recovery without a stable radio ID.
4. Run the six-tag IDE-scale test through the browser Run API.
5. Run `k8s/showcase/indexed-job.yaml` only after the runbook preflight is green.

## Guardrails

The showcase requests 1.8 CPU / 4096 MiB and limits each sector to 3 CPU /
5120 MiB, with parallelism four. Both 12-core/15 GiB workers were Ready in the
2026-07-17 acceptance run, with two sectors placed on each. Do not raise
parallelism or shorten the eight-second observation window without repeating
the complete 12-sector capacity test.

The design is a hackathon digital twin, not a production tracking protocol.
SipHash is used as a short-input PRF/MAC and the simulated fleet seeds are
deterministic. Production devices require secure provisioning, protected key
storage, replay policy, radio certification, and a complete privacy review.
