#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ghost_protocol.h"

_Static_assert(GHOST_COMPANY_ID == 0xF00Du, "company id is part of the wire contract");
_Static_assert(GHOST_PROTOCOL_VERSION == 2u, "protocol version is fixed");
_Static_assert(GHOST_PAYLOAD_SIZE == 28u, "legacy BLE payload budget is fixed");
_Static_assert(GHOST_FLAG_DISTRESS == 0x01u, "distress flag is fixed");

static int failures;

static const uint64_t siphash24_vectors[64] = {
    UINT64_C(0x726fdb47dd0e0e31), UINT64_C(0x74f839c593dc67fd),
    UINT64_C(0x0d6c8009d9a94f5a), UINT64_C(0x85676696d7fb7e2d),
    UINT64_C(0xcf2794e0277187b7), UINT64_C(0x18765564cd99a68d),
    UINT64_C(0xcbc9466e58fee3ce), UINT64_C(0xab0200f58b01d137),
    UINT64_C(0x93f5f5799a932462), UINT64_C(0x9e0082df0ba9e4b0),
    UINT64_C(0x7a5dbbc594ddb9f3), UINT64_C(0xf4b32f46226bada7),
    UINT64_C(0x751e8fbc860ee5fb), UINT64_C(0x14ea5627c0843d90),
    UINT64_C(0xf723ca908e7af2ee), UINT64_C(0xa129ca6149be45e5),
    UINT64_C(0x3f2acc7f57c29bdb), UINT64_C(0x699ae9f52cbe4794),
    UINT64_C(0x4bc1b3f0968dd39c), UINT64_C(0xbb6dc91da77961bd),
    UINT64_C(0xbed65cf21aa2ee98), UINT64_C(0xd0f2cbb02e3b67c7),
    UINT64_C(0x93536795e3a33e88), UINT64_C(0xa80c038ccd5ccec8),
    UINT64_C(0xb8ad50c6f649af94), UINT64_C(0xbce192de8a85b8ea),
    UINT64_C(0x17d835b85bbb15f3), UINT64_C(0x2f2e6163076bcfad),
    UINT64_C(0xde4daaaca71dc9a5), UINT64_C(0xa6a2506687956571),
    UINT64_C(0xad87a3535c49ef28), UINT64_C(0x32d892fad841c342),
    UINT64_C(0x7127512f72f27cce), UINT64_C(0xa7f32346f95978e3),
    UINT64_C(0x12e0b01abb051238), UINT64_C(0x15e034d40fa197ae),
    UINT64_C(0x314dffbe0815a3b4), UINT64_C(0x027990f029623981),
    UINT64_C(0xcadcd4e59ef40c4d), UINT64_C(0x9abfd8766a33735c),
    UINT64_C(0x0e3ea96b5304a7d0), UINT64_C(0xad0c42d6fc585992),
    UINT64_C(0x187306c89bc215a9), UINT64_C(0xd4a60abcf3792b95),
    UINT64_C(0xf935451de4f21df2), UINT64_C(0xa9538f0419755787),
    UINT64_C(0xdb9acddff56ca510), UINT64_C(0xd06c98cd5c0975eb),
    UINT64_C(0xe612a3cb9ecba951), UINT64_C(0xc766e62cfcadaf96),
    UINT64_C(0xee64435a9752fe72), UINT64_C(0xa192d576b245165a),
    UINT64_C(0x0a8787bf8ecb74b2), UINT64_C(0x81b3e73d20b49b6f),
    UINT64_C(0x7fa8220ba3b2ecea), UINT64_C(0x245731c13ca42499),
    UINT64_C(0xb78dbfaf3a8d83bd), UINT64_C(0xea1ad565322a1a0b),
    UINT64_C(0x60e61c23a3795013), UINT64_C(0x6606d7e446282b93),
    UINT64_C(0x6ca4ecb15c5f91e1), UINT64_C(0x9f626da15c9625f3),
    UINT64_C(0xe51b38608ef25f57), UINT64_C(0x958a324ceb064572),
};

static const uint8_t payload_vector[GHOST_PAYLOAD_SIZE] = {
    0x0d, 0xf0, 0x02, 0x01, 0x29, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x00, 0x00, 0x34, 0xe3, 0xb5, 0xf5,
    0x71, 0x83, 0x9e, 0x98, 0x26, 0x8c, 0x44, 0x07,
    0x8d, 0x9f, 0x69, 0x9d,
};

static void check(bool condition, const char *label)
{
    if (condition) {
        printf("[PASS] %s\n", label);
    } else {
        printf("[FAIL] %s\n", label);
        failures++;
    }
}

static uint64_t rotate_left_64(uint64_t value, unsigned int bits)
{
    return (value << bits) | (value >> (64u - bits));
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

static void derive_mac_key(uint64_t seed, uint8_t key[16])
{
    write_u64_le(key, seed ^ UINT64_C(0xA5A5D3C7F00DBAAD));
    write_u64_le(key + 8,
                 rotate_left_64(seed, 29) ^ UINT64_C(0x6C7967656E657261));
    key[0] ^= 0x4du;
    key[7] ^= 0x41u;
    key[8] ^= 0x43u;
    key[15] ^= 0xa7u;
}

static void resign_payload(uint64_t seed, uint8_t payload[GHOST_PAYLOAD_SIZE])
{
    uint8_t key[16];
    derive_mac_key(seed, key);
    write_u64_le(payload + 20, ghost_siphash24(key, payload, 20));
}

static bool contains_bytes(const uint8_t *haystack, size_t haystack_length,
                           const uint8_t *needle, size_t needle_length)
{
    if (needle_length > haystack_length) {
        return false;
    }
    for (size_t i = 0; i + needle_length <= haystack_length; ++i) {
        if (memcmp(haystack + i, needle, needle_length) == 0) {
            return true;
        }
    }
    return false;
}

int main(void)
{
    uint8_t key[16];
    uint8_t message[64];
    for (size_t i = 0; i < sizeof(key); ++i) {
        key[i] = (uint8_t)i;
    }
    for (size_t i = 0; i < sizeof(message); ++i) {
        message[i] = (uint8_t)i;
    }

    for (size_t length = 0; length < 64; ++length) {
        char label[64];
        snprintf(label, sizeof(label), "SipHash-2-4 vector length=%zu", length);
        check(ghost_siphash24(key, message, length) == siphash24_vectors[length],
              label);
    }

    const uint64_t seed = UINT64_C(0x0123456789abcdef);
    uint8_t first[GHOST_PAYLOAD_SIZE];
    uint8_t repeated[GHOST_PAYLOAD_SIZE];
    uint8_t rotated[GHOST_PAYLOAD_SIZE];
    uint8_t other_sector[GHOST_PAYLOAD_SIZE];
    uint8_t other_seed[GHOST_PAYLOAD_SIZE];

    ghost_build_payload(seed, 7u, 41u, GHOST_FLAG_DISTRESS, first);
    ghost_build_payload(seed, 7u, 41u, GHOST_FLAG_DISTRESS, repeated);
    ghost_build_payload(seed, 7u, 42u, GHOST_FLAG_DISTRESS, rotated);
    ghost_build_payload(seed, 8u, 41u, GHOST_FLAG_DISTRESS, other_sector);
    ghost_build_payload(seed ^ 1u, 7u, 41u, GHOST_FLAG_DISTRESS, other_seed);

    check(memcmp(first, payload_vector, sizeof(first)) == 0,
          "complete 28-byte payload contract vector");
    check(memcmp(first, repeated, sizeof(first)) == 0,
          "payload generation is deterministic");
    check(first[0] == 0x0du && first[1] == 0xf0u,
          "company id is encoded little-endian");
    check(first[2] == 2u, "protocol version is encoded");
    check(first[3] == GHOST_FLAG_DISTRESS, "flags are encoded");
    check(ghost_payload_epoch(first) == 41u, "epoch is encoded little-endian");
    check(ghost_payload_sector(first) == 7u, "sector is encoded little-endian");
    check(ghost_verify_payload(seed, first), "valid payload is accepted");
    check(ghost_payload_eid(first) != ghost_payload_eid(rotated),
          "ephemeral id rotates with epoch");
    check(ghost_payload_eid(first) != ghost_payload_eid(other_sector),
          "ephemeral id is bound to sector");
    check(ghost_payload_eid(first) != ghost_payload_eid(other_seed),
          "ephemeral id is bound to device seed");
    check(!ghost_verify_payload(seed ^ 1u, first),
          "wrong fleet seed is rejected");

    uint8_t preserved[GHOST_PAYLOAD_SIZE];
    memcpy(preserved, first, sizeof(preserved));
    (void)ghost_verify_payload(seed, first);
    check(memcmp(first, preserved, sizeof(first)) == 0,
          "verification does not modify the packet");

    for (size_t i = 0; i < GHOST_PAYLOAD_SIZE; ++i) {
        uint8_t tampered[GHOST_PAYLOAD_SIZE];
        char label[64];
        memcpy(tampered, first, sizeof(tampered));
        tampered[i] ^= 0x01u;
        snprintf(label, sizeof(label), "tamper at byte %zu is rejected", i);
        check(!ghost_verify_payload(seed, tampered), label);
    }

    uint8_t forged[GHOST_PAYLOAD_SIZE];
    memcpy(forged, first, sizeof(forged));
    forged[0] ^= 0x01u;
    resign_payload(seed, forged);
    check(!ghost_verify_payload(seed, forged),
          "re-signed wrong company id is rejected");

    memcpy(forged, first, sizeof(forged));
    forged[2] = 3u;
    resign_payload(seed, forged);
    check(!ghost_verify_payload(seed, forged),
          "re-signed wrong protocol version is rejected");

    memcpy(forged, first, sizeof(forged));
    forged[12] ^= 0x80u;
    resign_payload(seed, forged);
    check(!ghost_verify_payload(seed, forged),
          "re-signed wrong ephemeral id is rejected");

    memcpy(forged, first, sizeof(forged));
    forged[8] ^= 0x01u;
    resign_payload(seed, forged);
    check(!ghost_verify_payload(seed, forged),
          "re-signed sector with stale ephemeral id is rejected");

    uint8_t seed_little_endian[8];
    uint8_t seed_big_endian[8];
    write_u64_le(seed_little_endian, seed);
    for (size_t i = 0; i < sizeof(seed_big_endian); ++i) {
        seed_big_endian[i] = seed_little_endian[7u - i];
    }
    check(!contains_bytes(first, sizeof(first), seed_little_endian,
                          sizeof(seed_little_endian)),
          "stable seed is absent in little-endian form");
    check(!contains_bytes(first, sizeof(first), seed_big_endian,
                          sizeof(seed_big_endian)),
          "stable seed is absent in big-endian form");

    printf("PROTOCOL_CONTRACT_TESTS failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
