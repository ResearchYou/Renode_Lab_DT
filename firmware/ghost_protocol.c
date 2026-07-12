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

static void seed_to_key(uint64_t seed, uint8_t key[16])
{
    write_u64_le(key, seed ^ UINT64_C(0xA5A5D3C7F00DBAAD));
    write_u64_le(key + 8,
                 ((seed << 29) | (seed >> 35)) ^
                     UINT64_C(0x6C7967656E657261));
}

uint64_t ghost_siphash24(const uint8_t key[16], const uint8_t *message,
                         size_t length)
{
    /*
     * TODO 1: implement SipHash-2-4.
     *
     * Use the canonical 128-bit-key / 64-bit-output construction and keep all
     * byte decoding little-endian. The tests include the authors' known-answer
     * vectors, so an almost-correct round function will not pass.
     */
    (void)key;
    (void)message;
    (void)length;
    return 0u;
}

void ghost_build_payload(uint64_t device_seed, uint32_t sector, uint32_t epoch,
                         uint8_t flags, uint8_t out[GHOST_PAYLOAD_SIZE])
{
    uint8_t key[16];
    uint8_t eid_input[9];

    memset(out, 0, GHOST_PAYLOAD_SIZE);
    out[0] = (uint8_t)GHOST_COMPANY_ID;
    out[1] = (uint8_t)(GHOST_COMPANY_ID >> 8);
    out[2] = GHOST_PROTOCOL_VERSION;
    out[3] = flags;
    write_u32_le(out + 4, epoch);
    write_u32_le(out + 8, sector);
    seed_to_key(device_seed, key);

    eid_input[0] = 0x45u; /* domain separation: ephemeral identity */
    write_u32_le(eid_input + 1, epoch);
    write_u32_le(eid_input + 5, sector);

    /*
     * TODO 2: derive and write the ephemeral ID at bytes 12..19, then derive
     * the authentication tag over bytes 0..19 using a distinct MAC domain.
     * A correct packet never transmits device_seed itself.
     */
    (void)eid_input;
    (void)key;
}

bool ghost_verify_payload(uint64_t device_seed,
                          const uint8_t payload[GHOST_PAYLOAD_SIZE])
{
    /*
     * TODO 3: reject malformed headers, recompute the expected ephemeral ID
     * and authentication tag, and compare both with the packet.
     */
    (void)device_seed;
    (void)payload;
    return false;
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
