#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"

#define LED_PIN 25

int main() {
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    printf("BOOT: RP2040 Digital Twin POC\n");
    printf("UART initialized successfully\n");

    for (int i = 0; i < 5; i++) {
        // gpio_put(LED_PIN, 1);
        printf("[%u] LED ON  - cycle %d\n", (unsigned int)time_us_32(), i);
        sleep_ms(300);

        gpio_put(LED_PIN, 0);
        printf("[%u] LED OFF - cycle %d\n", (unsigned int)time_us_32(), i);
        sleep_ms(500);
    }

    printf("[%u] TEST COMPLETE\n", (unsigned int)time_us_32());

    while (1) {
        tight_loop_contents();
    }

    return 0;
}
