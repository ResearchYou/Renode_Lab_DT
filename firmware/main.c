#include <stdint.h>
#include "stm32f4.h"
#include "token_protocol.h"

static const uint8_t resident_secret[TOKEN_MAC_SIZE] = {
    0x8d, 0x34, 0x77, 0x10, 0x5a, 0xc9, 0x11, 0x42,
    0xa1, 0xe0, 0x63, 0x2f, 0x9b, 0xd2, 0x7c, 0x08,
    0x44, 0x91, 0xb6, 0x35, 0xef, 0x20, 0x19, 0xaa,
    0x6c, 0xde, 0x03, 0x57, 0x88, 0x14, 0xf1, 0x2b,
};

static uint32_t auth_counter;
static uint32_t last_nonce;
static uint8_t last_challenge[TOKEN_CHALLENGE_SIZE];
static uint8_t last_seq;
static int have_last_auth;

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

static void uart_hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc(hex[(value >> (uint32_t)shift) & 0xFU]);
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

static int same_challenge(const uint8_t *a, const uint8_t *b)
{
    uint8_t diff = 0U;
    for (uint32_t i = 0; i < TOKEN_CHALLENGE_SIZE; i++) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }
    return diff == 0U;
}

static int is_replay_buggy(const token_request_t *req)
{
    if (!have_last_auth) {
        return 0;
    }

    /*
     * Intentional challenge bug:
     * replay protection incorrectly includes the transport sequence number.
     * A captured request replayed with a new HID sequence is accepted.
     */
    return req->nonce == last_nonce &&
           req->seq == last_seq &&
           same_challenge(req->challenge, last_challenge);
}

static void remember_auth(const token_request_t *req)
{
    last_nonce = req->nonce;
    last_seq = req->seq;
    for (uint32_t i = 0; i < TOKEN_CHALLENGE_SIZE; i++) {
        last_challenge[i] = req->challenge[i];
    }
    have_last_auth = 1;
}

static uint32_t rotate_left(uint32_t value, uint32_t bits)
{
    return (value << bits) | (value >> (32U - bits));
}

static void make_lab_mac(const token_request_t *req, uint32_t counter,
                         uint8_t *mac)
{
    uint32_t state[8] = {
        0x243F6A88U, 0x85A308D3U, 0x13198A2EU, 0x03707344U,
        0xA4093822U, 0x299F31D0U, 0x082EFA98U, 0xEC4E6C89U,
    };

    for (uint32_t i = 0; i < 8U; i++) {
        uint32_t secret_word = ((uint32_t)resident_secret[(i * 4U) + 0U]) |
                               ((uint32_t)resident_secret[(i * 4U) + 1U] << 8) |
                               ((uint32_t)resident_secret[(i * 4U) + 2U] << 16) |
                               ((uint32_t)resident_secret[(i * 4U) + 3U] << 24);
        state[i] ^= secret_word ^ req->nonce ^ counter;
    }

    for (uint32_t round = 0; round < 4U; round++) {
        for (uint32_t i = 0; i < TOKEN_CHALLENGE_SIZE; i++) {
            uint32_t lane = i & 7U;
            state[lane] += req->challenge[i] + resident_secret[(i + round) & 31U];
            state[lane] = rotate_left(state[lane], ((i + round) & 7U) + 3U);
            state[lane] ^= state[(lane + 1U) & 7U] + 0x9E3779B9U + round;
        }
    }

    for (uint32_t i = 0; i < 8U; i++) {
        mac[(i * 4U) + 0U] = (uint8_t)(state[i]);
        mac[(i * 4U) + 1U] = (uint8_t)(state[i] >> 8);
        mac[(i * 4U) + 2U] = (uint8_t)(state[i] >> 16);
        mac[(i * 4U) + 3U] = (uint8_t)(state[i] >> 24);
    }
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
    rsp->mac[9] = TOKEN_CMD_AUTH;
    token_response_finalize(rsp);

    uart_puts("TOKEN: GET_INFO seq=");
    uart_uint(req->seq);
    uart_puts(" status=OK\r\n");
}

static void handle_auth(const token_request_t *req, token_response_t *rsp,
                        uint32_t status)
{
    int touch = (status & MOCK_STATUS_TOUCH_PRESENT) != 0U;
    uint8_t rsp_status = TOKEN_STATUS_OK;

    if (!touch) {
        rsp_status = TOKEN_STATUS_ERR_TOUCH;
    } else if (is_replay_buggy(req)) {
        rsp_status = TOKEN_STATUS_ERR_REPLAY;
    }

    if (rsp_status == TOKEN_STATUS_OK) {
        auth_counter++;
        token_response_init(rsp, req->seq, TOKEN_STATUS_OK, auth_counter);
        make_lab_mac(req, auth_counter, rsp->mac);
        remember_auth(req);
    } else {
        token_response_init(rsp, req->seq, rsp_status, auth_counter);
    }
    token_response_finalize(rsp);

    uart_puts("TOKEN: AUTH seq=");
    uart_uint(req->seq);
    uart_puts(" touch=");
    uart_uint((uint32_t)touch);
    uart_puts(" nonce=");
    uart_hex32(req->nonce);
    uart_puts(" status=");
    if (rsp_status == TOKEN_STATUS_OK) {
        uart_puts("OK counter=");
        uart_uint(auth_counter);
    } else if (rsp_status == TOKEN_STATUS_ERR_REPLAY) {
        uart_puts("ERR_REPLAY");
    } else {
        uart_puts("ERR_TOUCH");
    }
    uart_puts("\r\n");
}

static void process_report(void)
{
    uint8_t request_bytes[TOKEN_REPORT_SIZE];
    token_response_t rsp;
    token_request_t *req = (token_request_t *)request_bytes;
    uint32_t status = MOCK_STATUS;

    copy_from_words(request_bytes);
    MOCK_CONTROL = MOCK_CONTROL_ACK_OUT;

    if (!token_validate_request(req)) {
        token_response_init(&rsp, req->seq, TOKEN_STATUS_ERR_CRC, auth_counter);
        token_response_finalize(&rsp);
        uart_puts("TOKEN: BAD_CRC\r\n");
    } else if (req->cmd == TOKEN_CMD_GET_INFO) {
        handle_get_info(req, &rsp);
    } else if (req->cmd == TOKEN_CMD_AUTH) {
        handle_auth(req, &rsp, status);
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

    uart_puts("TOKEN: boot YK-MOCK challenge-response\r\n");
    uart_puts("TOKEN: resident secret loaded, touch-required auth enabled\r\n");

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
