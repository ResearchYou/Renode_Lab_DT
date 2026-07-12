#ifndef GHOST_PROTOCOL_H
#define GHOST_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GHOST_COMPANY_ID 0xF00Du
#define GHOST_PROTOCOL_VERSION 2u
#define GHOST_PAYLOAD_SIZE 28u
#define GHOST_FLAG_DISTRESS 0x01u

/*
 * Wire layout (little-endian integers):
 *   0..1   company ID
 *   2      protocol version
 *   3      flags
 *   4..7   rotating epoch
 *   8..11  public sector ID
 *   12..19 keyed ephemeral ID
 *   20..27 keyed authentication tag over bytes 0..19
 *
 * A stable tag ID is deliberately absent from the radio packet.
 */

uint64_t ghost_siphash24(const uint8_t key[16], const uint8_t *message,
                         size_t length);

void ghost_build_payload(uint64_t device_seed, uint32_t sector, uint32_t epoch,
                         uint8_t flags, uint8_t out[GHOST_PAYLOAD_SIZE]);

bool ghost_verify_payload(uint64_t device_seed,
                          const uint8_t payload[GHOST_PAYLOAD_SIZE]);

uint32_t ghost_payload_epoch(const uint8_t payload[GHOST_PAYLOAD_SIZE]);
uint32_t ghost_payload_sector(const uint8_t payload[GHOST_PAYLOAD_SIZE]);
uint64_t ghost_payload_eid(const uint8_t payload[GHOST_PAYLOAD_SIZE]);

#endif
