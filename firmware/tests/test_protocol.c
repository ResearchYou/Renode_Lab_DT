#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ghost_protocol.h"

static int failures;

struct fake_flash {
    uint8_t bytes[GHOST_JOURNAL_SIZE];
    uint32_t writes;
    uint32_t erases;
    uint32_t fail_write;
};

static void check(bool condition, const char *label)
{
    printf("[%s] %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition) {
        failures++;
    }
}

static int fake_read(void *context, uint32_t offset, void *data, size_t length)
{
    struct fake_flash *flash = context;
    if ((size_t)offset + length > sizeof(flash->bytes)) {
        return -1;
    }
    memcpy(data, flash->bytes + offset, length);
    return 0;
}

static int fake_write(void *context, uint32_t offset, const void *data,
                      size_t length)
{
    struct fake_flash *flash = context;
    const uint8_t *source = data;
    if ((size_t)offset + length > sizeof(flash->bytes)) {
        return -1;
    }
    flash->writes++;
    size_t written = length;
    if (flash->fail_write == flash->writes) {
        written = length / 2u;
    }
    for (size_t index = 0; index < written; ++index) {
        flash->bytes[offset + index] &= source[index];
    }
    return written == length ? 0 : -1;
}

static int fake_erase(void *context, uint32_t offset, size_t length)
{
    struct fake_flash *flash = context;
    if ((size_t)offset + length > sizeof(flash->bytes)) {
        return -1;
    }
    memset(flash->bytes + offset, 0xff, length);
    flash->erases++;
    return 0;
}

static struct ghost_storage storage_for(struct fake_flash *flash)
{
    const struct ghost_storage storage = {
        .context = flash,
        .read = fake_read,
        .write = fake_write,
        .erase = fake_erase,
    };
    return storage;
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
    for (size_t index = 0; index < sizeof(key); ++index) {
        key[index] = (uint8_t)index;
    }
    for (size_t index = 0; index < sizeof(message); ++index) {
        message[index] = (uint8_t)index;
    }
    for (size_t length = 0; length < 8u; ++length) {
        char label[64];
        snprintf(label, sizeof(label), "SipHash-2-4 vector length=%zu", length);
        check(ghost_siphash24(key, message, length) == vectors[length], label);
    }

    static const uint8_t epoch16_key[16] = {
        0x2f, 0x2d, 0x37, 0x62, 0x77, 0xc7, 0x12, 0x83,
        0xd3, 0x8c, 0x7c, 0x5b, 0xf0, 0xed, 0x5f, 0x11,
    };
    const uint64_t seed = UINT64_C(0x0123456789abcdef);
    ghost_ratchet_key(seed, 16u, key);
    check(memcmp(key, epoch16_key, sizeof(key)) == 0,
          "ratchet reaches the epoch-16 known answer");

    uint8_t first[GHOST_PAYLOAD_SIZE];
    uint8_t rotated[GHOST_PAYLOAD_SIZE];
    ghost_build_payload(seed, 7u, 0u, GHOST_FLAG_DISTRESS, first);
    ghost_build_payload(seed, 7u, 1u, GHOST_FLAG_DISTRESS, rotated);
    check(ghost_verify_payload(seed, first), "valid v3 payload is accepted");
    check(ghost_payload_eid(first) != ghost_payload_eid(rotated),
          "ratcheted EID changes every epoch");
    first[3] ^= GHOST_FLAG_DISTRESS;
    check(!ghost_verify_payload(seed, first), "tampered flags are rejected");

    struct fake_flash flash;
    memset(&flash, 0, sizeof(flash));
    memset(flash.bytes, 0xff, sizeof(flash.bytes));
    struct ghost_storage storage = storage_for(&flash);
    struct ghost_state state;
    int status = ghost_state_boot(&state, seed, 7u, &storage);
    check(status == 0, "fresh state reserves a durable epoch lease");
    uint32_t before_cut = 0u;
    if (status == 0) {
        for (uint32_t index = 0; index < 4u; ++index) {
            status = ghost_state_next_payload(&state, 0u, rotated);
            if (status == 0) {
                before_cut = ghost_payload_epoch(rotated);
            }
        }
    }
    check(status == 0 && before_cut == 3u,
          "RAM advances inside one reserved lease");

    struct ghost_state recovered;
    status = ghost_state_boot(&recovered, seed, 7u, &storage);
    check(status == 0 && recovered.recovered,
          "reboot recovers a committed journal record");
    if (status == 0) {
        status = ghost_state_next_payload(&recovered, 0u, rotated);
    }
    check(status == 0 && ghost_payload_epoch(rotated) >= 16u,
          "power cut skips every possibly used epoch");
    check(status == 0 && ghost_verify_payload(seed, rotated),
          "gateway can verify the recovered ratchet epoch");
    check(status == 0 &&
              ghost_state_energy_units(&recovered, 2u) <=
                  GHOST_RUNTIME_ENERGY_BUDGET,
          "runtime flash and advertising energy stays in budget");

    struct fake_flash endurance;
    memset(&endurance, 0, sizeof(endurance));
    memset(endurance.bytes, 0xff, sizeof(endurance.bytes));
    storage = storage_for(&endurance);
    status = ghost_state_boot(&state, seed, 7u, &storage);
    for (uint32_t index = 0; status == 0 && index < 256u; ++index) {
        status = ghost_state_next_payload(&state, 0u, rotated);
    }
    check(status == 0 && endurance.writes <= 32u,
          "256 epochs use at most 32 flash writes");
    check(status == 0 && endurance.erases == 0u,
          "256 epochs do not erase a journal page");

    printf("PROTOCOL_TESTS failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
