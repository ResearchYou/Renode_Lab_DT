#include "ghost_protocol.h"

#include <string.h>

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t read_u64_le(const uint8_t *p)
{
    return (uint64_t)read_u32_le(p) | ((uint64_t)read_u32_le(p + 4) << 32);
}

static void write_u32_le(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void write_u64_le(uint8_t *p, uint64_t value)
{
    write_u32_le(p, (uint32_t)value);
    write_u32_le(p + 4, (uint32_t)(value >> 32));
}

static uint64_t rotate_left(uint64_t value, unsigned int shift)
{
    return (value << shift) | (value >> (64u - shift));
}

static void seed_to_key(uint64_t seed, uint8_t key[16])
{
    write_u64_le(key, seed ^ UINT64_C(0xA5A5D3C7F00DBAAD));
    write_u64_le(key + 8,
                 rotate_left(seed, 29u) ^ UINT64_C(0x6C7967656E657261));
}

uint64_t ghost_siphash24(const uint8_t key[16], const uint8_t *message,
                         size_t length)
{
    /*
     * TODO 1: implement canonical SipHash-2-4 with little-endian message
     * words. The hidden suite includes all 64 authors' vectors.
     */
    (void)key;
    (void)message;
    (void)length;
    return 0u;
}

static void ratchet_step(uint8_t key[16], uint32_t next_epoch)
{
    /*
     * TODO 2: derive both halves of the next key with SipHash. Use domain
     * byte 0x52, little-endian next_epoch, and lane bytes 0 and 1.
     */
    (void)key;
    (void)next_epoch;
}

void ghost_ratchet_key(uint64_t device_seed, uint32_t epoch, uint8_t key[16])
{
    seed_to_key(device_seed, key);
    for (uint32_t current = 1u; current <= epoch; ++current) {
        ratchet_step(key, current);
    }
}

static void build_with_epoch_key(const uint8_t epoch_key[16], uint32_t sector,
                                 uint32_t epoch, uint8_t flags,
                                 uint8_t out[GHOST_PAYLOAD_SIZE])
{
    /*
     * TODO 3: write the v3 header, derive the EID with domain 0x45, then
     * authenticate bytes 0..19 with a domain-separated MAC key.
     */
    (void)epoch_key;
    memset(out, 0, GHOST_PAYLOAD_SIZE);
    out[0] = (uint8_t)GHOST_COMPANY_ID;
    out[1] = (uint8_t)(GHOST_COMPANY_ID >> 8);
    out[2] = GHOST_PROTOCOL_VERSION;
    out[3] = flags;
    write_u32_le(out + 4, epoch);
    write_u32_le(out + 8, sector);
}

void ghost_build_payload(uint64_t device_seed, uint32_t sector, uint32_t epoch,
                         uint8_t flags, uint8_t out[GHOST_PAYLOAD_SIZE])
{
    uint8_t key[16];
    ghost_ratchet_key(device_seed, epoch, key);
    build_with_epoch_key(key, sector, epoch, flags, out);
}

bool ghost_verify_payload(uint64_t device_seed,
                          const uint8_t payload[GHOST_PAYLOAD_SIZE])
{
    /*
     * TODO 4: reject malformed headers and unreasonable epochs, then compare
     * the expected EID and MAC without an early-exit timing leak.
     */
    (void)device_seed;
    (void)payload;
    return false;
}

int ghost_state_boot(struct ghost_state *state, uint64_t device_seed,
                     uint32_t sector, const struct ghost_storage *storage)
{
    /*
     * TODO 5: recover the newest committed CRC-protected journal record from
     * either flash page. Reserve the next 16-epoch lease before returning.
     * A torn body or missing final commit word must never become valid state.
     */
    (void)state;
    (void)device_seed;
    (void)sector;
    (void)storage;
    return -1;
}

int ghost_state_next_payload(struct ghost_state *state, uint8_t flags,
                             uint8_t out[GHOST_PAYLOAD_SIZE])
{
    /*
     * TODO 6: reserve another lease before exhaustion, emit the current
     * ratcheted payload, then advance RAM state exactly once.
     */
    (void)state;
    (void)flags;
    (void)out;
    return -1;
}

uint32_t ghost_state_energy_units(const struct ghost_state *state,
                                  uint32_t advertisements)
{
    if (state == NULL) {
        return UINT32_MAX;
    }
    return state->generation * 2u * GHOST_ENERGY_WRITE_UNITS +
           state->erase_count * GHOST_ENERGY_ERASE_UNITS +
           advertisements * GHOST_ENERGY_ADVERTISEMENT_UNITS;
}

uint32_t ghost_payload_epoch(const uint8_t payload[GHOST_PAYLOAD_SIZE])
{
    return read_u32_le(payload + 4);
}

uint32_t ghost_payload_sector(const uint8_t payload[GHOST_PAYLOAD_SIZE])
{
    return read_u32_le(payload + 8);
}

uint64_t ghost_payload_eid(const uint8_t payload[GHOST_PAYLOAD_SIZE])
{
    return read_u64_le(payload + 12);
}
