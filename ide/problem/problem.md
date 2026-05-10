# Challenge: I2C Humidity Threshold Alerts

## Scenario

Two RP2040 microcontrollers communicate over I2C:

```
[Node RP2040]                        [Master RP2040]
  HDC1080 humidity sensor (I2C0)       polls Node every 3 s over I2C
  SX1276 LoRa module (SPI0)            logs reading to UART
  I2C1 slave — responds to master
```

The **node** reads humidity every 2 seconds from HDC1080 and exposes it on the
I2C slave register `0x01`.

The **master** periodically reads humidity and raises an alert in UART output when
the value exceeds a configured threshold.

## Your task

Both firmwares are complete and runnable. Extend or modify them:

- **Node** (`firmware/node/main.c`): keep the humidity sampling path stable and
  deterministic.
- **Master** (`firmware/master/main.c`): process threshold-based alerts and print
  a human-readable warning whenever the value is above the limit.

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

Pass condition: both UARTs show boot messages, node logs humidity reads and LoRa
TX events, and master reports at least one threshold alert.
