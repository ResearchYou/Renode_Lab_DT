#ifndef TOKEN_PROTOCOL_H
#define TOKEN_PROTOCOL_H

#include <stdint.h>

#define TOKEN_REPORT_SIZE       64U
#define TOKEN_CHALLENGE_SIZE    32U
#define TOKEN_MAC_SIZE          32U

#define TOKEN_MAGIC0            0x59U
#define TOKEN_MAGIC1            0x4BU
#define TOKEN_VERSION           0x01U
#define TOKEN_GUARD             0x7EU

#define TOKEN_CMD_GET_INFO      0x01U
#define TOKEN_CMD_AUTH          0x02U
#define TOKEN_CMD_HMAC_SHA1     0x03U

#define TOKEN_STATUS_OK         0x00U
#define TOKEN_STATUS_ERR_CRC    0x01U
#define TOKEN_STATUS_ERR_CMD    0x02U
#define TOKEN_STATUS_ERR_TOUCH  0x03U
#define TOKEN_STATUS_ERR_REPLAY 0x04U

typedef struct __attribute__((packed)) {
    uint8_t magic0;
    uint8_t magic1;
    uint8_t version;
    uint8_t seq;
    uint8_t cmd;
    uint8_t flags;
    uint32_t nonce;
    uint8_t challenge[TOKEN_CHALLENGE_SIZE];
    uint8_t reserved[20];
    uint8_t crc8;
    uint8_t guard;
} token_request_t;

typedef struct __attribute__((packed)) {
    uint8_t magic0;
    uint8_t magic1;
    uint8_t version;
    uint8_t seq;
    uint8_t status;
    uint8_t flags;
    uint32_t counter;
    uint8_t mac[TOKEN_MAC_SIZE];
    uint8_t reserved[20];
    uint8_t crc8;
    uint8_t guard;
} token_response_t;

static inline uint8_t token_crc8(const uint8_t *data, uint32_t len)
{
    uint8_t crc = 0x00U;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint32_t bit = 0; bit < 8U; bit++) {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x07U)
                                : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static inline int token_validate_request(const token_request_t *req)
{
    if (req->magic0 != TOKEN_MAGIC0 || req->magic1 != TOKEN_MAGIC1) return 0;
    if (req->version != TOKEN_VERSION) return 0;
    if (req->guard != TOKEN_GUARD) return 0;
    return req->crc8 == token_crc8((const uint8_t *)req, TOKEN_REPORT_SIZE - 2U);
}

static inline void token_response_init(token_response_t *rsp, uint8_t seq,
                                       uint8_t status, uint32_t counter)
{
    uint8_t *raw = (uint8_t *)rsp;
    for (uint32_t i = 0; i < TOKEN_REPORT_SIZE; i++) {
        raw[i] = 0U;
    }
    rsp->magic0 = TOKEN_MAGIC0;
    rsp->magic1 = TOKEN_MAGIC1;
    rsp->version = TOKEN_VERSION;
    rsp->seq = seq;
    rsp->status = status;
    rsp->counter = counter;
    rsp->guard = TOKEN_GUARD;
}

static inline void token_response_finalize(token_response_t *rsp)
{
    rsp->crc8 = token_crc8((const uint8_t *)rsp, TOKEN_REPORT_SIZE - 2U);
}

#endif /* TOKEN_PROTOCOL_H */
