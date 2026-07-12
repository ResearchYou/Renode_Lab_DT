#include <stdint.h>
#include <stdio.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

#include "ghost_protocol.h"

#define GHOST_CONFIG_BASE 0x000FF000u
#define GHOST_CONFIG_MAGIC 0x47484F53u
#define GHOST_EPOCH_MS 2000u

static uint8_t advertisement[GHOST_PAYLOAD_SIZE];

static const struct bt_data ad[] = {
    BT_DATA(BT_DATA_MANUFACTURER_DATA, advertisement, sizeof(advertisement)),
};

static uint64_t read_seed(void)
{
    uint32_t low = sys_read32(GHOST_CONFIG_BASE + 4u);
    uint32_t high = sys_read32(GHOST_CONFIG_BASE + 8u);
    return ((uint64_t)high << 32) | low;
}

int main(void)
{
    if (sys_read32(GHOST_CONFIG_BASE) != GHOST_CONFIG_MAGIC) {
        printk("GHOST_FATAL missing simulation identity\n");
        return 0;
    }

    const uint64_t seed = read_seed();
    const uint32_t sector = sys_read32(GHOST_CONFIG_BASE + 12u);
    const uint8_t flags = (seed % 7u == 0u) ? GHOST_FLAG_DISTRESS : 0u;
    uint32_t epoch = 0u;

    ghost_build_payload(seed, sector, epoch, flags, advertisement);

    int err = bt_enable(NULL);
    if (err != 0) {
        printk("GHOST_FATAL bluetooth_init=%d\n", err);
        return 0;
    }

    err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err != 0) {
        printk("GHOST_FATAL advertising_start=%d\n", err);
        return 0;
    }

    printk("GHOST_TAG_READY sector=%u protocol=%u\n", sector,
           GHOST_PROTOCOL_VERSION);

    while (true) {
        uint32_t next_epoch = (uint32_t)(k_uptime_get() / GHOST_EPOCH_MS);
        if (next_epoch != epoch) {
            epoch = next_epoch;
            ghost_build_payload(seed, sector, epoch, flags, advertisement);
            err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
            if (err != 0) {
                printk("GHOST_UPDATE_FAIL epoch=%u err=%d\n", epoch, err);
            } else {
                printk("GHOST_ROTATE epoch=%u eid=%016llx\n", epoch,
                       (unsigned long long)ghost_payload_eid(advertisement));
            }
        }
        k_msleep(100u);
    }

    return 0;
}
