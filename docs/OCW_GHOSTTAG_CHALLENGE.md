# GhostTag Apocalypse

## Challenge overview

The Internet is unavailable, GPS is jammed, and only a sparse network of BLE
gateways remains operational. Your task is to complete the firmware protocol
for battery-powered rescue tags that must remain discoverable without
broadcasting a permanent identity.

The protocol must survive unstable power, reject forged and replayed packets,
limit flash wear, and remain within a strict energy budget. The final system is
tested as a multi-device nRF52840 swarm in Renode.

This is an approximately 8-hour firmware challenge. You will work only in:

```text
firmware/ghost_protocol.c
```

The public API, application code, gateway firmware, simulation scenario, and
validation rules are fixed contracts.

## Read this documentation thoroughly

Do not start coding before reading the following files in order:

1. [`problem/problem.md`](../ide/problem/problem.md) -- the complete protocol,
   journal, recovery, energy, and acceptance contract.
2. [`firmware/include/ghost_protocol.h`](../firmware/include/ghost_protocol.h)
   -- all public types, constants, callbacks, and function signatures.
3. [`firmware/ghost_protocol.c`](../firmware/ghost_protocol.c) -- the starter
   implementation and the six TODOs you must complete.
4. [`firmware/tests/test_protocol.c`](../firmware/tests/test_protocol.c) -- the
   fast public tests and the expected storage behavior.
5. [`renode/README.md`](../ide/renode/README.md) -- the files exposed for the
   simulation and the reset/attacker sequence.
6. [`renode/generate_swarm_resc.py`](../docker/scripts/generate_swarm_resc.py)
   and [`renode/run_test.sh`](../docker/scripts/run_test.sh) -- how the swarm is
   generated, executed, and passed to validation.
7. [`docs/renode_primer.md`](renode_primer.md) -- the nRF52840 machines, BLE
   medium, provisioning region, persistent journal region, and UART evidence.
8. [`docs/exercise_guide.md`](exercise_guide.md) -- the recommended solution
   order and the three feedback layers.

The values and byte layouts below are a summary. If this page and the fixed C
header differ, the header and the complete problem statement are authoritative.

## What you must implement

Complete the six TODOs in `firmware/ghost_protocol.c`:

1. canonical SipHash-2-4, including partial final message blocks;
2. the two-lane persistent key-ratchet step;
3. construction of the protocol-v3 packet, EID, and domain-separated MAC;
4. strict packet verification with constant-time EID and MAC comparison;
5. boot-time journal scan, validation, recovery, and lease reservation;
6. payload emission with reservation before lease exhaustion and exactly one
   in-memory epoch advance.

Keep the existing function signatures. Do not change the packet size, constants,
storage callbacks, or public structures to work around the contract.

## Packet contract

Every BLE manufacturer payload is exactly 28 bytes:

| Bytes | Field |
|---|---|
| `0..1` | company ID `0xF00D`, little-endian |
| `2` | protocol version `3` |
| `3` | flags |
| `4..7` | persistent ratchet epoch, little-endian |
| `8..11` | public city sector, little-endian |
| `12..19` | keyed ephemeral identifier |
| `20..27` | keyed MAC over bytes `0..19` |

A stable tag identifier must never appear on air in either byte order.

### Key ratchet

The provided `seed_to_key` function derives the epoch-0 key. To derive the key
for `next_epoch`, compute two SipHash outputs using the current 16-byte key:

```text
0x52 || next_epoch_le32 || lane
```

Use lane `0` for new key bytes `0..7` and lane `1` for bytes `8..15`. Compute
both outputs before replacing the current key.

### EID and MAC

For a packet at epoch `e`:

- ratchet from epoch 0 through `e` to obtain the epoch key;
- compute the EID with SipHash over
  `0x45 || epoch_le32 || sector_le32`;
- copy the epoch key and XOR bytes `0`, `7`, `8`, and `15` with `0x4d`, `0x41`,
  `0x43`, and `0xa7` to obtain the MAC key;
- authenticate packet bytes `0..19` with the MAC key;
- reject an incorrect company ID, version, EID, MAC, or an epoch above
  `1,000,000`;
- compare all EID and MAC bytes without returning on the first mismatch.

## Crash-safe persistent state

The tag has two 4096-byte journal pages. Erased bytes are `0xff`; a write may
only change bits from 1 to 0; an erase operates on one complete page.

Each journal slot is a 40-byte record:

| Bytes | Field |
|---|---|
| `0..3` | magic `0x47535452` |
| `4..7` | generation |
| `8..11` | future resume epoch |
| `12..15` | cumulative page-erase count |
| `16..31` | ratcheted key for the resume epoch |
| `32..35` | IEEE CRC32 over bytes `0..31` |
| `36..39` | commit word `0xC01117ED` |

Write the 36-byte body first. Write the commit word in a separate final storage
operation. A missing commit, bad CRC, bad magic, or partially written body is
not a valid record.

The journal reserves leases of 16 epochs:

- before the first advertisement, durably reserve epochs `0..15` by storing
  the resume state for epoch `16`;
- after a clean reboot, resume at the stored future epoch and reserve the next
  lease before transmitting;
- if a non-erased invalid slot follows the newest valid record, skip two full
  leases conservatively before reserving again;
- append after the newest record;
- when one page is full, erase the other page and write its first record;
- never erase the page holding the newest valid state before its successor is
  committed.

These rules must prevent epoch reuse after a torn body write, torn commit,
corrupted CRC, page rollover, or power loss at any storage operation.

## Energy and endurance limits

Runtime energy is calculated as:

```text
energy = flash_writes * 8 + page_erases * 40 + advertisements
```

One committed record requires two writes: body, then commit. The power-cut
simulation must use no more than 80 energy units.

The endurance tests generate 2000 sequential payloads. A passing solution uses
at most 252 flash writes and exactly one page erase. Persisting every packet
cannot pass these limits; lease reservation is required.

## Simulation

The complete run starts:

- 6 authorized nRF52840 tags;
- 3 trusted observer gateways;
- 1 unregistered clone;
- 1 attacker replaying a captured, correctly authenticated packet.

At virtual second 3, Renode resets tag 1 while preserving its nonvolatile
journal. The replay attacker then transmits an old epoch-0 packet from a
different BLE address.

A correct system must:

- discover all six authorized tags;
- observe identity rotation for every tag;
- reject the unregistered clone;
- reject the captured replay;
- recover tag 1 without reusing an epoch;
- remain within the runtime energy budget;
- finish without firmware fatal errors.

## Recommended 8-hour workflow

| Time | Goal |
|---|---|
| `0:00-1:00` | Read every contract and pass all SipHash vectors. |
| `1:00-2:30` | Implement ratchet, packet generation, EID, MAC, and verification. |
| `2:30-5:00` | Implement record encoding, CRC32, scanning, append, and leases. |
| `5:00-6:00` | Handle torn writes, corruption, reboot, and page rollover. |
| `6:00-7:00` | Verify flash-wear and energy bounds. |
| `7:00-8:00` | Run Zephyr and Renode; diagnose the complete swarm. |

Use the fastest feedback first. Do not repeatedly run the full swarm while the
native protocol suite still fails.

## Understanding the Run result

The Run action has three gates:

1. native C17 tests for cryptography, packet integrity, persistence, recovery,
   corruption handling, and endurance;
2. Zephyr builds for the nRF52840 tag and trusted gateway;
3. an 8-second Renode swarm followed by validation and report generation.

The complete success output includes lines equivalent to:

```text
PROTOCOL_CONTRACT_TESTS failures=0
Renode exit status: 0
GHOST_VALIDATION passed=1 ... tags=6/6 rotated=6/6 ... recovered=1 energy=.../80
>>> GHOSTTAG FLEET SURVIVED THE APOCALYPSE <<<
```

After the run, refresh the Results panel and inspect `output/report.html`. When
a check fails, read the first failing gate and its log before changing code.

## Completion checklist

Before considering the challenge complete, confirm that:

- all six TODOs have real implementations;
- all 64 SipHash vectors pass;
- the exact ratchet and 28-byte packet vectors pass;
- malformed, tampered, forged, and unreasonable packets are rejected;
- fresh boot reserves a lease before any packet is emitted;
- every recovery path avoids epoch reuse;
- torn and corrupted records remain invalid;
- 2000 payloads stay within the write and erase limits;
- both Zephyr applications build;
- the Renode validator reports all six tags seen and rotated;
- clone rejection, replay rejection, recovery, energy, and fatal-error checks
  all pass.
