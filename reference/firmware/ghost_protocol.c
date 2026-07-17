#include "ghost_protocol.h"

#include <string.h>

#define JOURNAL_MAGIC UINT32_C(0x47535452)
#define JOURNAL_COMMIT UINT32_C(0xC01117ED)
#define JOURNAL_RECORD_SIZE 40u
#define JOURNAL_BODY_SIZE 36u
#define JOURNAL_COMMIT_OFFSET 36u
#define JOURNAL_RECORDS_PER_PAGE \
    (GHOST_JOURNAL_PAGE_SIZE / JOURNAL_RECORD_SIZE)
#define GHOST_MAX_VERIFY_EPOCH UINT32_C(1000000)

struct journal_record {
    uint32_t generation;
    uint32_t resume_epoch;
    uint32_t erase_count;
    uint8_t epoch_key[16];
};

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

static void sip_round(uint64_t *v0, uint64_t *v1, uint64_t *v2, uint64_t *v3)
{
    *v0 += *v1;
    *v1 = rotate_left(*v1, 13u);
    *v1 ^= *v0;
    *v0 = rotate_left(*v0, 32u);
    *v2 += *v3;
    *v3 = rotate_left(*v3, 16u);
    *v3 ^= *v2;
    *v0 += *v3;
    *v3 = rotate_left(*v3, 21u);
    *v3 ^= *v0;
    *v2 += *v1;
    *v1 = rotate_left(*v1, 17u);
    *v1 ^= *v2;
    *v2 = rotate_left(*v2, 32u);
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
    const size_t full_bytes = length & ~(size_t)7u;

    for (size_t offset = 0; offset < full_bytes; offset += 8u) {
        const uint64_t word = read_u64_le(message + offset);
        v3 ^= word;
        sip_round(&v0, &v1, &v2, &v3);
        sip_round(&v0, &v1, &v2, &v3);
        v0 ^= word;
    }

    uint64_t tail = (uint64_t)length << 56;
    for (size_t index = 0; index < length - full_bytes; ++index) {
        tail |= (uint64_t)message[full_bytes + index] << (8u * index);
    }

    v3 ^= tail;
    sip_round(&v0, &v1, &v2, &v3);
    sip_round(&v0, &v1, &v2, &v3);
    v0 ^= tail;
    v2 ^= UINT64_C(0xff);
    sip_round(&v0, &v1, &v2, &v3);
    sip_round(&v0, &v1, &v2, &v3);
    sip_round(&v0, &v1, &v2, &v3);
    sip_round(&v0, &v1, &v2, &v3);
    return v0 ^ v1 ^ v2 ^ v3;
}

static void seed_to_key(uint64_t seed, uint8_t key[16])
{
    write_u64_le(key, seed ^ UINT64_C(0xA5A5D3C7F00DBAAD));
    write_u64_le(key + 8,
                 rotate_left(seed, 29u) ^ UINT64_C(0x6C7967656E657261));
}

static void ratchet_step(uint8_t key[16], uint32_t next_epoch)
{
    uint8_t input[6];
    uint8_t next[16];

    input[0] = 0x52u;
    write_u32_le(input + 1, next_epoch);
    input[5] = 0u;
    write_u64_le(next, ghost_siphash24(key, input, sizeof(input)));
    input[5] = 1u;
    write_u64_le(next + 8, ghost_siphash24(key, input, sizeof(input)));
    memcpy(key, next, sizeof(next));
}

void ghost_ratchet_key(uint64_t device_seed, uint32_t epoch, uint8_t key[16])
{
    seed_to_key(device_seed, key);
    for (uint32_t current = 1u; current <= epoch; ++current) {
        ratchet_step(key, current);
    }
}

static void mac_key(const uint8_t key[16], uint8_t out[16])
{
    memcpy(out, key, 16);
    out[0] ^= 0x4du;
    out[7] ^= 0x41u;
    out[8] ^= 0x43u;
    out[15] ^= 0xa7u;
}

static void build_with_epoch_key(const uint8_t epoch_key[16], uint32_t sector,
                                 uint32_t epoch, uint8_t flags,
                                 uint8_t out[GHOST_PAYLOAD_SIZE])
{
    uint8_t auth_key[16];
    uint8_t eid_input[9];

    memset(out, 0, GHOST_PAYLOAD_SIZE);
    out[0] = (uint8_t)GHOST_COMPANY_ID;
    out[1] = (uint8_t)(GHOST_COMPANY_ID >> 8);
    out[2] = GHOST_PROTOCOL_VERSION;
    out[3] = flags;
    write_u32_le(out + 4, epoch);
    write_u32_le(out + 8, sector);

    eid_input[0] = 0x45u;
    write_u32_le(eid_input + 1, epoch);
    write_u32_le(eid_input + 5, sector);
    write_u64_le(out + 12,
                 ghost_siphash24(epoch_key, eid_input, sizeof(eid_input)));

    mac_key(epoch_key, auth_key);
    write_u64_le(out + 20, ghost_siphash24(auth_key, out, 20u));
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
    uint8_t expected[GHOST_PAYLOAD_SIZE];
    uint8_t difference = 0u;
    const uint32_t epoch = read_u32_le(payload + 4);

    if (payload[0] != (uint8_t)GHOST_COMPANY_ID ||
        payload[1] != (uint8_t)(GHOST_COMPANY_ID >> 8) ||
        payload[2] != GHOST_PROTOCOL_VERSION || epoch > GHOST_MAX_VERIFY_EPOCH) {
        return false;
    }

    ghost_build_payload(device_seed, read_u32_le(payload + 8), epoch,
                        payload[3], expected);
    for (size_t index = 12; index < GHOST_PAYLOAD_SIZE; ++index) {
        difference |= expected[index] ^ payload[index];
    }
    return difference == 0u;
}

static uint32_t crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_C(0xffffffff);
    for (size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (unsigned int bit = 0; bit < 8u; ++bit) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

static bool all_erased(const uint8_t *data, size_t length)
{
    uint8_t combined = 0xffu;
    for (size_t index = 0; index < length; ++index) {
        combined &= data[index];
    }
    return combined == 0xffu;
}

static bool decode_record(const uint8_t raw[JOURNAL_RECORD_SIZE],
                          struct journal_record *record)
{
    if (read_u32_le(raw) != JOURNAL_MAGIC ||
        read_u32_le(raw + JOURNAL_COMMIT_OFFSET) != JOURNAL_COMMIT ||
        read_u32_le(raw + 32) != crc32(raw, 32u)) {
        return false;
    }
    record->generation = read_u32_le(raw + 4);
    record->resume_epoch = read_u32_le(raw + 8);
    record->erase_count = read_u32_le(raw + 12);
    memcpy(record->epoch_key, raw + 16, sizeof(record->epoch_key));
    return true;
}

static void encode_record(uint8_t raw[JOURNAL_RECORD_SIZE],
                          const struct journal_record *record)
{
    memset(raw, 0xff, JOURNAL_RECORD_SIZE);
    write_u32_le(raw, JOURNAL_MAGIC);
    write_u32_le(raw + 4, record->generation);
    write_u32_le(raw + 8, record->resume_epoch);
    write_u32_le(raw + 12, record->erase_count);
    memcpy(raw + 16, record->epoch_key, sizeof(record->epoch_key));
    write_u32_le(raw + 32, crc32(raw, 32u));
}

static int scan_journal(const struct ghost_storage *storage,
                        struct journal_record *best, bool *found,
                        uint32_t *best_offset)
{
    uint8_t raw[JOURNAL_RECORD_SIZE];
    *found = false;
    *best_offset = 0u;

    for (uint32_t page = 0; page < GHOST_JOURNAL_PAGE_COUNT; ++page) {
        for (uint32_t slot = 0; slot < JOURNAL_RECORDS_PER_PAGE; ++slot) {
            const uint32_t offset = page * GHOST_JOURNAL_PAGE_SIZE +
                                    slot * JOURNAL_RECORD_SIZE;
            struct journal_record candidate;
            if (storage->read(storage->context, offset, raw, sizeof(raw)) != 0) {
                return -1;
            }
            if (all_erased(raw, sizeof(raw))) {
                break;
            }
            if (decode_record(raw, &candidate) &&
                (!*found || candidate.generation > best->generation)) {
                *best = candidate;
                *best_offset = offset;
                *found = true;
            }
        }
    }
    return 0;
}

static int find_erased_slot(const struct ghost_storage *storage,
                            uint32_t page, uint32_t first_slot,
                            uint32_t *offset)
{
    uint8_t raw[JOURNAL_RECORD_SIZE];
    for (uint32_t slot = first_slot; slot < JOURNAL_RECORDS_PER_PAGE; ++slot) {
        const uint32_t candidate = page * GHOST_JOURNAL_PAGE_SIZE +
                                   slot * JOURNAL_RECORD_SIZE;
        if (storage->read(storage->context, candidate, raw, sizeof(raw)) != 0) {
            return -1;
        }
        if (all_erased(raw, sizeof(raw))) {
            *offset = candidate;
            return 1;
        }
    }
    return 0;
}

static int has_uncertain_record(const struct ghost_storage *storage,
                                bool found, uint32_t best_offset,
                                bool *uncertain)
{
    uint8_t raw[JOURNAL_RECORD_SIZE];
    uint32_t page = 0u;
    uint32_t first_slot = 0u;
    *uncertain = false;

    if (found) {
        page = best_offset / GHOST_JOURNAL_PAGE_SIZE;
        first_slot =
            (best_offset % GHOST_JOURNAL_PAGE_SIZE) / JOURNAL_RECORD_SIZE + 1u;
    }

    const uint32_t first_page = found ? page : 0u;
    const uint32_t last_page = found ? page + 1u : GHOST_JOURNAL_PAGE_COUNT;
    for (uint32_t current_page = first_page; current_page < last_page;
         ++current_page) {
        const uint32_t start = current_page == page ? first_slot : 0u;
        for (uint32_t slot = start; slot < JOURNAL_RECORDS_PER_PAGE; ++slot) {
            const uint32_t offset = current_page * GHOST_JOURNAL_PAGE_SIZE +
                                    slot * JOURNAL_RECORD_SIZE;
            struct journal_record ignored;
            if (storage->read(storage->context, offset, raw, sizeof(raw)) != 0) {
                return -1;
            }
            if (all_erased(raw, sizeof(raw))) {
                break;
            }
            if (!all_erased(raw, sizeof(raw)) && !decode_record(raw, &ignored)) {
                *uncertain = true;
                return 0;
            }
        }
    }

    if (found) {
        const uint32_t best_slot =
            (best_offset % GHOST_JOURNAL_PAGE_SIZE) / JOURNAL_RECORD_SIZE;
        if (best_slot + 1u == JOURNAL_RECORDS_PER_PAGE) {
            const uint32_t other_page =
                (best_offset / GHOST_JOURNAL_PAGE_SIZE + 1u) %
                GHOST_JOURNAL_PAGE_COUNT;
            struct journal_record ignored;
            if (storage->read(storage->context,
                              other_page * GHOST_JOURNAL_PAGE_SIZE, raw,
                              sizeof(raw)) != 0) {
                return -1;
            }
            if (!all_erased(raw, sizeof(raw)) &&
                !decode_record(raw, &ignored)) {
                *uncertain = true;
            }
        }
    }
    return 0;
}

static int append_record(struct ghost_state *state, uint32_t resume_epoch,
                         const uint8_t resume_key[16])
{
    struct journal_record best = {0};
    struct journal_record next;
    bool found;
    uint32_t best_offset;
    uint32_t target = 0u;
    int result = scan_journal(&state->storage, &best, &found, &best_offset);
    if (result != 0) {
        return result;
    }

    if (found) {
        const uint32_t page = best_offset / GHOST_JOURNAL_PAGE_SIZE;
        const uint32_t slot =
            (best_offset % GHOST_JOURNAL_PAGE_SIZE) / JOURNAL_RECORD_SIZE;
        result = find_erased_slot(&state->storage, page, slot + 1u, &target);
        if (result < 0) {
            return result;
        }
        if (result == 0) {
            const uint32_t other_page = (page + 1u) % GHOST_JOURNAL_PAGE_COUNT;
            target = other_page * GHOST_JOURNAL_PAGE_SIZE;
            if (state->storage.erase(state->storage.context, target,
                                     GHOST_JOURNAL_PAGE_SIZE) != 0) {
                return -1;
            }
            state->erase_count++;
        }
    } else {
        result = find_erased_slot(&state->storage, 0u, 0u, &target);
        if (result < 0) {
            return result;
        }
        if (result == 0) {
            if (state->storage.erase(state->storage.context, 0u,
                                     GHOST_JOURNAL_PAGE_SIZE) != 0) {
                return -1;
            }
            state->erase_count++;
        }
    }

    next.generation = state->generation + 1u;
    next.resume_epoch = resume_epoch;
    next.erase_count = state->erase_count;
    memcpy(next.epoch_key, resume_key, sizeof(next.epoch_key));

    uint8_t raw[JOURNAL_RECORD_SIZE];
    uint8_t commit[4];
    encode_record(raw, &next);
    write_u32_le(commit, JOURNAL_COMMIT);
    if (state->storage.write(state->storage.context, target, raw,
                             JOURNAL_BODY_SIZE) != 0) {
        return -1;
    }
    if (state->storage.write(state->storage.context,
                             target + JOURNAL_COMMIT_OFFSET, commit,
                             sizeof(commit)) != 0) {
        return -1;
    }
    state->generation = next.generation;
    return 0;
}

static int reserve_lease(struct ghost_state *state)
{
    uint8_t resume_key[16];
    uint32_t resume_epoch = state->epoch;
    memcpy(resume_key, state->epoch_key, sizeof(resume_key));

    for (uint32_t index = 0; index < GHOST_RATCHET_LEASE_EPOCHS; ++index) {
        resume_epoch++;
        ratchet_step(resume_key, resume_epoch);
    }
    if (append_record(state, resume_epoch, resume_key) != 0) {
        return -1;
    }
    state->lease_end = resume_epoch;
    return 0;
}

int ghost_state_boot(struct ghost_state *state, uint64_t device_seed,
                     uint32_t sector, const struct ghost_storage *storage)
{
    if (state == NULL || storage == NULL || storage->read == NULL ||
        storage->write == NULL || storage->erase == NULL) {
        return -1;
    }

    memset(state, 0, sizeof(*state));
    state->storage = *storage;
    state->device_seed = device_seed;
    state->sector = sector;

    struct journal_record best = {0};
    bool found;
    bool uncertain;
    uint32_t ignored_offset;
    if (scan_journal(storage, &best, &found, &ignored_offset) != 0) {
        return -1;
    }
    if (has_uncertain_record(storage, found, ignored_offset, &uncertain) != 0) {
        return -1;
    }
    if (found) {
        state->epoch = best.resume_epoch;
        state->generation = best.generation;
        state->erase_count = best.erase_count;
        memcpy(state->epoch_key, best.epoch_key, sizeof(state->epoch_key));
        state->recovered = true;
    } else {
        ghost_ratchet_key(device_seed, 0u, state->epoch_key);
    }

    if (uncertain) {
        /* One unreadable slot may have held a lease written after an earlier
         * recovery skip. Two lease widths are the conservative no-reuse
         * bound when the generation and resume epoch are both unavailable. */
        for (uint32_t index = 0;
             index < 2u * GHOST_RATCHET_LEASE_EPOCHS; ++index) {
            state->epoch++;
            ratchet_step(state->epoch_key, state->epoch);
        }
        state->recovered = true;
    }

    return reserve_lease(state);
}

int ghost_state_next_payload(struct ghost_state *state, uint8_t flags,
                             uint8_t out[GHOST_PAYLOAD_SIZE])
{
    if (state == NULL || out == NULL || state->epoch > state->lease_end) {
        return -1;
    }
    if (state->epoch == state->lease_end && reserve_lease(state) != 0) {
        return -1;
    }

    build_with_epoch_key(state->epoch_key, state->sector, state->epoch, flags,
                         out);
    state->epoch++;
    ratchet_step(state->epoch_key, state->epoch);
    return 0;
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
