# Exercise Guide

The student exercise is in `firmware/main.c`. The completed reference is in
`reference/firmware/main.c`.

## Data Stream

The virtual sensor emits this deterministic stream:

| Seq | Value | Intended Role |
|-----|-------|---------------|
| 0 | 500 | Normal baseline |
| 1 | 506 | Small normal movement |
| 2 | 612 | Borderline jump |
| 3 | 520 | Return to normal |
| 4 | 785 | High outlier |
| 5 | 532 | Normal |
| 6 | 340 | Low outlier |
| 7 | 650 | Borderline jump |
| 8 | 545 | Normal |
| 9 | 498 | Normal |

## Threshold Filter

Implement `threshold_filter_should_keep`.

Rules:

- Drop values below `THRESHOLD_MIN_VALUE`.
- Drop values above `THRESHOLD_MAX_VALUE`.
- Drop values whose absolute jump from `previous_kept` is greater than
  `THRESHOLD_MAX_JUMP`.
- Keep all other values.

Expected drops: `seq=4`, `seq=6`.

## Tiny Fixed-Point Model

Implement `model_filter_score`.

The model uses two features:

- `abs_center = abs(value - MODEL_CENTER_VALUE)`
- `abs_jump = abs(value - previous_kept)`

The score formula is:

```c
score = MODEL_BIAS_Q0
      + MODEL_WEIGHT_ABS_CENTER_Q0 * abs_center
      + MODEL_WEIGHT_ABS_JUMP_Q0 * abs_jump;
```

Implement `model_filter_should_keep` by keeping samples whose score is greater
than or equal to `MODEL_KEEP_THRESHOLD_Q0`.

Expected drops: `seq=2`, `seq=4`, `seq=6`, `seq=7`.

## Passing Output

A passing run ends with:

```text
[PASS] Boot message
[PASS] I2C sensor init
[PASS] Sensor sample count (10)
[PASS] Threshold rejects high outlier
[PASS] Threshold rejects low outlier
[PASS] Model rejects borderline jump
[PASS] Model rejects second borderline jump
[PASS] Method disagreement count (2)
[PASS] Summary counts
[PASS] Test completion
>>> ALL CHECKS PASSED - sensor filters behave as expected <<<
```

The report is written to `output/report.html`.
