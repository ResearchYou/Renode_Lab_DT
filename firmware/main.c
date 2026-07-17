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
#define GHOST_JOURNAL_BASE 0x000FD000u
#define GHOST_EPOCH_MS 2000u
#define GHOST_ROLE_REPLAY 0x52504C59u

struct runtime_storage {
    uint32_t writes;
    uint32_t erases;
};

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

static int journal_read(void *context, uint32_t offset, void *data, size_t length)
{
    (void)context;
    uint8_t *destination = data;
    if ((size_t)offset + length > GHOST_JOURNAL_SIZE) {
        return -1;
    }
    for (size_t index = 0; index < length; ++index) {
        destination[index] = sys_read8(GHOST_JOURNAL_BASE + offset + index);
    }
    return 0;
}

static int journal_write(void *context, uint32_t offset, const void *data,
                         size_t length)
{
    struct runtime_storage *storage = context;
    const uint8_t *source = data;
    if ((size_t)offset + length > GHOST_JOURNAL_SIZE) {
        return -1;
    }
    for (size_t index = 0; index < length; ++index) {
        const uintptr_t address = GHOST_JOURNAL_BASE + offset + index;
        const uint8_t current = sys_read8(address);
        if ((uint8_t)(~current) & source[index]) {
            return -1;
        }
        sys_write8(current & source[index], address);
    }
    storage->writes++;
    return 0;
}

static int journal_erase(void *context, uint32_t offset, size_t length)
{
    struct runtime_storage *storage = context;
    if ((size_t)offset + length > GHOST_JOURNAL_SIZE ||
        offset % GHOST_JOURNAL_PAGE_SIZE != 0u ||
        length != GHOST_JOURNAL_PAGE_SIZE) {
        return -1;
    }
    for (size_t index = 0; index < length; ++index) {
        sys_write8(0xffu, GHOST_JOURNAL_BASE + offset + index);
    }
    storage->erases++;
    return 0;
}

static int start_advertising(void)
{
    int err = bt_enable(NULL);
    if (err != 0) {
        printk("GHOST_FATAL bluetooth_init=%d\n", err);
        return err;
    }
    err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err != 0) {
        printk("GHOST_FATAL advertising_start=%d\n", err);
    }
    return err;
}

int main(void)
{
    if (sys_read32(GHOST_CONFIG_BASE) != GHOST_CONFIG_MAGIC) {
        printk("GHOST_FATAL missing simulation identity\n");
        return 0;
    }

    const uint64_t seed = read_seed();
    const uint32_t sector = sys_read32(GHOST_CONFIG_BASE + 12u);
    const uint32_t role = sys_read32(GHOST_CONFIG_BASE + 16u);
    const uint8_t flags = (seed % 7u == 0u) ? GHOST_FLAG_DISTRESS : 0u;

    if (role == GHOST_ROLE_REPLAY) {
        ghost_build_payload(seed, sector, 0u, flags, advertisement);
        if (start_advertising() != 0) {
            return 0;
        }
        printk("GHOST_TAG_READY sector=%u protocol=%u role=replay epoch=0\n",
               sector, GHOST_PROTOCOL_VERSION);
        while (true) {
            k_sleep(K_SECONDS(1));
        }
    }

    struct runtime_storage runtime = {0};

    const struct ghost_storage storage = {
        .context = &runtime,
        .read = journal_read,
        .write = journal_write,
        .erase = journal_erase,
    };
    struct ghost_state state;
    if (ghost_state_boot(&state, seed, sector, &storage) != 0) {
        printk("GHOST_FATAL state_boot\n");
        return 0;
    }
    if (ghost_state_next_payload(&state, flags, advertisement) != 0) {
        printk("GHOST_FATAL state_first_payload\n");
        return 0;
    }

    uint32_t advertisements = 1u;
    uint32_t epoch = ghost_payload_epoch(advertisement);
    if (start_advertising() != 0) {
        return 0;
    }

    printk("GHOST_TAG_READY sector=%u protocol=%u role=tag\n", sector,
           GHOST_PROTOCOL_VERSION);
    printk("GHOST_STATE_READY epoch=%u lease_end=%u generation=%u recovered=%u\n",
           epoch, state.lease_end, state.generation, state.recovered ? 1u : 0u);
    printk("GHOST_ENERGY epoch=%u advertisements=%u writes=%u erases=%u units=%u\n",
           epoch, advertisements, state.generation * 2u, state.erase_count,
           ghost_state_energy_units(&state, advertisements));

    while (true) {
        k_sleep(K_MSEC(GHOST_EPOCH_MS));
        if (ghost_state_next_payload(&state, flags, advertisement) != 0) {
            printk("GHOST_FATAL state_advance\n");
            return 0;
        }
        epoch = ghost_payload_epoch(advertisement);
        int err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
        if (err != 0) {
            printk("GHOST_UPDATE_FAIL epoch=%u err=%d\n", epoch, err);
        } else {
            advertisements++;
            printk("GHOST_ROTATE epoch=%u eid=%016llx\n", epoch,
                   (unsigned long long)ghost_payload_eid(advertisement));
            printk("GHOST_ENERGY epoch=%u advertisements=%u writes=%u erases=%u units=%u\n",
                   epoch, advertisements, state.generation * 2u,
                   state.erase_count,
                   ghost_state_energy_units(&state, advertisements));
        }
    }

    return 0;
}
