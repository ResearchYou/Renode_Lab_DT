# GhostTag Lab Workflow

## Build and execution flow

```text
participant C
  -> native known-answer/tamper binary
  -> Zephyr nRF52840 tag ELF
                         \
trusted gateway C         -> generated Renode city-sector script
  -> Zephyr gateway ELF  /       |
                                BLEMedium + positions + 3 gateways
                                          |
                              UART evidence from each gateway
                                          |
                         validator -> JSON -> HTML report -> IDE panel
```

`docker/scripts/build_firmware.sh` builds both ELFs. The participant source is
read from the per-user firmware PVC; the gateway and reference protocol live in
the immutable image.

`docker/scripts/generate_swarm_resc.py` expands environment settings into a
normal Renode monitor script. It assigns every machine a unique BLE controller
address and writes its deterministic private seed into an unused flash page
after ELF loading. The firmware reads that page exactly as a provisioned device
would read protected identity material.

`docker/scripts/validate_swarm.py` uses only observable gateway UART output. It
does not inspect participant C variables or tag RAM.

## Scale modes

| Mode | Tags | Gateways | Rogues | Purpose |
|---|---:|---:|---:|---|
| local smoke | 6 | 3 | 2 | image/runtime check |
| participant IDE | 12 | 3 | 2 | normal feedback loop |
| one showcase sector | 16 | 3 | 2 | cluster stress unit |
| 12-sector indexed Job | 192 | 36 | 24 | 252-board spectacle |

Sectors are independent deterministic RF domains. Kubernetes assigns each
Indexed Job completion a sector number through `JOB_COMPLETION_INDEX`.
