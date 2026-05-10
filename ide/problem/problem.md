# Challenge: Commandable Sample Interval over I2C

## Scenario

Two RP2040 microcontrollers communicate over I2C:

```
[Node RP2040]                        [Master RP2040]
  HDC1080 humidity sensor (I2C0)       polls Node every 3 s over I2C
  SX1276 LoRa module (SPI0)            issues interval commands via I2C
  I2C1 slave — responds to master
```

The **node** reads humidity from HDC1080 and exposes it on register `0x01`.
The master can also write command bytes (`0x11` for fast, `0x10` for slow) to
reconfigure the node sampling period at runtime.

The **master** alternates command writes between fast and slow modes and keeps
polling humidity.

## Your task

Both firmwares are complete and runnable. Extend or modify them:

- **Node** (`firmware/node/main.c`): implement command handling in the I2C slave path
  and apply interval changes safely.
- **Master** (`firmware/master/main.c`): send interval-control commands to the
  node and observe how the node behavior changes.

## Hardware mapping

| Signal       | Node pin | Master pin |
|--------------|----------|------------|
| I2C SDA      | GP4      | GP4        |
| I2C SCL      | GP5      | GP5        |
| Node slave   | I2C1 GP2/GP3 | polls 0x08 |
| HDC1080      | I2C0 GP4/GP5 addr 0x40 | — |
| SX1276 CS    | GP17     | —          |
| SX1276 SCK   | GP18     | —          |
| SX1276 MOSI  | GP19     | —          |
| SX1276 MISO  | GP16     | —          |

## Running tests

```bash
docker compose up digital-twin
```

Pass condition: both UARTs show boot messages, node logs humidity reads and command
processing, and master sends command bytes.
