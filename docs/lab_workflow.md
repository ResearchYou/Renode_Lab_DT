# GhostTag Lab Workflow

## Build and execution flow

```text
participant C
  -> native protocol/persistence/endurance binary
  -> Zephyr nRF52840 tag ELF
                         \
trusted gateway C         -> generated Renode city-sector script
  -> Zephyr gateway ELF  /       |
                         BLEMedium + positions + power cut + attackers
                                          |
                              UART evidence from each gateway
                                          |
                         validator -> JSON -> HTML report -> IDE panel
```

`docker/scripts/build_firmware.sh` builds both ELFs. The participant source is
read from the per-user firmware PVC; the gateway verifier is linked from a
precompiled organizer object and the immutable protocol tests live in
the immutable image.

`docker/scripts/generate_swarm_resc.py` expands environment settings into a
normal Renode monitor script. It assigns every machine a unique BLE controller
address and writes its deterministic private seed into an unused flash page
after ELF loading. The firmware reads that page exactly as a provisioned device
would read protected identity material.

The generator also loads an erased 8 KiB journal image into each machine. After
three virtual seconds it resets tag 1 without clearing that memory, then adds a
second-address attacker that replays tag 1's valid epoch-0 packet.

`docker/scripts/validate_swarm.py` uses only observable gateway UART output. It
does not inspect participant C variables or tag RAM.

## Scale modes

| Mode | Tags | Gateways | Clone | Replay | Purpose |
|---|---:|---:|---:|---:|---|
| local smoke | 6 | 3 | 1 | 1 | image/runtime check |
| participant IDE | 6 | 3 | 1 | 1 | normal feedback loop |
| one showcase sector | 16 | 3 | 1 | 1 | cluster stress unit |
| 12-sector indexed Job | 192 | 36 | 12 | 12 | 252-board spectacle |

Sectors are independent deterministic RF domains. Kubernetes assigns each
Indexed Job completion a sector number through `JOB_COMPLETION_INDEX`.
