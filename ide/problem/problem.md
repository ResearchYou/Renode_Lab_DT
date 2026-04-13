# Challenge: I2C Humidity Node + LoRaWAN Broadcast

## Scenario

Two RP2040 microcontrollers communicate over I2C:

```
[Node RP2040]                        [Master RP2040]
  HDC1080 humidity sensor (I2C0)       polls Node every 3 s over I2C
  SX1276 LoRa module (SPI0)            logs reading to UART
  I2C1 slave — responds to master
```

The **node** samples humidity from an HDC1080 sensor, broadcasts each reading
over LoRaWAN (SX1276), and exposes the latest value to the master via I2C slave.

The **master** polls the node every 3 seconds and logs the received humidity.

## Your task

Both firmwares are complete and runnable. Extend or modify them:

- **Node** (`firmware/node/main.c`): adjust the LoRa frame format, sampling
  rate, or add temperature to the payload.
- **Master** (`firmware/master/main.c`): parse multi-value frames, add
  threshold alerting, or request temperature as well.

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

## Simulation notes

In the Renode simulation:

- `renode/peripherals/hdc1080.py` — I2C peripheral that returns oscillating
  humidity values (40–80%) on the node's I2C0 bus.
- `renode/peripherals/sx1276.py` — SPI peripheral that logs LoRa TX events to
  the Renode log when TX mode is triggered.
- `renode/peripherals/node_i2c_bridge.py` — I2C peripheral on the master's
  I2C0 bus that simulates the node's slave response (returns the same humidity
  curve as the HDC1080 model).

True inter-machine I2C wiring is not modelled; each machine's peripherals are
simulated independently. On real hardware the node's I2C1 slave and the
master's I2C0 master connect directly.

## Running tests

```bash
docker compose up digital-twin
```

Pass condition: both UARTs show boot messages, node logs sensor reads and LoRa
TX events, master logs humidity poll results.
