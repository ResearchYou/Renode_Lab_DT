# Participant Exercise Guide

The complete task is rendered in the browser from `ide/problem/problem.md`.
This file is the short command-line companion.

## Work surface

- `firmware/ghost_protocol.c`: the only required edits.
- `firmware/include/ghost_protocol.h`: wire layout and API contract.
- `firmware/tests/test_protocol.c`: public known-answer and tamper tests.
- `firmware/main.c`: Zephyr BLE broadcaster that calls the protocol.
- `output/report.html`: generated swarm evidence.

The trusted gateway is intentionally outside the participant seed under
`support/gateway/`.

## Feedback layers

The Run button executes three increasingly expensive gates:

1. native C17 tests for the SipHash vectors and payload invariants;
2. Zephyr builds for the Nordic nRF52840 DK target;
3. a multi-machine Renode BLE simulation and cross-gateway evidence audit.

The starter compiles and broadcasts packets but fails authentication. A compile
error stops before Renode; an algorithm error reaches the simulation and is
reported as missing/rogue fleet traffic.

## Constraints

- Keep `GHOST_PAYLOAD_SIZE` at 28 bytes; a legacy BLE advertising data element
  has only enough room for this payload plus its type/length overhead.
- Keep all wire integers little-endian.
- Use the MAC-domain key transform in the TODO comment/reference design; reusing
  the raw EID key is rejected by the expected packet vectors.
- Do not reveal `device_seed` in any form intended as a stable identifier.
- Do not weaken verification to make the swarm accept clones.
