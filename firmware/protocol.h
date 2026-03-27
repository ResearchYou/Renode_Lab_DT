#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/*
 * STMP v1 -- SPI Test Messaging Protocol, version 1
 *
 * Fixed 21-byte frame over SPI, full-duplex, master-initiated.
 *
 * Frame layout:
 *   [0]    MAGIC   = 0xA5            start-of-frame marker
 *   [1]    SEQ     = 1-255           per-exchange sequence number
 *   [2]    CMD                       opcode (see STMP_CMD_* below)
 *   [3]    FLAGS   = 0x00            reserved, must be zero
 *   [4:18] PAYLOAD[15]               command-specific data, zero-padded
 *   [19]   CRC8    CRC-8/SMBUS over bytes [0:18]
 *   [20]   GUARD   = 0x55            end-of-frame marker
 *
 * CRC: CRC-8/SMBUS, poly=0x07, init=0x00, no reflection, no final XOR.
 *
 * Commands (master->slave / slave->master):
 *   0x10 PING_REQ   / 0x11 PING_RSP   payload[0]=echo_seq
 *   0x20 REG_WR     / 0x21 REG_WR_ACK payload[0]=reg_id, [1:4]=val_LE / [0]=reg_id, [1]=status
 *   0x30 REG_RD     / 0x31 REG_RD_RSP payload[0]=reg_id               / [0]=reg_id, [1:4]=val_LE
 *   0x40 STAT_REQ   / 0x41 STAT_RSP   none / [0:3]=rx_count, [4:7]=tx_count, [8:11]=err_count
 *
 * Status codes: 0x00=OK 0x01=ERR_REG 0x02=ERR_CMD 0x03=ERR_CRC
 *
 * Register file: 16 x uint32_t, zero-initialised.
 *
 * Test sequence (10 exchanges, SEQ 1-10):
 *   1  PING_REQ seq=1
 *   2  PING_REQ seq=2
 *   3  REG_WR   reg=0 val=0xCAFEBABE
 *   4  REG_RD   reg=0  -> 0xCAFEBABE
 *   5  REG_WR   reg=1 val=0xDEAD1234
 *   6  REG_RD   reg=1  -> 0xDEAD1234
 *   7  STAT_REQ        -> rx=7 tx=6
 *   8  PING_REQ seq=8
 *   9  REG_RD   reg=0  -> 0xCAFEBABE (persisted)
 *  10  STAT_REQ        -> rx=10 tx=9
 */

#define STMP_MAGIC          0xA5U
#define STMP_GUARD          0x55U
#define STMP_PACKET_SIZE    21U
#define STMP_PAYLOAD_LEN    15U
#define STMP_REG_COUNT      16U
#define NUM_TEST_PACKETS    10U

#define STMP_CMD_PING_REQ   0x10U
#define STMP_CMD_PING_RSP   0x11U
#define STMP_CMD_REG_WR     0x20U
#define STMP_CMD_REG_WR_ACK 0x21U
#define STMP_CMD_REG_RD     0x30U
#define STMP_CMD_REG_RD_RSP 0x31U
#define STMP_CMD_STAT_REQ   0x40U
#define STMP_CMD_STAT_RSP   0x41U

#define STMP_STATUS_OK      0x00U
#define STMP_STATUS_ERR_REG 0x01U
#define STMP_STATUS_ERR_CMD 0x02U
#define STMP_STATUS_ERR_CRC 0x03U

typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t seq;
    uint8_t cmd;
    uint8_t flags;
    uint8_t payload[STMP_PAYLOAD_LEN];
    uint8_t crc8;
    uint8_t guard;
} stmp_packet_t;

static inline uint8_t stmp_crc8(const uint8_t *data, uint32_t len)
{
    uint8_t crc = 0x00;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x07U)
                                 : (uint8_t)(crc << 1);
    }
    return crc;
}

static inline int stmp_validate_basic(const stmp_packet_t *pkt)
{
    if (pkt->magic != STMP_MAGIC) return 0;
    if (pkt->flags != 0x00U)      return 0;
    if (pkt->guard != STMP_GUARD) return 0;
    uint8_t expected = stmp_crc8((const uint8_t*)pkt, STMP_PACKET_SIZE - 2U);
    return (pkt->crc8 == expected) ? 1 : 0;
}

static inline int stmp_validate_rsp(const stmp_packet_t *pkt,
                                    uint8_t expected_cmd, uint8_t expected_seq)
{
    if (!stmp_validate_basic(pkt)) return 0;
    if (pkt->cmd != expected_cmd)  return 0;
    if (pkt->seq != expected_seq)  return 0;
    return 1;
}

static inline void stmp_init_pkt(stmp_packet_t *pkt, uint8_t seq, uint8_t cmd)
{
    pkt->magic = STMP_MAGIC;
    pkt->seq   = seq;
    pkt->cmd   = cmd;
    pkt->flags = 0x00U;
    for (uint8_t i = 0; i < STMP_PAYLOAD_LEN; i++) pkt->payload[i] = 0x00U;
    pkt->crc8  = 0x00U;
    pkt->guard = STMP_GUARD;
}

static inline void stmp_finalize(stmp_packet_t *pkt)
{
    pkt->crc8 = stmp_crc8((const uint8_t*)pkt, STMP_PACKET_SIZE - 2U);
}

static inline uint32_t stmp_get_u32le(const uint8_t *b)
{
    return (uint32_t)b[0]          | ((uint32_t)b[1] <<  8U) |
           ((uint32_t)b[2] << 16U) | ((uint32_t)b[3] << 24U);
}
static inline void stmp_put_u32le(uint8_t *b, uint32_t v)
{
    b[0] = (uint8_t)(v);
    b[1] = (uint8_t)(v >>  8U);
    b[2] = (uint8_t)(v >> 16U);
    b[3] = (uint8_t)(v >> 24U);
}

#endif /* PROTOCOL_H */
