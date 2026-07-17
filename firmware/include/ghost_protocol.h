#ifndef GHOST_PROTOCOL_H
#define GHOST_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GHOST_COMPANY_ID 0xF00Du
#define GHOST_PROTOCOL_VERSION 3u
#define GHOST_PAYLOAD_SIZE 28u
#define GHOST_FLAG_DISTRESS 0x01u

#define GHOST_RATCHET_LEASE_EPOCHS 16u
#define GHOST_JOURNAL_PAGE_SIZE 4096u
#define GHOST_JOURNAL_PAGE_COUNT 2u
#define GHOST_JOURNAL_SIZE \
    (GHOST_JOURNAL_PAGE_SIZE * GHOST_JOURNAL_PAGE_COUNT)
#define GHOST_ENERGY_WRITE_UNITS 8u
#define GHOST_ENERGY_ERASE_UNITS 40u
#define GHOST_ENERGY_ADVERTISEMENT_UNITS 1u
#define GHOST_RUNTIME_ENERGY_BUDGET 80u

/*
 * Wire layout (little-endian integers):
 *   0..1   company ID
 *   2      protocol version
 *   3      flags
 *   4..7   persistent ratchet epoch
 *   8..11  public sector ID
 *   12..19 keyed ephemeral ID
 *   20..27 keyed authentication tag over bytes 0..19
 */

struct ghost_storage {
    void *context;
    int (*read)(void *context, uint32_t offset, void *data, size_t length);
    int (*write)(void *context, uint32_t offset, const void *data,
                 size_t length);
    int (*erase)(void *context, uint32_t offset, size_t length);
};

struct ghost_state {
    struct ghost_storage storage;
    uint64_t device_seed;
    uint32_t sector;
    uint32_t epoch;
    uint32_t lease_end;
    uint32_t generation;
    uint32_t erase_count;
    uint8_t epoch_key[16];
    bool recovered;
};

uint64_t ghost_siphash24(const uint8_t key[16], const uint8_t *message,
                         size_t length);

void ghost_ratchet_key(uint64_t device_seed, uint32_t epoch,
                       uint8_t key[16]);

void ghost_build_payload(uint64_t device_seed, uint32_t sector, uint32_t epoch,
                         uint8_t flags, uint8_t out[GHOST_PAYLOAD_SIZE]);

bool ghost_verify_payload(uint64_t device_seed,
                          const uint8_t payload[GHOST_PAYLOAD_SIZE]);

int ghost_state_boot(struct ghost_state *state, uint64_t device_seed,
                     uint32_t sector, const struct ghost_storage *storage);

int ghost_state_next_payload(struct ghost_state *state, uint8_t flags,
                             uint8_t out[GHOST_PAYLOAD_SIZE]);

uint32_t ghost_state_energy_units(const struct ghost_state *state,
                                  uint32_t advertisements);

uint32_t ghost_payload_epoch(const uint8_t payload[GHOST_PAYLOAD_SIZE]);
uint32_t ghost_payload_sector(const uint8_t payload[GHOST_PAYLOAD_SIZE]);
uint64_t ghost_payload_eid(const uint8_t payload[GHOST_PAYLOAD_SIZE]);

#endif
