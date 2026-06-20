#include <stdint.h>
#include "stm32f4.h"
#include "token_protocol.h"

static uint32_t auth_counter;

static const uint8_t rfc2202_key_1[20] = {
    0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
    0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
    0x0b, 0x0b, 0x0b, 0x0b,
};

static const uint8_t rfc2202_msg_1[] = {
    'H', 'i', ' ', 'T', 'h', 'e', 'r', 'e',
};

static const uint8_t rfc2202_key_2[] = {
    'J', 'e', 'f', 'e',
};

static const uint8_t rfc2202_msg_2[] = {
    'w', 'h', 'a', 't', ' ', 'd', 'o', ' ', 'y', 'a', ' ',
    'w', 'a', 'n', 't', ' ', 'f', 'o', 'r', ' ', 'n', 'o',
    't', 'h', 'i', 'n', 'g', '?',
};

static const uint8_t rfc2202_key_3[20] = {
    0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
    0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
    0xaa, 0xaa, 0xaa, 0xaa,
};

static const uint8_t rfc2202_msg_3[50] = {
    0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
    0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
    0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
    0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
    0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
    0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd,
    0xdd, 0xdd,
};

typedef struct {
    const uint8_t *key;
    uint32_t key_len;
    const uint8_t *message;
    uint32_t message_len;
} hmac_sha1_vector_t;

static const hmac_sha1_vector_t hmac_sha1_vectors[] = {
    { rfc2202_key_1, sizeof(rfc2202_key_1), rfc2202_msg_1, sizeof(rfc2202_msg_1) },
    { rfc2202_key_2, sizeof(rfc2202_key_2), rfc2202_msg_2, sizeof(rfc2202_msg_2) },
    { rfc2202_key_3, sizeof(rfc2202_key_3), rfc2202_msg_3, sizeof(rfc2202_msg_3) },
};

static void uart_init(void)
{
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC_APB2ENR |= RCC_APB2ENR_USART1EN;

    /* PA9 = USART1_TX, AF7. Renode also observes USART writes directly. */
    GPIOA_MODER = (GPIOA_MODER & ~(3U << 18)) | (2U << 18);
    GPIOA_AFRH = (GPIOA_AFRH & ~(0xFU << 4)) | (7U << 4);

    USART1_BRR = 139U;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void uart_putc(char c)
{
    volatile uint32_t timeout = 200000U;
    while (!(USART1_SR & USART_SR_TXE) && --timeout) {
    }
    USART1_DR = (uint32_t)(uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

static void uart_uint(uint32_t value)
{
    char buf[11];
    uint32_t index = 0;
    if (value == 0U) {
        uart_putc('0');
        return;
    }
    while (value != 0U && index < sizeof(buf)) {
        buf[index++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (index != 0U) {
        uart_putc(buf[--index]);
    }
}

static void uart_hex_bytes(const uint8_t *data, uint32_t length)
{
    static const char hex[] = "0123456789ABCDEF";
    for (uint32_t i = 0; i < length; i++) {
        uart_putc(hex[data[i] >> 4]);
        uart_putc(hex[data[i] & 0x0FU]);
    }
}

static void copy_from_words(uint8_t *dst)
{
    for (uint32_t i = 0; i < TOKEN_REPORT_SIZE / 4U; i++) {
        uint32_t word = MOCK_OUT_WORD(i);
        dst[(i * 4U) + 0U] = (uint8_t)(word);
        dst[(i * 4U) + 1U] = (uint8_t)(word >> 8);
        dst[(i * 4U) + 2U] = (uint8_t)(word >> 16);
        dst[(i * 4U) + 3U] = (uint8_t)(word >> 24);
    }
}

static void copy_to_words(const uint8_t *src)
{
    for (uint32_t i = 0; i < TOKEN_REPORT_SIZE / 4U; i++) {
        uint32_t word = ((uint32_t)src[(i * 4U) + 0U]) |
                        ((uint32_t)src[(i * 4U) + 1U] << 8) |
                        ((uint32_t)src[(i * 4U) + 2U] << 16) |
                        ((uint32_t)src[(i * 4U) + 3U] << 24);
        MOCK_IN_WORD(i) = word;
    }
}

static uint32_t rotate_left(uint32_t value, uint32_t bits)
{
    return (value << bits) | (value >> (32U - bits));
}

static uint32_t load_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           ((uint32_t)data[3]);
}

static void store_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static void sha1_compress(uint32_t state[5], const uint8_t block[64])
{
    uint32_t w[80];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;

    for (uint32_t i = 0; i < 16U; i++) {
        w[i] = load_be32(&block[i * 4U]);
    }
    for (uint32_t i = 16U; i < 80U; i++) {
        w[i] = rotate_left(w[i - 3U] ^ w[i - 8U] ^ w[i - 14U] ^ w[i - 16U], 1U);
    }

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    for (uint32_t i = 0; i < 80U; i++) {
        uint32_t f;
        uint32_t k;
        uint32_t temp;
        if (i < 20U) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999U;
        } else if (i < 40U) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1U;
        } else if (i < 60U) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCU;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6U;
        }
        temp = rotate_left(a, 5U) + f + e + k + w[i];
        e = d;
        d = c;
        c = rotate_left(b, 30U);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

static void sha1_hash(const uint8_t *message, uint32_t message_len,
                      uint8_t digest[20])
{
    uint32_t state[5] = {
        0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U,
    };
    uint8_t block[64];
    uint32_t offset = 0;

    while (message_len - offset >= 64U) {
        sha1_compress(state, &message[offset]);
        offset += 64U;
    }

    uint32_t remaining = message_len - offset;
    for (uint32_t i = 0; i < 64U; i++) {
        block[i] = 0U;
    }
    for (uint32_t i = 0; i < remaining; i++) {
        block[i] = message[offset + i];
    }
    block[remaining] = 0x80U;

    if (remaining >= 56U) {
        sha1_compress(state, block);
        for (uint32_t i = 0; i < 64U; i++) {
            block[i] = 0U;
        }
    }

    uint32_t bit_len_low = message_len << 3;
    block[60] = (uint8_t)(bit_len_low >> 24);
    block[61] = (uint8_t)(bit_len_low >> 16);
    block[62] = (uint8_t)(bit_len_low >> 8);
    block[63] = (uint8_t)bit_len_low;
    sha1_compress(state, block);

    for (uint32_t i = 0; i < 5U; i++) {
        store_be32(&digest[i * 4U], state[i]);
    }
}

static void hmac_sha1(const uint8_t *key, uint32_t key_len,
                      const uint8_t *message, uint32_t message_len,
                      uint8_t digest[20])
{
    uint8_t key_block[64];
    uint8_t inner_block[64 + 50];
    uint8_t outer_block[64 + 20];
    uint8_t inner_digest[20];

    for (uint32_t i = 0; i < 64U; i++) {
        key_block[i] = 0U;
    }

    if (key_len > 64U) {
        sha1_hash(key, key_len, key_block);
    } else {
        for (uint32_t i = 0; i < key_len; i++) {
            key_block[i] = key[i];
        }
    }

    for (uint32_t i = 0; i < 64U; i++) {
        inner_block[i] = key_block[i] ^ 0x36U;
        outer_block[i] = key_block[i] ^ 0x5cU;
    }
    for (uint32_t i = 0; i < message_len; i++) {
        inner_block[64U + i] = message[i];
    }

    sha1_hash(inner_block, 64U + message_len, inner_digest);

    for (uint32_t i = 0; i < 20U; i++) {
        outer_block[64U + i] = inner_digest[i];
    }
    sha1_hash(outer_block, sizeof(outer_block), digest);
}

static void handle_get_info(const token_request_t *req, token_response_t *rsp)
{
    token_response_init(rsp, req->seq, TOKEN_STATUS_OK, auth_counter);
    rsp->mac[0] = 'Y';
    rsp->mac[1] = 'K';
    rsp->mac[2] = 'M';
    rsp->mac[3] = 'O';
    rsp->mac[4] = 'C';
    rsp->mac[5] = 'K';
    rsp->mac[6] = '1';
    rsp->mac[8] = TOKEN_CMD_GET_INFO;
    rsp->mac[9] = TOKEN_CMD_HMAC_SHA1;
    token_response_finalize(rsp);

    uart_puts("TOKEN: GET_INFO seq=");
    uart_uint(req->seq);
    uart_puts(" status=OK\r\n");
}

static void handle_hmac_sha1(const token_request_t *req, token_response_t *rsp)
{
    uint8_t vector_index = req->challenge[0];

    if (vector_index >= (sizeof(hmac_sha1_vectors) / sizeof(hmac_sha1_vectors[0]))) {
        token_response_init(rsp, req->seq, TOKEN_STATUS_ERR_CMD, auth_counter);
        token_response_finalize(rsp);
        uart_puts("TOKEN: HMAC_SHA1 vector=");
        uart_uint(vector_index);
        uart_puts(" status=ERR_CMD\r\n");
        return;
    }

    token_response_init(rsp, req->seq, TOKEN_STATUS_OK, auth_counter);
    hmac_sha1(hmac_sha1_vectors[vector_index].key,
              hmac_sha1_vectors[vector_index].key_len,
              hmac_sha1_vectors[vector_index].message,
              hmac_sha1_vectors[vector_index].message_len,
              rsp->mac);
    token_response_finalize(rsp);

    uart_puts("TOKEN: HMAC_SHA1 vector=");
    uart_uint((uint32_t)vector_index + 1U);
    uart_puts(" status=OK digest=");
    uart_hex_bytes(rsp->mac, 20U);
    uart_puts("\r\n");
}

static void process_report(void)
{
    uint8_t request_bytes[TOKEN_REPORT_SIZE];
    token_response_t rsp;
    token_request_t *req = (token_request_t *)request_bytes;

    copy_from_words(request_bytes);
    MOCK_CONTROL = MOCK_CONTROL_ACK_OUT;

    if (!token_validate_request(req)) {
        token_response_init(&rsp, req->seq, TOKEN_STATUS_ERR_CRC, auth_counter);
        token_response_finalize(&rsp);
        uart_puts("TOKEN: BAD_CRC\r\n");
    } else if (req->cmd == TOKEN_CMD_GET_INFO) {
        handle_get_info(req, &rsp);
    } else if (req->cmd == TOKEN_CMD_HMAC_SHA1) {
        handle_hmac_sha1(req, &rsp);
    } else {
        token_response_init(&rsp, req->seq, TOKEN_STATUS_ERR_CMD, auth_counter);
        token_response_finalize(&rsp);
        uart_puts("TOKEN: UNKNOWN_CMD\r\n");
    }

    copy_to_words((const uint8_t *)&rsp);
    MOCK_CONTROL = MOCK_CONTROL_SEND_IN;
}

int main(void)
{
    uart_init();

    uart_puts("TOKEN: boot HMAC-SHA1 functional validation\r\n");
    uart_puts("TOKEN: RFC2202 fixed vectors loaded\r\n");

    while (1) {
        uint32_t status = MOCK_STATUS;
        if (status & MOCK_STATUS_OUT_READY) {
            process_report();
        }
        if (status & MOCK_STATUS_DONE) {
            uart_puts("TOKEN: SCRIPT COMPLETE\r\n");
            break;
        }
    }

    while (1) {
    }
}
