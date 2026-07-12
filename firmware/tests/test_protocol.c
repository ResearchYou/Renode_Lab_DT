#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ghost_protocol.h"

static int failures;

static void check(bool condition, const char *label)
{
    if (condition) {
        printf("[PASS] %s\n", label);
    } else {
        printf("[FAIL] %s\n", label);
        failures++;
    }
}

int main(void)
{
    static const uint64_t vectors[] = {
        UINT64_C(0x726fdb47dd0e0e31), UINT64_C(0x74f839c593dc67fd),
        UINT64_C(0x0d6c8009d9a94f5a), UINT64_C(0x85676696d7fb7e2d),
        UINT64_C(0xcf2794e0277187b7), UINT64_C(0x18765564cd99a68d),
        UINT64_C(0xcbc9466e58fee3ce), UINT64_C(0xab0200f58b01d137),
    };
    uint8_t key[16];
    uint8_t message[64];

    for (size_t i = 0; i < sizeof(key); ++i) {
        key[i] = (uint8_t)i;
    }
    for (size_t i = 0; i < sizeof(message); ++i) {
        message[i] = (uint8_t)i;
    }
    for (size_t length = 0; length < sizeof(vectors) / sizeof(vectors[0]);
         ++length) {
        char label[64];
        snprintf(label, sizeof(label), "SipHash-2-4 vector length=%zu", length);
        check(ghost_siphash24(key, message, length) == vectors[length], label);
    }

    uint8_t first[GHOST_PAYLOAD_SIZE];
    uint8_t rotated[GHOST_PAYLOAD_SIZE];
    const uint64_t seed = UINT64_C(0x0123456789abcdef);

    ghost_build_payload(seed, 7u, 41u, GHOST_FLAG_DISTRESS, first);
    ghost_build_payload(seed, 7u, 42u, GHOST_FLAG_DISTRESS, rotated);

    check(ghost_verify_payload(seed, first), "valid authenticated payload");
    check(ghost_payload_epoch(first) == 41u, "epoch encoding");
    check(ghost_payload_sector(first) == 7u, "sector encoding");
    check(ghost_payload_eid(first) != ghost_payload_eid(rotated),
          "ephemeral ID rotates with epoch");

    uint8_t tampered[GHOST_PAYLOAD_SIZE];
    memcpy(tampered, first, sizeof(tampered));
    tampered[3] ^= GHOST_FLAG_DISTRESS;
    check(!ghost_verify_payload(seed, tampered), "tampered flags are rejected");
    check(!ghost_verify_payload(seed ^ 1u, first), "wrong fleet key is rejected");

    bool seed_leaked = false;
    for (size_t i = 0; i + sizeof(seed) <= sizeof(first); ++i) {
        if (memcmp(first + i, &seed, sizeof(seed)) == 0) {
            seed_leaked = true;
        }
    }
    check(!seed_leaked, "stable device seed is absent from packet");

    printf("PROTOCOL_TESTS failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
