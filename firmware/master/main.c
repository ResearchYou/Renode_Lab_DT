/*
 * Master firmware — RP2040
 *
 * Peripherals:
 *   I2C0 (GP4=SDA, GP5=SCL) — polls node at I2C address 0x08
 *
 * Protocol:
 *   Write 0x01 (humidity register) → read 2 bytes big-endian uint16
 *   Value is humidity * 100 (e.g. 0x17A2 = 6050 → 60.50%)
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"

#define NODE_I2C_ADDR       0x08
#define NODE_REG_HUMIDITY   0x01

#define POLL_INTERVAL_MS    3000

static float poll_node_humidity(void) {
    uint8_t reg = NODE_REG_HUMIDITY;
    uint8_t raw[2] = {0};

    int rc = i2c_write_blocking(i2c0, NODE_I2C_ADDR, &reg, 1, true);
    if (rc == PICO_ERROR_GENERIC) {
        printf("[MASTER] ERROR: node not responding (write)\n");
        return -1.0f;
    }

    sleep_ms(5);

    rc = i2c_read_blocking(i2c0, NODE_I2C_ADDR, raw, 2, false);
    if (rc == PICO_ERROR_GENERIC) {
        printf("[MASTER] ERROR: node not responding (read)\n");
        return -1.0f;
    }

    uint16_t val = (uint16_t)((raw[0] << 8) | raw[1]);
    return val / 100.0f;
}

int main(void) {
    stdio_init_all();
    printf("[MASTER] Boot: polling master\n");

    i2c_init(i2c0, 100 * 1000);
    gpio_set_function(4, GPIO_FUNC_I2C);
    gpio_set_function(5, GPIO_FUNC_I2C);
    gpio_pull_up(4);
    gpio_pull_up(5);

    printf("[MASTER] Initialized. Polling node 0x%02X every %d ms\n",
           NODE_I2C_ADDR, POLL_INTERVAL_MS);

    uint32_t poll_count = 0;

    while (1) {
        sleep_ms(POLL_INTERVAL_MS);

        float humidity = poll_node_humidity();
        if (humidity >= 0.0f) {
            printf("[MASTER] Poll #%lu: humidity=%.2f%%\n",
                   (unsigned long)++poll_count, humidity);
        }
    }

    return 0;
}
