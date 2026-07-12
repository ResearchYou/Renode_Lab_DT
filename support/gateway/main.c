#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/sys_io.h>

#include "ghost_protocol.h"

#define GHOST_CONFIG_BASE 0x000FF000u
#define GHOST_CONFIG_MAGIC 0x47484F53u

struct tag_state {
    bool seen;
    uint32_t last_epoch;
    uint64_t last_eid;
    uint32_t sightings;
    uint32_t rotations;
};

static struct tag_state tags[GHOST_TAG_COUNT];
static uint32_t gateway_id;
static uint32_t unique_tags;
static uint32_t valid_packets;
static uint32_t rogue_packets;
static uint32_t total_rotations;
static uint32_t last_rogue_epoch = UINT32_MAX;

static uint64_t seed_for_tag(uint32_t tag_id)
{
    uint64_t value = (uint64_t)tag_id ^ UINT64_C(0x47484F5354544147);
    value += UINT64_C(0x9E3779B97F4A7C15);
    value = (value ^ (value >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    value = (value ^ (value >> 27)) * UINT64_C(0x94D049BB133111EB);
    return value ^ (value >> 31);
}

static bool identify(const uint8_t payload[GHOST_PAYLOAD_SIZE],
                     uint32_t *local_index)
{
    if (ghost_payload_sector(payload) != GHOST_SECTOR) {
        return false;
    }

    for (uint32_t i = 0; i < GHOST_TAG_COUNT; ++i) {
        uint32_t tag_id = GHOST_TAG_ID_BASE + i + 1u;
        if (ghost_verify_payload(seed_for_tag(tag_id), payload)) {
            *local_index = i;
            return true;
        }
    }
    return false;
}

static bool parse_ad(struct bt_data *data, void *user_data)
{
    (void)user_data;
    if (data->type != BT_DATA_MANUFACTURER_DATA ||
        data->data_len != GHOST_PAYLOAD_SIZE) {
        return true;
    }

    const uint8_t *payload = data->data;
    uint32_t local_index;
    uint32_t epoch = ghost_payload_epoch(payload);

    if (!identify(payload, &local_index)) {
        rogue_packets++;
        if (epoch != last_rogue_epoch) {
            last_rogue_epoch = epoch;
            printk("GHOST_ROGUE gateway=%u sector=%u epoch=%u reason=untrusted\n",
                   gateway_id, ghost_payload_sector(payload), epoch);
        }
        return true;
    }

    struct tag_state *state = &tags[local_index];
    uint32_t tag_id = GHOST_TAG_ID_BASE + local_index + 1u;
    uint64_t eid = ghost_payload_eid(payload);
    valid_packets++;
    state->sightings++;

    if (!state->seen) {
        state->seen = true;
        state->last_epoch = epoch;
        state->last_eid = eid;
        unique_tags++;
        printk("GHOST_SIGHT gateway=%u tag=%u sector=%u epoch=%u eid=%016llx first=1\n",
               gateway_id, tag_id, GHOST_SECTOR, epoch,
               (unsigned long long)eid);
    } else if (state->last_epoch != epoch) {
        bool rotated = state->last_eid != eid;
        state->last_epoch = epoch;
        state->last_eid = eid;
        if (rotated) {
            state->rotations++;
            total_rotations++;
        }
        printk("GHOST_SIGHT gateway=%u tag=%u sector=%u epoch=%u eid=%016llx rotated=%u\n",
               gateway_id, tag_id, GHOST_SECTOR, epoch,
               (unsigned long long)eid, rotated ? 1u : 0u);
    }
    return true;
}

static void device_found(const bt_addr_le_t *address, int8_t rssi,
                         uint8_t type, struct net_buf_simple *advertisement)
{
    (void)address;
    (void)rssi;
    (void)type;
    bt_data_parse(advertisement, parse_ad, NULL);
}

int main(void)
{
    if (sys_read32(GHOST_CONFIG_BASE) != GHOST_CONFIG_MAGIC) {
        printk("GATEWAY_FATAL missing simulation identity\n");
        return 0;
    }
    gateway_id = sys_read32(GHOST_CONFIG_BASE + 16u);

    int err = bt_enable(NULL);
    if (err != 0) {
        printk("GATEWAY_FATAL bluetooth_init=%d\n", err);
        return 0;
    }

    const struct bt_le_scan_param scan = {
        .type = BT_LE_SCAN_TYPE_PASSIVE,
        .options = BT_LE_SCAN_OPT_NONE,
        .interval = 0x0010,
        .window = 0x0010,
    };
    err = bt_le_scan_start(&scan, device_found);
    if (err != 0) {
        printk("GATEWAY_FATAL scan_start=%d\n", err);
        return 0;
    }

    printk("GHOST_GATEWAY_READY gateway=%u sector=%u fleet=%u\n", gateway_id,
           GHOST_SECTOR, GHOST_TAG_COUNT);

    while (true) {
        k_sleep(K_SECONDS(2));
        printk("GHOST_SUMMARY gateway=%u sector=%u unique=%u valid=%u rotations=%u rogues=%u\n",
               gateway_id, GHOST_SECTOR, unique_tags, valid_packets,
               total_rotations, rogue_packets);
    }

    return 0;
}
