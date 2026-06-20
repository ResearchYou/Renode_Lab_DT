# Lab Workflow: Renode RP2040 Sensor Filtering

This lab is designed for a 3-hour session:

- 1h30 guided introduction to Renode and the digital-twin workflow.
- 1h30 hands-on firmware exercise using a virtual sensor stream.

The exercise target is a single Raspberry Pi Pico firmware project. Renode
provides the board, the I2C sensor, UART output capture, and the validation
report.

## Learning Goals

By the end of the lab, students should be able to:

- Explain the role of `.resc` scripts and `.repl` platform descriptions.
- Run a Pico firmware image inside Renode without physical hardware.
- Attach a custom virtual peripheral to an emulated bus.
- Read Renode/UART output as validation evidence.
- Implement and compare a threshold filter with a tiny fixed-point model.

## Session Plan

### 0:00-0:15: Digital Twin Setup

Show the project shape:

- `firmware/main.c`: firmware under test.
- `renode/run_test.resc`: Renode scenario script.
- `renode/sensor_i2c.repl`: virtual wiring overlay.
- `renode/peripherals/VirtualSensorStream.cs`: custom sensor component.
- `docker/scripts/run_test.sh`: build/run/validate harness.
- `output/report.html`: generated validation report.

Run the starter once:

```bash
./run.sh up --build digital-twin
```

Expected starter result: firmware builds and reads all 10 samples, but final
filter checks fail because the TODO functions return `KEEP`.

### 0:15-0:35: RESC Script Walkthrough

Open `renode/run_test.resc`.

Discuss:

- `path add @/opt/renode-rp2040`: makes the community RP2040 support visible.
- `include @...VirtualSensorStream.cs`: compiles/loads the custom component.
- `include @boards/initialize_raspberry_pico.resc`: creates the Pico machine.
- `machine LoadPlatformDescription @...sensor_i2c.repl`: attaches the sensor.
- `sysbus LoadELF`: loads the built firmware.
- `sysbus.uart0 CreateFileBackend`: captures firmware logs.
- `emulation RunFor`: defines the simulation window.

Key point: `.resc` files are automation scripts for creating, wiring, running,
and observing machines.

### 0:35-0:55: REPL and Virtual Wiring

Open `renode/sensor_i2c.repl`.

Discuss:

- The left side names a peripheral instance.
- The type `I2C.VirtualSensorStream` is the C# class loaded by the `.resc`.
- `@ i2c0 0x52` connects the peripheral to the Pico's I2C0 bus at address
  `0x52`.

Key point: `.repl` files describe hardware topology. They are the "virtual
wires" between CPUs, buses, and peripherals.

### 0:55-1:15: Custom Component

Open `renode/peripherals/VirtualSensorStream.cs`.

Discuss:

- `II2CPeripheral` is the Renode interface for I2C devices.
- `Write(byte[] data)` receives register-selection writes from firmware.
- `Read(int count)` returns bytes to the emulated I2C controller.
- The deterministic sample table makes the exercise repeatable.
- The component tracks byte-wise reads so a 4-byte sample frame remains stable
  even when the controller asks for one byte at a time.

Key point: a custom component can model only the behavior needed by the lab. It
does not need to be a complete physical sensor model.

### 1:15-1:30: IDE and Report Loop

Show the student loop:

1. Edit `firmware/main.c`.
2. Run the test from the IDE panel or with `./run.sh up digital-twin`.
3. Inspect terminal output and `output/report.html`.
4. Iterate until validation passes.

Explain that the report is generated from UART lines. Stable output strings are
part of the lab contract.

### 1:30-2:50: Exercise Block

Students implement:

- `threshold_filter_should_keep`
- `model_filter_score`
- `model_filter_should_keep`

Expected behavior:

- Threshold drops samples `seq=4` and `seq=6`.
- Model drops samples `seq=2`, `seq=4`, `seq=6`, and `seq=7`.
- Disagreements occur at `seq=2` and `seq=7`.
- Final summary:

```text
SUMMARY threshold_keep=8 threshold_drop=2 model_keep=6 model_drop=4 disagreements=2
```

### 2:50-3:00: Debrief

Discuss why the two methods differ:

- Threshold logic catches values outside hard limits and large immediate jumps.
- The model uses distance from the learned center and distance from the last
  accepted model value.
- The model is stricter on borderline jumps because both features contribute to
  the score.

## Instructor Reference

The completed drop-in firmware is in `reference/firmware/main.c`.

To validate it without modifying the student starter:

```bash
sg docker -c 'DOCKER_HOST=unix:///var/run/docker.sock docker run --rm --entrypoint /bin/bash \
  -v "$PWD":/host:ro renode_dt-digital-twin:latest \
  -lc "cp /host/reference/firmware/main.c /workspace/firmware/main.c && /workspace/scripts/entrypoint.sh"'
```

If the current shell already has the `docker` group, the outer `sg docker -c`
wrapper is not needed. Keep the `DOCKER_HOST=unix:///var/run/docker.sock`
prefix if your environment points Docker at a stale rootless socket.
