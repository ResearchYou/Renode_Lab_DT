# GhostTag Apocalypse: build AirTag for the end of the Internet

The phones are dead. GPS is jammed. The cloud region is now a crater. There are
still hundreds of battery-powered tags and a handful of BLE rescue gateways.
Your job is to make the tags findable without broadcasting a permanent identity
that turns every survivor into a tracking target.

## The swarm you are coding for

One test run boots a complete Renode city sector:

```text
participant nRF52840 tags -- BLE advertisements --> observer nRF52840 gateways
            |                                          |
            +-- rotating private IDs                   +-- authorized fleet search
            +-- authenticated packets                  +-- clone rejection
            +-- no stable ID on air                    +-- multi-gateway coverage
```

The normal IDE run uses 6 tags, three gateways, and two rogue clones. The
cluster spectacle runs 12 indexed sectors containing 252 emulated nRF52840
boards in total.

## Packet contract

Your 28-byte manufacturer payload is fixed:

| Bytes | Meaning |
|---|---|
| 0..1 | GhostTag company ID `0xF00D` |
| 2 | protocol version `2` |
| 3 | flags |
| 4..7 | rotation epoch, little-endian |
| 8..11 | public city sector, little-endian |
| 12..19 | keyed ephemeral ID |
| 20..27 | keyed authentication tag over bytes 0..19 |

There is deliberately no stable tag ID in the packet. A trusted gateway tests
the small authorized fleet keyspace to recover which tag sent a valid sighting.
That is expensive in exactly the fun way: the cluster spends CPU so the radio
packet can remain private and tiny.

## Your three TODOs

Edit only `firmware/ghost_protocol.c`:

1. `ghost_siphash24`: implement canonical SipHash-2-4 with a 128-bit key,
   64-bit output, and little-endian message words.
2. `ghost_build_payload`: derive the ephemeral ID from domain byte `0x45`, the
   epoch, and sector; derive the authentication tag with a distinct MAC key
   domain; never copy `device_seed` into the packet.
3. `ghost_verify_payload`: reject bad headers, wrong IDs, wrong authentication
   tags, tampering, and the wrong device seed.

Do not change the packet size, company ID, version, epoch duration, UART line
formats, CMake files, or gateway code. Those are the digital-twin contract.

## What attacks your implementation

- canonical SipHash known-answer vectors;
- bit flips in authenticated flags;
- a wrong fleet seed;
- stable-seed leakage scanning;
- multiple epochs, which must produce different radio IDs;
- real Zephyr builds for `nrf52840dk/nrf52840`;
- many concurrent nRF52840 machines on a range-limited BLE medium;
- unregistered devices sending correctly shaped clone traffic.

## Passing evidence

A pass ends with output similar to:

```text
[PASS] SipHash known-answer and tamper suite: exit=0
[PASS] all observer gateways boot: ready=3/3
[PASS] entire authorized fleet is discoverable: all tags seen
[PASS] every stable identity rotates on air: all tags rotated
[PASS] unregistered clone traffic is rejected: rogue_packets=...
[PASS] no firmware fatal errors: none
GHOST_VALIDATION passed=1 ...
>>> GHOSTTAG FLEET SURVIVED THE APOCALYPSE <<<
```

Refresh the Results panel to open the generated survival report.
