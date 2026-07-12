#include "ghost_protocol.h"

#include <string.h>

#define ROTL64(value, bits) (((value) << (bits)) | ((value) >> (64u - (bits))))
#define SIPROUND()                                                            \
    do {                                                                      \
        v0 += v1;                                                             \
        v1 = ROTL64(v1, 13);                                                  \
        v1 ^= v0;                                                             \
        v0 = ROTL64(v0, 32);                                                  \
        v2 += v3;                                                             \
        v3 = ROTL64(v3, 16);                                                  \
        v3 ^= v2;                                                             \
        v0 += v3;                                                             \
        v3 = ROTL64(v3, 21);                                                  \
        v3 ^= v0;                                                             \
        v2 += v1;                                                             \
        v1 = ROTL64(v1, 17);                                                  \
        v1 ^= v2;                                                             \
        v2 = ROTL64(v2, 32);                                                  \
    } while (0)

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
                 ROTL64(seed, 29) ^ UINT64_C(0x6C7967656E657261));
}

static void mac_key(const uint8_t key[16], uint8_t out[16])
{
    memcpy(out, key, 16);
    out[0] ^= 0x4du;
    out[7] ^= 0x41u;
    out[8] ^= 0x43u;
    out[15] ^= 0xa7u;
}

uint64_t ghost_siphash24(const uint8_t key[16], const uint8_t *message,
                         size_t length)
{
    const uint64_t k0 = read_u64_le(key);
    const uint64_t k1 = read_u64_le(key + 8);
    uint64_t v0 = UINT64_C(0x736f6d6570736575) ^ k0;
    uint64_t v1 = UINT64_C(0x646f72616e646f6d) ^ k1;
    uint64_t v2 = UINT64_C(0x6c7967656e657261) ^ k0;
    uint64_t v3 = UINT64_C(0x7465646279746573) ^ k1;
    const uint8_t *end = message + (length - (length % 8u));
    uint64_t tail = (uint64_t)length << 56;

    for (const uint8_t *p = message; p != end; p += 8) {
        uint64_t block = read_u64_le(p);
        v3 ^= block;
        SIPROUND();
        SIPROUND();
        v0 ^= block;
    }

    switch (length & 7u) {
    case 7: tail |= (uint64_t)end[6] << 48; /* fall through */
    case 6: tail |= (uint64_t)end[5] << 40; /* fall through */
    case 5: tail |= (uint64_t)end[4] << 32; /* fall through */
    case 4: tail |= (uint64_t)end[3] << 24; /* fall through */
    case 3: tail |= (uint64_t)end[2] << 16; /* fall through */
    case 2: tail |= (uint64_t)end[1] << 8;  /* fall through */
    case 1: tail |= (uint64_t)end[0];       /* fall through */
    case 0: break;
    }

    v3 ^= tail;
    SIPROUND();
    SIPROUND();
    v0 ^= tail;
    v2 ^= 0xffu;
    SIPROUND();
    SIPROUND();
    SIPROUND();
    SIPROUND();
    return v0 ^ v1 ^ v2 ^ v3;
}

void ghost_build_payload(uint64_t device_seed, uint32_t sector, uint32_t epoch,
                         uint8_t flags, uint8_t out[GHOST_PAYLOAD_SIZE])
{
    uint8_t key[16];
    uint8_t auth_key[16];
    uint8_t eid_input[9];

    out[0] = (uint8_t)GHOST_COMPANY_ID;
    out[1] = (uint8_t)(GHOST_COMPANY_ID >> 8);
    out[2] = GHOST_PROTOCOL_VERSION;
    out[3] = flags;
    write_u32_le(out + 4, epoch);
    write_u32_le(out + 8, sector);

    seed_to_key(device_seed, key);
    eid_input[0] = 0x45u;
    write_u32_le(eid_input + 1, epoch);
    write_u32_le(eid_input + 5, sector);
    write_u64_le(out + 12, ghost_siphash24(key, eid_input, sizeof(eid_input)));

    mac_key(key, auth_key);
    write_u64_le(out + 20, ghost_siphash24(auth_key, out, 20));
}

bool ghost_verify_payload(uint64_t device_seed,
                          const uint8_t payload[GHOST_PAYLOAD_SIZE])
{
    if (payload[0] != (uint8_t)GHOST_COMPANY_ID ||
        payload[1] != (uint8_t)(GHOST_COMPANY_ID >> 8) ||
        payload[2] != GHOST_PROTOCOL_VERSION) {
        return false;
    }

    uint8_t expected[GHOST_PAYLOAD_SIZE];
    ghost_build_payload(device_seed, ghost_payload_sector(payload),
                        ghost_payload_epoch(payload), payload[3], expected);

    uint8_t difference = 0u;
    for (size_t i = 12; i < GHOST_PAYLOAD_SIZE; ++i) {
        difference |= expected[i] ^ payload[i];
    }
    return difference == 0u;
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
