# Instructor notes: GhostTag Apocalypse

## Eight-hour format

- 0:00-0:30: premise, privacy threat model, fixed 28-byte BLE budget.
- 0:30-1:15: SipHash-2-4 and domain separation.
- 1:15-2:15: per-epoch key ratchet, EID, MAC, and replay threat.
- 2:15-3:00: flash semantics, CRC, and commit-last records.
- 3:00-5:00: team implementation of the two-page journal and leases.
- 5:00-5:30: forced Renode reset and torn-write failure clinic.
- 5:30-6:30: endurance and energy optimization.
- 6:30-7:30: full Zephyr/Renode runs and report diagnosis.
- 7:30-8:00: final passes, replay/recovery evidence, showcase.

## Demo order

1. Run the untouched skeleton and show that gate 1 fails before Renode.
2. Explain the public Renode script: six tags, three gateways, clone, reset,
   and late replay attacker.
3. Show the 40-byte journal layout and why commit is a separate final write.
4. Run the organizer reference through the same participant image.
5. Read the final evidence: 6/6 seen, 6/6 rotated, rogue and replay rejected,
   recovered epoch unique, energy at or below 80.

## Guardrails

- Never give participants `reference/firmware/` or the private seed helper.
- Help with C, logs, and platform use; do not implement TODOs for teams.
- Do not accept a native-only pass.
- Do not shorten the eight-second observation window.
- Keep participant workspaces isolated and do not inspect their solution unless
  support or grading requires it.

This is a hackathon digital twin, not a production tracking protocol.
Production devices still need secure provisioning, protected key storage,
hardware-specific flash drivers, radio certification, and a privacy review.
