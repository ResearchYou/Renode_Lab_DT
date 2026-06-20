#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "model_weights.h"
#include "pico/stdlib.h"

#define SENSOR_I2C i2c0
#define SENSOR_ADDR 0x52
#define SENSOR_REG_NEXT_SAMPLE 0x00
#define SENSOR_SAMPLE_COUNT 10

#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5

#define THRESHOLD_MIN_VALUE 350
#define THRESHOLD_MAX_VALUE 750
#define THRESHOLD_MAX_JUMP 220

typedef struct {
    uint16_t seq;
    int16_t value;
} sensor_sample_t;

static int abs_i32(int value)
{
    return value < 0 ? -value : value;
}

static void sensor_init(void)
{
    i2c_init(SENSOR_I2C, 100 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

static bool sensor_read_next(sensor_sample_t *sample)
{
    uint8_t reg = SENSOR_REG_NEXT_SAMPLE;
    uint8_t raw[4] = {0};

    int rc = i2c_write_blocking(SENSOR_I2C, SENSOR_ADDR, &reg, 1, true);
    if (rc == PICO_ERROR_GENERIC) {
        return false;
    }

    rc = i2c_read_blocking(SENSOR_I2C, SENSOR_ADDR, raw, sizeof(raw), false);
    if (rc == PICO_ERROR_GENERIC) {
        return false;
    }

    sample->seq = (uint16_t)((raw[0] << 8) | raw[1]);
    sample->value = (int16_t)((raw[2] << 8) | raw[3]);
    return true;
}

static bool threshold_filter_should_keep(int16_t value, int16_t previous_kept)
{
    if (value < THRESHOLD_MIN_VALUE || value > THRESHOLD_MAX_VALUE) {
        return false;
    }
    return abs_i32(value - previous_kept) <= THRESHOLD_MAX_JUMP;
}

static int32_t model_filter_score(int16_t value, int16_t previous_kept)
{
    int32_t abs_center = abs_i32(value - MODEL_CENTER_VALUE);
    int32_t abs_jump = abs_i32(value - previous_kept);
    return MODEL_BIAS_Q0 +
           (MODEL_WEIGHT_ABS_CENTER_Q0 * abs_center) +
           (MODEL_WEIGHT_ABS_JUMP_Q0 * abs_jump);
}

static bool model_filter_should_keep(int16_t value, int16_t previous_kept,
                                     int32_t *score)
{
    *score = model_filter_score(value, previous_kept);
    return *score >= MODEL_KEEP_THRESHOLD_Q0;
}

int main(void)
{
    stdio_init_all();
    sleep_ms(200);

    printf("BOOT: RP2040 sensor filter TinyML lab\n");
    sensor_init();
    printf("I2C sensor ready addr=0x%02X\n", SENSOR_ADDR);

    int16_t threshold_previous = MODEL_INITIAL_VALUE;
    int16_t model_previous = MODEL_INITIAL_VALUE;
    uint32_t threshold_keep = 0;
    uint32_t threshold_drop = 0;
    uint32_t model_keep = 0;
    uint32_t model_drop = 0;
    uint32_t disagreements = 0;

    for (uint32_t i = 0; i < SENSOR_SAMPLE_COUNT; i++) {
        sensor_sample_t sample;
        if (!sensor_read_next(&sample)) {
            printf("ERROR: sensor read failed at index=%lu\n", (unsigned long)i);
            break;
        }

        printf("SAMPLE seq=%u value=%d\n", (unsigned)sample.seq, sample.value);

        bool threshold_decision =
            threshold_filter_should_keep(sample.value, threshold_previous);
        if (threshold_decision) {
            threshold_keep++;
            threshold_previous = sample.value;
        } else {
            threshold_drop++;
        }
        printf("THRESHOLD seq=%u decision=%s previous=%d value=%d\n",
               (unsigned)sample.seq, threshold_decision ? "KEEP" : "DROP",
               threshold_previous, sample.value);

        int32_t score = 0;
        bool model_decision =
            model_filter_should_keep(sample.value, model_previous, &score);
        if (model_decision) {
            model_keep++;
            model_previous = sample.value;
        } else {
            model_drop++;
        }
        printf("MODEL seq=%u decision=%s score=%ld previous=%d value=%d\n",
               (unsigned)sample.seq, model_decision ? "KEEP" : "DROP",
               (long)score, model_previous, sample.value);

        if (threshold_decision != model_decision) {
            disagreements++;
            printf("DISAGREE seq=%u threshold=%s model=%s\n",
                   (unsigned)sample.seq, threshold_decision ? "KEEP" : "DROP",
                   model_decision ? "KEEP" : "DROP");
        }

        sleep_ms(50);
    }

    printf("SUMMARY threshold_keep=%lu threshold_drop=%lu model_keep=%lu "
           "model_drop=%lu disagreements=%lu\n",
           (unsigned long)threshold_keep, (unsigned long)threshold_drop,
           (unsigned long)model_keep, (unsigned long)model_drop,
           (unsigned long)disagreements);
    printf("TEST COMPLETE\n");

    while (1) {
        tight_loop_contents();
    }

    return 0;
}
