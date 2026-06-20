# Renode Primer for This Lab

This document explains the Renode files used in the lab. It is intentionally
scoped to the workflow students see here.

## RESC Files

`.resc` files are Renode monitor scripts. They automate what an instructor could
type manually in the Renode monitor.

In this lab, `renode/run_test.resc` does five jobs:

- Loads the custom C# sensor model.
- Creates a Raspberry Pi Pico machine.
- Applies the sensor wiring overlay.
- Loads `firmware.elf`.
- Captures UART output and runs the emulation.

Useful commands in this scenario:

```resc
include @/workspace/renode/peripherals/VirtualSensorStream.cs
EnsureTypeIsLoaded "Antmicro.Renode.Peripherals.I2C.VirtualSensorStream"
include @boards/initialize_raspberry_pico.resc
machine LoadPlatformDescription @/workspace/renode/sensor_i2c.repl
sysbus LoadELF @/workspace/build/firmware.elf
sysbus.uart0 CreateFileBackend @/workspace/output/uart_output.txt true
emulation RunFor "00:00:05"
```

## REPL Files

`.repl` files describe platform topology: which peripherals exist and where
they connect.

This lab uses a small overlay instead of redefining the whole Pico:

```repl
sensor: I2C.VirtualSensorStream @ i2c0 0x52
```

Read it as:

- Create an instance named `sensor`.
- Its Renode type is `I2C.VirtualSensorStream`.
- Attach it to bus `i2c0`.
- Use I2C address `0x52`.

## Custom Components

`VirtualSensorStream.cs` is a minimal Renode I2C peripheral. It models the
contract the firmware needs:

- Firmware writes register `0x00`.
- Firmware reads four bytes.
- The sensor returns `seq:uint16_be` and `value:int16_be`.
- Each complete frame advances to the next deterministic sample.

The component is intentionally not a full real-world sensor. For a lab, the
important part is deterministic behavior that students can reason about.

## Virtual Wiring

The virtual wiring path is:

```text
run_test.resc
  loads VirtualSensorStream.cs
  initializes Raspberry Pi Pico
  applies sensor_i2c.repl

sensor_i2c.repl
  connects VirtualSensorStream to i2c0 address 0x52

firmware/main.c
  initializes i2c0 on GP4/GP5
  writes register 0x00
  reads 4-byte sample frames
```

If any part of this chain is wrong, the firmware still builds, but UART output
will show failed sensor reads or incorrect samples.

## Validation Surface

The harness does not inspect C variables directly. It validates behavior through
UART lines:

- `SAMPLE seq=... value=...`
- `THRESHOLD seq=... decision=...`
- `MODEL seq=... decision=... score=...`
- `DISAGREE seq=...`
- `SUMMARY ...`

This keeps the digital twin close to a real board workflow: the observable
surface is serial output plus emulator logs.
