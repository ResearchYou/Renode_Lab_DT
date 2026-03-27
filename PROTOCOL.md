# STMP v1 — SPI Test Messaging Protocol

## Overview

**STMP v1** (SPI Test Messaging Protocol, version 1) is a compact, fixed-frame
binary protocol designed for testing master–slave SPI communication on bare-metal
STM32F7 microcontrollers.  It exercises four distinct message semantics—ping,
register write, register read, and status query—across a deterministic 10-exchange
test sequence that validates CRC integrity, register persistence, and slave-side
counter accuracy.

The same firmware binary runs on both boards.  The role (master / slave) is
injected into a no-init SRAM word at `0x20000000` by the Renode emulation script
before execution starts.

---

## Frame Format

Every frame is exactly **21 bytes**.

```
Offset  Size  Field    Value / Description
──────  ────  ───────  ──────────────────────────────────────────────────
  0      1    MAGIC    0xA5  — start-of-frame marker
  1      1    SEQ      1–255 — exchange sequence number (1-based)
  2      1    CMD      opcode (see Command Codes below)
  3      1    FLAGS    0x00  — reserved, must be zero
  4–18  15    PAYLOAD  command-specific data; unused bytes zero-padded
 19      1    CRC8     CRC-8/SMBUS over bytes 0–18
 20      1    GUARD    0x55  — end-of-frame marker
```

### CRC-8/SMBUS

| Parameter     | Value                        |
|---------------|------------------------------|
| Polynomial    | 0x07 (x⁸ + x² + x + 1)      |
| Initial value | 0x00                         |
| Input reflect | No                           |
| Output reflect| No                           |
| Final XOR     | 0x00                         |

The CRC is computed over bytes `[0:18]` (19 bytes; GUARD is excluded).

---

## Command Codes

### 0x10 PING_REQ  /  0x11 PING_RSP

Purpose: round-trip latency check; verifies basic frame routing.

| Direction    | CMD  | PAYLOAD                    |
|--------------|------|----------------------------|
| master→slave | 0x10 | all zeros                  |
| slave→master | 0x11 | `[0]` = echo of SEQ        |

### 0x20 REG_WR  /  0x21 REG_WR_ACK

Purpose: write a 32-bit value into the slave's virtual register file.

| Direction    | CMD  | PAYLOAD                                           |
|--------------|------|---------------------------------------------------|
| master→slave | 0x20 | `[0]` = reg_id (0–15); `[1:4]` = value (LE)      |
| slave→master | 0x21 | `[0]` = reg_id echo; `[1]` = status (see below)  |

### 0x30 REG_RD  /  0x31 REG_RD_RSP

Purpose: read a 32-bit value from the slave's virtual register file.

| Direction    | CMD  | PAYLOAD                                           |
|--------------|------|---------------------------------------------------|
| master→slave | 0x30 | `[0]` = reg_id (0–15); `[1:14]` = 0x00           |
| slave→master | 0x31 | `[0]` = reg_id echo; `[1:4]` = value (LE)        |

### 0x40 STAT_REQ  /  0x41 STAT_RSP

Purpose: query slave-side packet counters.

| Direction    | CMD  | PAYLOAD                                               |
|--------------|------|-------------------------------------------------------|
| master→slave | 0x40 | all zeros                                             |
| slave→master | 0x41 | `[0:3]` = rx_count (LE); `[4:7]` = tx_count (LE);   |
|              |      | `[8:11]` = err_count (LE); `[12:14]` = 0x00          |

`rx_count` is incremented **before** the response is built; `tx_count` is
incremented **after**.  Thus for a STAT_REQ at exchange N:
- `rx_count = N`
- `tx_count = N − 1`

---

## Status Codes (REG_WR_ACK payload[1])

| Value | Name        | Meaning                                  |
|-------|-------------|------------------------------------------|
| 0x00  | OK          | Operation succeeded                      |
| 0x01  | ERR_REG     | Register address out of range (0–15)     |
| 0x02  | ERR_CMD     | Unknown command code received            |
| 0x03  | ERR_CRC     | Incoming frame failed CRC-8/SMBUS check  |

---

## Register File

The slave maintains 16 virtual 32-bit registers, `reg_table[0..15]`,
zero-initialised on reset.  Registers persist across exchanges for the
duration of a single emulation run.

---

## Test Sequence

10 exchanges with SEQ values 1–10:

| SEQ | Master sends       | Slave responds      | Validation                        |
|-----|--------------------|---------------------|-----------------------------------|
| 1   | PING_REQ           | PING_RSP            | payload[0] == 0x01                |
| 2   | PING_REQ           | PING_RSP            | payload[0] == 0x02                |
| 3   | REG_WR reg=0 val=0xCAFEBABE | REG_WR_ACK | payload[0]==0, payload[1]==OK    |
| 4   | REG_RD reg=0       | REG_RD_RSP          | payload[0]==0, val==0xCAFEBABE    |
| 5   | REG_WR reg=1 val=0xDEAD1234 | REG_WR_ACK | payload[0]==1, payload[1]==OK    |
| 6   | REG_RD reg=1       | REG_RD_RSP          | payload[0]==1, val==0xDEAD1234    |
| 7   | STAT_REQ           | STAT_RSP            | rx_count==7, tx_count==6          |
| 8   | PING_REQ           | PING_RSP            | payload[0] == 0x08                |
| 9   | REG_RD reg=0       | REG_RD_RSP          | val==0xCAFEBABE (persisted from 3)|
| 10  | STAT_REQ           | STAT_RSP            | rx_count==10, tx_count==9         |

---

## Expected Bus Frames

All frames below are exact hex; CRC field is pre-computed with CRC-8/SMBUS.

```
SEQ  DIR   CMD         Frame (21 bytes, hex)
───  ────  ──────────  ──────────────────────────────────────────────────
 1   MOSI  PING_REQ    A5 01 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 DD 55
 1   MISO  PING_RSP    A5 01 11 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 66 55
 2   MOSI  PING_REQ    A5 02 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 A3 55
 2   MISO  PING_RSP    A5 02 11 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 C0 55
 3   MOSI  REG_WR[0]   A5 03 20 00 00 BE BA FE CA 00 00 00 00 00 00 00 00 00 00 D3 55
 3   MISO  REG_WR_ACK  A5 03 21 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 A0 55
 4   MOSI  REG_RD[0]   A5 04 30 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 98 55
 4   MISO  REG_RD_RSP  A5 04 31 00 00 BE BA FE CA 00 00 00 00 00 00 00 00 00 00 EB 55
 5   MOSI  REG_WR[1]   A5 05 20 00 01 34 12 AD DE 00 00 00 00 00 00 00 00 00 00 82 55
 5   MISO  REG_WR_ACK  A5 05 21 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 E9 55
 6   MOSI  REG_RD[1]   A5 06 30 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 79 55
 6   MISO  REG_RD_RSP  A5 06 31 00 01 34 12 AD DE 00 00 00 00 00 00 00 00 00 00 12 55
 7   MOSI  STAT_REQ    A5 07 40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 48 55
 7   MISO  STAT_RSP    A5 07 41 00 07 00 00 00 06 00 00 00 00 00 00 00 00 00 00 06 55
 8   MOSI  PING_REQ    A5 08 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 A0 55
 8   MISO  PING_RSP    A5 08 11 00 08 00 00 00 00 00 00 00 00 00 00 00 00 00 00 1D 55
 9   MOSI  REG_RD[0]   A5 09 30 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 4D 55
 9   MISO  REG_RD_RSP  A5 09 31 00 00 BE BA FE CA 00 00 00 00 00 00 00 00 00 00 3E 55
10   MOSI  STAT_REQ    A5 0A 40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 9D 55
10   MISO  STAT_RSP    A5 0A 41 00 0A 00 00 00 09 00 00 00 00 00 00 00 00 00 00 AA 55
```

---

## Physical / Emulation Layer

| Parameter       | Value                                      |
|-----------------|--------------------------------------------|
| SPI mode        | Mode 0 (CPOL=0, CPHA=0)                   |
| Bit order       | MSB first                                  |
| Clock           | fPCLK/2 ≈ 8 MHz (STM32F746 @ 16 MHz HSI) |
| CS              | Software-controlled (PA4, active-low)      |
| Data lines      | PA5=SCK, PA6=MISO, PA7=MOSI (AF5)         |
| Transfer phases | TX (request) then RX (response), separate CS assertions |
| Renode bridge   | `SPIBridgeSlave` (master spi1) ↔ `SPISlaveController` (slave sysbus 0x50000000) |

---

## File Structure

```
firmware/spi_test/
  protocol.h        STMP v1 types, CRC-8, and packet helpers
  main.c            Dual-role firmware (master test loop + slave ISR)
  stm32f746.h       Peripheral register definitions
  startup.s         Cortex-M7 vector table and reset handler
  linker.ld         STM32F746 memory map

renode/
  peripherals/
    SPIBridgeSlave.cs     Renode C# peripheral: MOSI packet delivery, MISO response
    SPISlaveController.cs Renode C# peripheral: cross-machine SRAM bridge + IRQ
  scripts/
    spi_test.resc         Emulation setup script

docker/
  Dockerfile.spi          Container image (gcc-arm-none-eabi + Renode)
  scripts/
    build_spi_firmware.sh Firmware build step
    run_spi_test.sh       Test runner + STMP v1 frame checker
    generate_spi_report.py HTML report generator

PROTOCOL.md         This document
```
