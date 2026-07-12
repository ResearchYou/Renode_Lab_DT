# Renode Primer for GhostTag

GhostTag uses only capabilities shipped in stable Renode 1.16.1.

## Machines and board model

Every tag and gateway is a separate Renode machine loaded from
`platforms/cpus/nrf52840.repl`. The platform includes the Cortex-M4 CPU, flash,
RAM, UART, timers, RNG, ECB block, and `NRF52840_Radio` peripheral.

## BLE medium

The generated script creates one medium per sector:

```resc
emulation CreateBLEMedium "ghostAir"
ghostAir SetRangeWirelessFunction 92
emulation SetGlobalQuantum "0.00001"
```

Each radio is connected and positioned in 3D:

```resc
connector Connect radio ghostAir
ghostAir SetPosition radio 60 40 3
```

The 10 microsecond global quantum is the value used by Renode's official
nRF52840 Zephyr BLE multi-node example. The range function makes topology
matter while remaining deterministic.

## Provisioning without source variants

All tags run the same ELF. The harness gives each machine a unique BLE FICR
address and writes a seed/config record into the final nRF52840 flash page at
`0x000FF000`. That page is outside the application image. This makes one build
scale to dozens of distinct devices without compiling per-tag binaries.

## Evidence

Only gateway UARTs are authoritative for the fleet pass. Renode file backends
capture `GHOST_SIGHT`, `GHOST_ROGUE`, and `GHOST_SUMMARY` lines. One sample tag
UART is retained for debugging rotation failures.
