# Instructor Notes: RP2040 Sensor Filtering TinyML Lab

Use this file as the short run sheet. The fuller teaching material is in:

- `docs/lab_workflow.md`
- `docs/renode_primer.md`
- `docs/exercise_guide.md`

## Timing

- 0:00-0:15: Renode motivation, machine model, UART/log outputs.
- 0:15-0:35: RESC walkthrough with `renode/run_test.resc`.
- 0:35-0:55: REPL and virtual wiring walkthrough with `renode/sensor_i2c.repl`.
- 0:55-1:15: Custom peripheral walkthrough with `VirtualSensorStream.cs`.
- 1:15-1:30: Firmware/test/report workflow in the browser IDE.
- 1:30-2:50: Student exercise: complete threshold and model filters.
- 2:50-3:00: Debrief: why the model rejects borderline jump samples the naive
  threshold accepts.

## Demo Flow

1. Run `./run.sh up --build ide` and open `http://localhost:8443`.
2. Show the problem statement and `firmware/main.c` TODOs.
3. Open `renode/run_test.resc` and point out script ordering:
   custom peripheral include, Pico board init, REPL overlay, ELF load, UART file.
4. Open `renode/sensor_i2c.repl` and explain the sensor connection to `i2c0`.
5. Open `VirtualSensorStream.cs` and show how reads advance through the sample
   table.
6. Run the starter once; expected result is sample reads pass and filter checks
   fail.
7. Apply the reference implementation from `reference/firmware/main.c` in a
   throwaway container when demonstrating the final expected output.

## Expected Solution Behavior

- Threshold keeps 8 samples and drops 2.
- Model keeps 6 samples and drops 4.
- The two method disagreements are samples `seq=2` and `seq=7`.

## Validation Commands

Starter/TODO variant:

```bash
./run.sh up --build digital-twin
```

Expected starter result: build and sample acquisition pass; filter checks fail.

Reference implementation:

```bash
sg docker -c 'DOCKER_HOST=unix:///var/run/docker.sock docker run --rm --entrypoint /bin/bash \
  -v "$PWD":/host:ro renode_dt-digital-twin:latest \
  -lc "cp /host/reference/firmware/main.c /workspace/firmware/main.c && /workspace/scripts/entrypoint.sh"'
```

Expected reference result: all checks pass.
