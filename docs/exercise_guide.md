# Participant exercise guide

The complete task is rendered from `ide/problem/problem.md`. This file is the
short command-line companion.

## Work surface

- edit only `firmware/ghost_protocol.c`;
- read `firmware/include/ghost_protocol.h` for the fixed API and constants;
- use `firmware/tests/test_protocol.c` for fast public feedback;
- inspect `renode/` to understand the reset and attacker sequence;
- open `output/report.html` after a complete run.

## Recommended order

1. make all SipHash vectors pass;
2. implement the ratchet and verify the epoch-16 known answer;
3. build and verify protocol-v3 packets;
4. implement record encoding, CRC32, scan, and commit-last append;
5. reserve 16-epoch leases on fresh boot and recovery;
6. handle torn body, torn commit, and corrupted newest-record cases;
7. check the 2000-epoch wear bound;
8. run Zephyr and Renode only after the native contract passes.

## Feedback layers

Run executes three gates:

1. native C17 protocol, persistence, corruption, and endurance tests;
2. Zephyr builds for the nRF52840 tag and trusted gateway;
3. an 8-second Renode swarm with a power cut, clone, replay, and energy audit.

The skeleton must fail at gate 1. A correct reference must pass all three.

## Fixed limits

- 28-byte packet and protocol version 3;
- two 4096-byte journal pages;
- 40-byte records, CRC32, final commit word;
- 16 epochs per durable lease;
- at most 80 runtime energy units;
- at most 252 flash writes and one erase over 2000 epochs.
