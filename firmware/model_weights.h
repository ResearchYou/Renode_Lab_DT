#ifndef MODEL_WEIGHTS_H
#define MODEL_WEIGHTS_H

#include <stdint.h>

/*
 * Tiny fixed-point classifier for the lab.
 *
 * Features:
 *   abs_center = abs(value - MODEL_CENTER_VALUE)
 *   abs_jump   = abs(value - previous_kept)
 *
 * Score:
 *   score = MODEL_BIAS_Q0
 *         + MODEL_WEIGHT_ABS_CENTER_Q0 * abs_center
 *         + MODEL_WEIGHT_ABS_JUMP_Q0   * abs_jump
 *
 * Keep the sample when score >= MODEL_KEEP_THRESHOLD_Q0.
 */
#define MODEL_INITIAL_VALUE 500
#define MODEL_CENTER_VALUE 500
#define MODEL_BIAS_Q0 1200
#define MODEL_WEIGHT_ABS_CENTER_Q0 (-6)
#define MODEL_WEIGHT_ABS_JUMP_Q0 (-10)
#define MODEL_KEEP_THRESHOLD_Q0 0

#endif /* MODEL_WEIGHTS_H */
