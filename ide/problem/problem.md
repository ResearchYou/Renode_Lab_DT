# GhostTag Apocalypse: build AirTag after the Internet

The phones are dead, GPS is jammed, and the cloud is gone. Battery-powered
tags must remain findable through sparse BLE rescue gateways without exposing a
permanent radio identity. Power is unstable, attackers replay captured packets,
and every flash operation consumes scarce energy.

This is an 8-hour firmware hackathon. Edit only
`firmware/ghost_protocol.c`; the application, header, gateway, Renode scenario,
and validator are fixed contracts.

## Expected schedule

| Time | Target |
|---|---|
| 0:00-1:00 | read the contract and make SipHash pass |
| 1:00-2:30 | implement the ratchet, EID, MAC, and verification |
| 2:30-5:00 | implement the two-page persistent journal |
| 5:00-6:00 | handle torn writes, corruption, and reboot recovery |
| 6:00-7:00 | meet flash-wear and energy limits |
| 7:00-8:00 | diagnose the full Zephyr and Renode swarm |

## Simulation

The normal Run boots 6 authorized nRF52840 tags, 3 observer gateways, one
untrusted clone, and one replay attacker. At virtual second 3, Renode resets
tag 1 without clearing its nonvolatile journal. The replay attacker then emits
a captured, correctly authenticated epoch-0 packet from a different BLE
address.

The gateway knows the authorized fleet seeds. It must see and rotate every tag,
reject the untrusted clone, reject the replay, and observe tag 1 resume without
reusing an epoch.

## Fixed 28-byte packet

| Bytes | Meaning |
|---|---|
| 0..1 | company ID `0xF00D`, little-endian |
| 2 | protocol version `3` |
| 3 | flags |
| 4..7 | persistent ratchet epoch, little-endian |
| 8..11 | public city sector, little-endian |
| 12..19 | keyed ephemeral ID |
| 20..27 | keyed MAC over bytes `0..19` |

No stable tag ID may appear in the packet.

## Cryptographic contract

The initial 16-byte key is already defined by `seed_to_key`. To advance from
the current key to `next_epoch`, derive two SipHash outputs with this 6-byte
input:

```text
0x52 || next_epoch_le32 || lane
```

Use lane `0` for key bytes `0..7` and lane `1` for bytes `8..15`. Replace the
old key only after both outputs are computed.

For a packet at epoch `e`:

- derive the epoch key by ratcheting from epoch 0 through `e`;
- derive the EID with SipHash over
  `0x45 || epoch_le32 || sector_le32`;
- derive a separate MAC key by XORing epoch-key bytes 0, 7, 8, and 15 with
  `0x4d`, `0x41`, `0x43`, and `0xa7`;
- authenticate packet bytes `0..19` with that MAC key;
- compare expected EID and MAC without early exit;
- reject malformed headers and epochs above `1,000,000`.

## Persistent journal contract

The state uses two 4096-byte pages. Flash begins erased (`0xff`), permits only
1-to-0 writes, and is erased one complete page at a time. Records are appended
in 40-byte slots:

| Bytes | Meaning |
|---|---|
| 0..3 | magic `0x47535452` |
| 4..7 | generation |
| 8..11 | future resume epoch |
| 12..15 | cumulative erase count |
| 16..31 | ratcheted key for the resume epoch |
| 32..35 | IEEE CRC32 over bytes `0..31` |
| 36..39 | commit `0xC01117ED` |

Write bytes `0..35` first and the commit word in a separate final write. A
missing commit, bad CRC, bad magic, or half-written body is invalid.

Reserve leases of 16 epochs. Before emitting from a fresh state, persist the
key and epoch immediately after the lease. On clean recovery, resume from that
future epoch and reserve another lease before emitting. If a non-erased invalid
record exists after the newest valid record, conservatively advance two lease
widths before reserving again. This prevents reuse even when a torn or
corrupted record hid its generation and resume epoch.

Append after the newest record. When its page is full, erase the other page and
continue at its first slot. Never erase the page containing the newest valid
record before its successor is durable.

## Your six TODOs

1. `ghost_siphash24`: canonical SipHash-2-4, including partial final blocks.
2. `ratchet_step`: the two-lane persistent key ratchet.
3. `build_with_epoch_key`: v3 header, EID, domain-separated MAC.
4. `ghost_verify_payload`: strict, constant-time EID/MAC verification.
5. `ghost_state_boot`: scan, validate, recover, and reserve the journal.
6. `ghost_state_next_payload`: reserve before exhaustion, emit once, advance
   RAM once.

## Tests and resource limits

The native suite checks all 64 SipHash vectors, an exact ratchet vector, an
exact 28-byte packet, every one-byte tamper, torn body writes, torn commits,
CRC corruption, reboot recovery, and 2000-epoch endurance.

The runtime energy formula is fixed:

```text
units = flash_writes * 8 + page_erases * 40 + advertisements
```

One journal record costs two writes. The reboot scenario must stay at or below
80 units. Across 2000 sequential epochs, the reference limit is at most 252
writes and exactly one page erase.

Passing native tests is necessary but not sufficient. The final output must
include:

```text
GHOST_VALIDATION passed=1 ... replays=... recovered=1 energy=.../80
>>> GHOSTTAG FLEET SURVIVED THE APOCALYPSE <<<
```

Refresh Results after the run to inspect the generated HTML report.
