# Lab: Renode RP2040 Sensor Filtering

## Scenario

You are working with a Raspberry Pi Pico firmware project in a Renode digital
twin. Renode provides a virtual I2C sensor, so no physical board or sensor is
needed.

```
[Virtual I2C sensor @ 0x52] -> sample stream -> [RP2040 firmware]
```

Each sample contains:

- `seq`: sample number
- `value`: signed sensor reading

The firmware must classify each sample with two filters:

- **Threshold filter**: hand-written `if`/`else` logic using min, max, and
  maximum jump thresholds.
- **TinyML filter**: a small fixed-point linear model using weights shipped in
  `firmware/model_weights.h`.

## Your Task

Edit `firmware/main.c` and complete:

- `threshold_filter_should_keep`
- `model_filter_score`
- `model_filter_should_keep`

Use the constants already defined in `firmware/main.c` and
`firmware/model_weights.h`. Do not change the UART line formats; the validation
harness reads those lines.

The target behavior is:

- Threshold filter catches the gross high and low outliers.
- TinyML filter also rejects two borderline jump samples.
- The final summary line is:

```text
SUMMARY threshold_keep=8 threshold_drop=2 model_keep=6 model_drop=4 disagreements=2
```

## Important Files

- `firmware/main.c` — student TODOs.
- `firmware/model_weights.h` — fixed-point model constants.
- `renode/run_test.resc` — Renode script that boots the Pico and attaches the
  sensor.
- `renode/sensor_i2c.repl` — virtual wiring overlay.
- `renode/peripherals/VirtualSensorStream.cs` — deterministic sensor model.

## Running Tests

From the IDE, press **Run** in the side panel.

From a shell:

```bash
./run.sh up digital-twin
```

The starter code boots and reads samples, but the validation fails until the two
filter TODOs are implemented.

## Implementation Hints

For the threshold filter:

- Reject values below `THRESHOLD_MIN_VALUE`.
- Reject values above `THRESHOLD_MAX_VALUE`.
- Reject values whose absolute jump from `previous_kept` exceeds
  `THRESHOLD_MAX_JUMP`.

For the model filter:

- Compute `abs_center = abs(value - MODEL_CENTER_VALUE)`.
- Compute `abs_jump = abs(value - previous_kept)`.
- Apply the fixed-point score formula from `model_weights.h`.
- Keep the sample when the score is at least `MODEL_KEEP_THRESHOLD_Q0`.
