#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ghost_protocol.h"

_Static_assert(GHOST_COMPANY_ID == 0xF00Du,
               "company id is part of the wire contract");
_Static_assert(GHOST_PROTOCOL_VERSION == 3u, "protocol version is fixed");
_Static_assert(GHOST_PAYLOAD_SIZE == 28u, "BLE payload budget is fixed");
_Static_assert(GHOST_FLAG_DISTRESS == 0x01u, "distress flag is fixed");
_Static_assert(GHOST_RATCHET_LEASE_EPOCHS == 16u,
               "persistent leases contain 16 epochs");
_Static_assert(GHOST_JOURNAL_PAGE_COUNT == 2u,
               "the journal alternates between two pages");

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
    0x0d, 0xf0, 0x03, 0x01, 0x29, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x00, 0x00, 0xd2, 0xb4, 0xd1, 0x35,
    0xac, 0xc4, 0xfa, 0xfa, 0x90, 0xb8, 0x08, 0xd5,
    0x21, 0xf7, 0x6c, 0xa7,
};

static const uint8_t epoch16_key[16] = {
    0x2f, 0x2d, 0x37, 0x62, 0x77, 0xc7, 0x12, 0x83,
    0xd3, 0x8c, 0x7c, 0x5b, 0xf0, 0xed, 0x5f, 0x11,
};

struct fake_flash {
    uint8_t bytes[GHOST_JOURNAL_SIZE];
    uint32_t writes;
    uint32_t erases;
    uint32_t body_writes;
    uint32_t commit_writes;
    uint32_t fail_write;
    uint32_t last_body_offset;
};

static void check(bool condition, const char *label)
{
    printf("[%s] %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition) {
        failures++;
    }
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
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

static void mac_key(const uint8_t epoch_key[16], uint8_t out[16])
{
    memcpy(out, epoch_key, 16u);
    out[0] ^= 0x4du;
    out[7] ^= 0x41u;
    out[8] ^= 0x43u;
    out[15] ^= 0xa7u;
}

static void resign_payload(uint64_t seed,
                           uint8_t payload[GHOST_PAYLOAD_SIZE])
{
    uint8_t key[16];
    uint8_t auth_key[16];
    ghost_ratchet_key(seed, read_u32_le(payload + 4), key);
    mac_key(key, auth_key);
    write_u64_le(payload + 20, ghost_siphash24(auth_key, payload, 20u));
}

static bool contains_bytes(const uint8_t *haystack, size_t haystack_length,
                           const uint8_t *needle, size_t needle_length)
{
    if (needle_length > haystack_length) {
        return false;
    }
    for (size_t index = 0; index + needle_length <= haystack_length;
         ++index) {
        if (memcmp(haystack + index, needle, needle_length) == 0) {
            return true;
        }
    }
    return false;
}

static void fake_flash_init(struct fake_flash *flash)
{
    memset(flash, 0, sizeof(*flash));
    memset(flash->bytes, 0xff, sizeof(flash->bytes));
    flash->last_body_offset = UINT32_MAX;
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
    if (length == 36u) {
        flash->body_writes++;
        flash->last_body_offset = offset;
    } else if (length == 4u) {
        flash->commit_writes++;
    }

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
    if ((size_t)offset + length > sizeof(flash->bytes) ||
        offset % GHOST_JOURNAL_PAGE_SIZE != 0u ||
        length != GHOST_JOURNAL_PAGE_SIZE) {
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

static uint32_t next_epoch(struct ghost_state *state, uint64_t seed)
{
    uint8_t payload[GHOST_PAYLOAD_SIZE];
    if (ghost_state_next_payload(state, 0u, payload) != 0 ||
        !ghost_verify_payload(seed, payload)) {
        return UINT32_MAX;
    }
    return ghost_payload_epoch(payload);
}

static void test_siphash_and_wire_contract(void)
{
    uint8_t key[16];
    uint8_t message[64];
    for (size_t index = 0; index < sizeof(key); ++index) {
        key[index] = (uint8_t)index;
    }
    for (size_t index = 0; index < sizeof(message); ++index) {
        message[index] = (uint8_t)index;
    }

    for (size_t length = 0; length < 64u; ++length) {
        char label[64];
        snprintf(label, sizeof(label), "SipHash-2-4 vector length=%zu",
                 length);
        check(ghost_siphash24(key, message, length) ==
                  siphash24_vectors[length],
              label);
    }

    const uint64_t seed = UINT64_C(0x0123456789abcdef);
    uint8_t ratcheted[16];
    uint8_t previous[16];
    ghost_ratchet_key(seed, 15u, previous);
    ghost_ratchet_key(seed, 16u, ratcheted);
    check(memcmp(ratcheted, epoch16_key, sizeof(ratcheted)) == 0,
          "ratchet reaches the epoch-16 known answer");
    check(memcmp(ratcheted, previous, sizeof(ratcheted)) != 0,
          "ratchet changes the complete key schedule by epoch");

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
          "complete 28-byte v3 payload contract vector");
    check(memcmp(first, repeated, sizeof(first)) == 0,
          "payload generation is deterministic");
    check(first[0] == 0x0du && first[1] == 0xf0u,
          "company id is encoded little-endian");
    check(first[2] == GHOST_PROTOCOL_VERSION, "protocol version is encoded");
    check(first[3] == GHOST_FLAG_DISTRESS, "flags are encoded");
    check(ghost_payload_epoch(first) == 41u, "epoch is encoded little-endian");
    check(ghost_payload_sector(first) == 7u,
          "sector is encoded little-endian");
    check(ghost_verify_payload(seed, first), "valid payload is accepted");
    check(ghost_payload_eid(first) != ghost_payload_eid(rotated),
          "ephemeral id rotates with the ratcheted epoch");
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

    for (size_t index = 0; index < GHOST_PAYLOAD_SIZE; ++index) {
        uint8_t tampered[GHOST_PAYLOAD_SIZE];
        char label[64];
        memcpy(tampered, first, sizeof(tampered));
        tampered[index] ^= 0x01u;
        snprintf(label, sizeof(label), "tamper at byte %zu is rejected", index);
        check(!ghost_verify_payload(seed, tampered), label);
    }

    uint8_t forged[GHOST_PAYLOAD_SIZE];
    memcpy(forged, first, sizeof(forged));
    forged[0] ^= 0x01u;
    resign_payload(seed, forged);
    check(!ghost_verify_payload(seed, forged),
          "re-signed wrong company id is rejected");

    memcpy(forged, first, sizeof(forged));
    forged[2] = 2u;
    resign_payload(seed, forged);
    check(!ghost_verify_payload(seed, forged),
          "re-signed wrong protocol version is rejected");

    memcpy(forged, first, sizeof(forged));
    forged[12] ^= 0x80u;
    resign_payload(seed, forged);
    check(!ghost_verify_payload(seed, forged),
          "re-signed wrong ephemeral id is rejected");

    memcpy(forged, first, sizeof(forged));
    write_u32_le(forged + 4, UINT32_C(1000001));
    check(!ghost_verify_payload(seed, forged),
          "unreasonable ratchet epoch is rejected before derivation");

    uint8_t seed_little_endian[8];
    uint8_t seed_big_endian[8];
    write_u64_le(seed_little_endian, seed);
    for (size_t index = 0; index < sizeof(seed_big_endian); ++index) {
        seed_big_endian[index] = seed_little_endian[7u - index];
    }
    check(!contains_bytes(first, sizeof(first), seed_little_endian,
                          sizeof(seed_little_endian)),
          "stable seed is absent in little-endian form");
    check(!contains_bytes(first, sizeof(first), seed_big_endian,
                          sizeof(seed_big_endian)),
          "stable seed is absent in big-endian form");
}

static void test_normal_recovery_and_budget(void)
{
    const uint64_t seed = UINT64_C(0x0123456789abcdef);
    struct fake_flash flash;
    fake_flash_init(&flash);
    struct ghost_storage storage = storage_for(&flash);
    struct ghost_state state;

    int status = ghost_state_boot(&state, seed, 7u, &storage);
    check(status == 0 && !state.recovered,
          "fresh boot reserves state without claiming recovery");
    check(status == 0 && state.epoch == 0u && state.lease_end == 16u,
          "fresh state durably reserves epochs 0 through 15");
    check(flash.body_writes == 1u && flash.commit_writes == 1u,
          "journal body is followed by a distinct commit write");

    uint32_t before_cut = UINT32_MAX;
    for (uint32_t index = 0; status == 0 && index < 4u; ++index) {
        before_cut = next_epoch(&state, seed);
        status = before_cut == UINT32_MAX ? -1 : 0;
    }
    check(status == 0 && before_cut == 3u,
          "RAM advances inside the reserved lease");

    struct ghost_state recovered;
    status = ghost_state_boot(&recovered, seed, 7u, &storage);
    check(status == 0 && recovered.recovered,
          "reboot recovers the newest committed record");
    const uint32_t after_cut = status == 0 ? next_epoch(&recovered, seed) : 0u;
    check(status == 0 && after_cut != UINT32_MAX &&
              after_cut >= GHOST_RATCHET_LEASE_EPOCHS,
          "power cut skips every possibly emitted epoch");
    check(after_cut > before_cut, "reboot never reuses an emitted epoch");
    check(status == 0 &&
              ghost_state_energy_units(&recovered, 8u) <=
                  GHOST_RUNTIME_ENERGY_BUDGET,
          "recovery and advertisements stay in the runtime energy budget");
}

static void test_torn_write_recovery(uint32_t failing_write_delta,
                                     const char *failure_label,
                                     const char *recovery_label)
{
    const uint64_t seed = UINT64_C(0x0123456789abcdef);
    struct fake_flash flash;
    fake_flash_init(&flash);
    struct ghost_storage storage = storage_for(&flash);
    struct ghost_state state;
    int status = ghost_state_boot(&state, seed, 7u, &storage);
    const uint32_t emitted = status == 0 ? next_epoch(&state, seed) : UINT32_MAX;

    flash.fail_write = flash.writes + failing_write_delta;
    struct ghost_state interrupted;
    status = ghost_state_boot(&interrupted, seed, 7u, &storage);
    check(status != 0, failure_label);

    flash.fail_write = 0u;
    struct ghost_state recovered;
    status = ghost_state_boot(&recovered, seed, 7u, &storage);
    const uint32_t epoch = status == 0 ? next_epoch(&recovered, seed) : 0u;
    check(status == 0 && recovered.recovered && epoch != UINT32_MAX &&
              epoch >= 48u,
          recovery_label);
    check(status == 0 && epoch > emitted,
          "torn journal record cannot cause epoch reuse");
}

static void test_corrupt_committed_record(void)
{
    const uint64_t seed = UINT64_C(0x0123456789abcdef);
    struct fake_flash flash;
    fake_flash_init(&flash);
    struct ghost_storage storage = storage_for(&flash);
    struct ghost_state first;
    struct ghost_state second;
    int status = ghost_state_boot(&first, seed, 7u, &storage);
    status = status == 0 ? ghost_state_boot(&second, seed, 7u, &storage) : status;
    const uint32_t emitted = status == 0 ? next_epoch(&second, seed) : UINT32_MAX;

    check(status == 0 && flash.last_body_offset != UINT32_MAX,
          "test fixture located the newest committed record");
    if (status == 0 && flash.last_body_offset != UINT32_MAX) {
        flash.bytes[flash.last_body_offset + 16u] ^= 0x01u;
    }

    struct ghost_state recovered;
    status = ghost_state_boot(&recovered, seed, 7u, &storage);
    const uint32_t epoch = status == 0 ? next_epoch(&recovered, seed) : 0u;
    check(status == 0 && recovered.recovered && epoch != UINT32_MAX &&
              epoch >= 48u,
          "CRC corruption falls back and conservatively skips two leases");
    check(status == 0 && epoch > emitted,
          "corrupted newest record cannot cause epoch reuse");
}

static int fill_first_journal_page(struct fake_flash *flash,
                                   struct ghost_state *state, uint64_t seed)
{
    struct ghost_storage storage = storage_for(flash);
    int status = ghost_state_boot(state, seed, 7u, &storage);
    for (uint32_t index = 0; status == 0 && index <= 1616u; ++index) {
        status = next_epoch(state, seed) == UINT32_MAX ? -1 : 0;
    }
    return status;
}

static void test_rollover_failure_recovery(void)
{
    const uint64_t seed = UINT64_C(0x0123456789abcdef);
    struct fake_flash torn;
    fake_flash_init(&torn);
    struct ghost_state state;
    int status = fill_first_journal_page(&torn, &state, seed);
    check(status == 0 && torn.body_writes == 102u,
          "fixture fills the first journal page exactly");

    struct ghost_storage storage = storage_for(&torn);
    torn.fail_write = torn.writes + 1u;
    struct ghost_state interrupted;
    status = ghost_state_boot(&interrupted, seed, 7u, &storage);
    check(status != 0, "torn body at page rollover aborts reservation");
    torn.fail_write = 0u;

    struct ghost_state recovered;
    status = ghost_state_boot(&recovered, seed, 7u, &storage);
    const uint32_t epoch = status == 0 ? next_epoch(&recovered, seed) : 0u;
    check(status == 0 && epoch != UINT32_MAX && epoch >= 1664u,
          "torn first record on the other page cannot reuse a lease");

    struct fake_flash corrupt;
    fake_flash_init(&corrupt);
    status = fill_first_journal_page(&corrupt, &state, seed);
    storage = storage_for(&corrupt);
    struct ghost_state crossed;
    status = status == 0 ? ghost_state_boot(&crossed, seed, 7u, &storage)
                         : status;
    const uint32_t emitted = status == 0 ? next_epoch(&crossed, seed)
                                         : UINT32_MAX;
    check(status == 0 && corrupt.last_body_offset == GHOST_JOURNAL_PAGE_SIZE,
          "page rollover commits first record on the other page");
    if (status == 0) {
        corrupt.bytes[corrupt.last_body_offset + 16u] ^= 0x01u;
    }

    status = ghost_state_boot(&recovered, seed, 7u, &storage);
    const uint32_t after_corruption =
        status == 0 ? next_epoch(&recovered, seed) : 0u;
    check(status == 0 && after_corruption != UINT32_MAX &&
              after_corruption >= 1664u && after_corruption > emitted,
          "corrupted rollover record falls back without epoch reuse");
}

static void test_endurance(void)
{
    const uint64_t seed = UINT64_C(0x0123456789abcdef);
    struct fake_flash flash;
    fake_flash_init(&flash);
    struct ghost_storage storage = storage_for(&flash);
    struct ghost_state state;
    int status = ghost_state_boot(&state, seed, 7u, &storage);
    uint32_t last = UINT32_MAX;
    for (uint32_t index = 0; status == 0 && index < 2000u; ++index) {
        last = next_epoch(&state, seed);
        status = last == UINT32_MAX ? -1 : 0;
    }

    check(status == 0 && last == 1999u,
          "2000 sequential payloads preserve exact ratchet ordering");
    check(status == 0 && flash.writes <= 252u,
          "2000 epochs need at most 252 flash writes");
    check(status == 0 && flash.erases == 1u,
          "two-page journal needs one erase across 2000 epochs");
    check(status == 0 && flash.body_writes == flash.commit_writes,
          "every durable record has exactly one final commit");
}

int main(void)
{
    test_siphash_and_wire_contract();
    test_normal_recovery_and_budget();
    test_torn_write_recovery(1u, "torn journal body aborts reservation",
                             "torn body is ignored with conservative recovery");
    test_torn_write_recovery(2u, "torn commit aborts reservation",
                             "missing commit is ignored with conservative recovery");
    test_corrupt_committed_record();
    test_rollover_failure_recovery();
    test_endurance();

    printf("PROTOCOL_CONTRACT_TESTS failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
