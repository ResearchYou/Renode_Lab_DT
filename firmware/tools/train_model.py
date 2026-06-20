#!/usr/bin/env python3
"""Reference generator for the lab's tiny fixed-point model weights.

This is intentionally small: the lab is about deploying and validating a model
inside a Renode digital twin, not about training infrastructure.
"""

SAMPLES = [500, 506, 612, 520, 785, 532, 340, 650, 545, 498]
WEIGHTS = {
    "MODEL_INITIAL_VALUE": 500,
    "MODEL_CENTER_VALUE": 500,
    "MODEL_BIAS_Q0": 1200,
    "MODEL_WEIGHT_ABS_CENTER_Q0": -6,
    "MODEL_WEIGHT_ABS_JUMP_Q0": -10,
    "MODEL_KEEP_THRESHOLD_Q0": 0,
}


def score(value, previous_kept):
    return (
        WEIGHTS["MODEL_BIAS_Q0"]
        + WEIGHTS["MODEL_WEIGHT_ABS_CENTER_Q0"]
        * abs(value - WEIGHTS["MODEL_CENTER_VALUE"])
        + WEIGHTS["MODEL_WEIGHT_ABS_JUMP_Q0"] * abs(value - previous_kept)
    )


def main():
    previous = WEIGHTS["MODEL_INITIAL_VALUE"]
    for seq, value in enumerate(SAMPLES):
        current_score = score(value, previous)
        keep = current_score >= WEIGHTS["MODEL_KEEP_THRESHOLD_Q0"]
        print(
            f"seq={seq} value={value} score={current_score} "
            f"decision={'KEEP' if keep else 'DROP'}"
        )
        if keep:
            previous = value


if __name__ == "__main__":
    main()
