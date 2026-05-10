# Challenge: I2C Humidity + Temperature Node + LoRa Payload

## Scenario

Two RP2040 microcontrollers communicate over I2C:

```
[Node RP2040]                        [Master RP2040]
  HDC1080 humidity/temperature sensor (I2C0)   polls Node every 3 s over I2C
  SX1276 LoRa module (SPI0)                       logs reading to UART
  I2C1 slave — responds to master
```

The **node** samples humidity and temperature from an HDC1080 sensor, broadcasts
each sample set over LoRaWAN (SX1276), and exposes both values to the master via
its I2C slave registers.

The **master** polls the node every 3 seconds and logs the latest humidity and
temperature from its dedicated registers.

## Your task

Both firmwares are complete and runnable. Extend or modify them:

- **Node** (`firmware/node/main.c`): adjust the LoRa frame format, sampling rate,
  and the humidity/temperature telemetry layout.
- **Master** (`firmware/master/main.c`): parse multi-register telemetry and use
  both humidity and temperature in local decisions.

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

- `renode/peripherals/HDC1080Device.cs` — I2C peripheral that returns oscillating
  humidity and temperature values on the node's I2C0 bus.
- `renode/peripherals/SX1276Device.cs` — SPI peripheral that logs LoRa TX events to
  the Renode log when TX mode is triggered.
- `renode/peripherals/NodeI2CBridge.cs` — I2C peripheral on the master's I2C0 bus
  that simulates the node's slave response:
  register 0x01 for humidity, register 0x02 for temperature.

True inter-machine I2C wiring is not modelled; each machine's peripherals are
simulated independently. On real hardware the node's I2C1 slave and the
master's I2C0 master connect directly.

## Running tests

```bash
docker compose up digital-twin
```

Pass condition: both UARTs show boot messages, node logs sensor reads and LoRa
TX events, master logs humidity and temperature poll results.
