/*
 * STM32F746 dual-board STMP v1 protocol test
 *
 * Role is detected from the first word of RAM (0x20000000), written by
 * the Renode script before execution starts:
 *   0x00000001 = MASTER
 *   0x00000000 = SLAVE
 *
 * The same ELF is loaded on both boards.
 *
 * Master flow (10 exchanges):
 *   TX  -- send 21-byte STMP request via SPI polling
 *   GAP -- busy-wait ~12 ms so slave ISR can execute
 *   RX  -- send 21 dummy bytes, receive 21-byte STMP response
 *   VAL -- validate seq, cmd, CRC8, guard, and payload
 *
 * Slave flow:
 *   Configures bridge controller at 0x50000000 with DMA buffer addresses.
 *   DMA2_Stream2_IRQHandler fires when master sends a packet.
 *   ISR decodes command, updates register file / counters, builds response.
 */

#include <stdint.h>
#include <string.h>
#include "stm32f746.h"
#include "protocol.h"

/* ---- Role flag (NOLOAD, set by Renode before CPU start) -------------- */
__attribute__((section(".noinit"))) volatile uint32_t g_role_flag;
#define ROLE_MASTER 1U
#define ROLE_SLAVE  0U

/* ---- DMA/exchange buffers (4-byte aligned) --------------------------- */
static uint8_t dma_tx_buf[STMP_PACKET_SIZE] __attribute__((aligned(4)));
static uint8_t dma_rx_buf[STMP_PACKET_SIZE] __attribute__((aligned(4)));

/* ---- Slave-side buffers (bridge controller DMA target/source) -------- */
static uint8_t slave_rx_buf[STMP_PACKET_SIZE] __attribute__((aligned(4)));
static uint8_t slave_tx_buf[STMP_PACKET_SIZE] __attribute__((aligned(4)));

/* ---- Slave register file and counters -------------------------------- */
static uint32_t reg_table[STMP_REG_COUNT];   /* virtual registers, BSS=0 */
static volatile uint32_t slave_rx_count = 0;
static volatile uint32_t slave_tx_count = 0;
static volatile uint32_t slave_err_count = 0;

/* ===================================================================== */
/*  UART helpers                                                          */
/* ===================================================================== */

static void uart_init(void)
{
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC_APB2ENR |= RCC_APB2ENR_USART1EN;

    /* PA9 = USART1_TX, AF7 */
    GPIOA_MODER  = (GPIOA_MODER & ~(3U << 18)) | (2U << 18);
    GPIOA_AFRH   = (GPIOA_AFRH  & ~(0xFU << 4)) | (7U << 4);

    /* 115200 baud @ 16 MHz HSI */
    USART1_BRR = 139;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE;
}

static void uart_putc(char c)
{
    volatile uint32_t t = 200000;
    while (!(USART1_ISR & USART_ISR_TXE) && --t);
    USART1_TDR = (uint8_t)c;
}

static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }

static void uart_print_uint(uint32_t v, int base)
{
    char buf[12]; int idx = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v) {
        int d = (int)(v % (uint32_t)base);
        buf[idx++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        v /= (uint32_t)base;
    }
    for (int i = idx - 1; i >= 0; i--) uart_putc(buf[i]);
}

#define UART_INT(n) uart_print_uint((uint32_t)(n), 10)
#define UART_HEX(n) do { uart_puts("0x"); uart_print_uint((uint32_t)(n), 16); } while(0)

static void fill_bytes(uint8_t *buf, uint8_t val, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) buf[i] = val;
}

/* ===================================================================== */
/*  Delay (~12.5 ms @ 16 MHz, spans multiple Renode quanta)             */
/* ===================================================================== */

static void delay_slave_processing(void)
{
    for (volatile uint32_t i = 0; i < 200000U; i++)
        __asm volatile("nop");
}

/* ===================================================================== */
/*  SPI init + exchange (polling, both phases)                           */
/* ===================================================================== */

static void spi_dma_init(void)
{
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA2EN;
    RCC_APB2ENR |= RCC_APB2ENR_SPI1EN;

    /* PA4=NSS(sw), PA5=SCK, PA6=MISO, PA7=MOSI -- all AF5 */
    GPIOA_MODER = (GPIOA_MODER
                   & ~((3U<<8)|(3U<<10)|(3U<<12)|(3U<<14)))
                  | (1U<<8)
                  | (2U<<10)|(2U<<12)|(2U<<14);
    GPIOA_OSPEEDR |= (3U<<10)|(3U<<12)|(3U<<14);
    GPIOA_AFRL = (GPIOA_AFRL & ~((0xFU<<16)|(0xFU<<20)|(0xFU<<24)|(0xFU<<28)))
               | (5U<<20)|(5U<<24)|(5U<<28);

    GPIOA_BSRR = (1U << 4);   /* CS deasserted */

    SPI1_CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_BR_DIV2;
    SPI1_CR2 = 0;
    SPI1_CR1 |= SPI_CR1_SPE;
}

/*
 * spi_dma_exchange() -- one full request/response cycle
 *
 * TX phase: send `tx[0..len-1]` byte-by-byte via polling.
 *           SPIBridgeSlave.Transmit() delivers the packet to the slave
 *           automatically on the 21st byte (no CS GPIO needed).
 * GAP:      busy-wait so slave ISR runs and writes slave_tx_buf.
 * RX phase: send len dummy bytes (0x00), read back slave response.
 */
static void spi_dma_exchange(const uint8_t *tx, uint8_t *rx, uint32_t len)
{
    volatile uint32_t t;

    GPIOA_BSRR = (1U << (4 + 16));   /* CS assert (PA4 low) */

    for (uint32_t i = 0; i < len; i++) {
        t = 100000; while (!(SPI1_SR & SPI_SR_TXE)  && --t);
        SPI1_DR = tx[i];
        t = 100000; while (!(SPI1_SR & SPI_SR_RXNE) && --t);
        (void)SPI1_DR;
    }
    while (SPI1_SR & SPI_SR_BSY);
    GPIOA_BSRR = (1U << 4);          /* CS deassert */

    delay_slave_processing();

    GPIOA_BSRR = (1U << (4 + 16));   /* CS assert for RX */
    SPI1_CR2 = 0;
    for (uint32_t i = 0; i < len; i++) {
        t = 100000; while (!(SPI1_SR & SPI_SR_TXE)  && --t);
        SPI1_DR = 0x00;
        t = 100000; while (!(SPI1_SR & SPI_SR_RXNE) && --t);
        rx[i] = (uint8_t)SPI1_DR;
    }
    while (SPI1_SR & SPI_SR_BSY);
    GPIOA_BSRR = (1U << 4);          /* CS deassert */
}

/* ===================================================================== */
/*  Bridge controller init (slave side)                                  */
/* ===================================================================== */

static void bridge_ctrl_init(void)
{
    BCTRL_DMA_DST = (uint32_t)slave_rx_buf;
    BCTRL_DMA_SRC = (uint32_t)slave_tx_buf;
    fill_bytes(slave_tx_buf, 0xFF, STMP_PACKET_SIZE);
    BCTRL_CTRL = BCTRL_CTRL_ENABLE | BCTRL_CTRL_USE_DMA | BCTRL_CTRL_IRQ_EN;
    NVIC_ISER1 = NVIC_IRQ58_EN;
}

/* ===================================================================== */
/*  SLAVE ISR (bridge fires DMA2_Stream2_IRQn = IRQ 58)                 */
/* ===================================================================== */

void DMA2_Stream2_IRQHandler(void)
{
    const stmp_packet_t *req = (const stmp_packet_t*)slave_rx_buf;
    stmp_packet_t       *rsp = (stmp_packet_t*)slave_tx_buf;

    slave_rx_count++;

    if (!stmp_validate_basic(req)) {
        slave_err_count++;
        uart_puts("SLAVE: ["); UART_INT(req->seq); uart_puts("] CRC_ERR\r\n");
        stmp_init_pkt(rsp, req->seq, STMP_CMD_STAT_RSP);
        rsp->payload[0] = STMP_STATUS_ERR_CRC;
        stmp_finalize(rsp);
        BCTRL_CTRL |= BCTRL_CTRL_ACK_RX;
        return;
    }

    switch (req->cmd) {

    case STMP_CMD_PING_REQ:
        uart_puts("SLAVE: ["); UART_INT(req->seq); uart_puts("] PING\r\n");
        stmp_init_pkt(rsp, req->seq, STMP_CMD_PING_RSP);
        rsp->payload[0] = req->seq;
        stmp_finalize(rsp);
        break;

    case STMP_CMD_REG_WR: {
        uint8_t  reg = req->payload[0];
        uint32_t val = stmp_get_u32le(&req->payload[1]);
        uart_puts("SLAVE: ["); UART_INT(req->seq);
        uart_puts("] REG_WR["); UART_INT(reg); uart_puts("]=");
        UART_HEX(val); uart_puts("\r\n");
        stmp_init_pkt(rsp, req->seq, STMP_CMD_REG_WR_ACK);
        rsp->payload[0] = reg;
        if (reg < STMP_REG_COUNT) {
            reg_table[reg]  = val;
            rsp->payload[1] = STMP_STATUS_OK;
        } else {
            rsp->payload[1] = STMP_STATUS_ERR_REG;
            slave_err_count++;
        }
        stmp_finalize(rsp);
        break;
    }

    case STMP_CMD_REG_RD: {
        uint8_t reg = req->payload[0];
        uart_puts("SLAVE: ["); UART_INT(req->seq);
        uart_puts("] REG_RD["); UART_INT(reg); uart_puts("]\r\n");
        stmp_init_pkt(rsp, req->seq, STMP_CMD_REG_RD_RSP);
        rsp->payload[0] = reg;
        if (reg < STMP_REG_COUNT)
            stmp_put_u32le(&rsp->payload[1], reg_table[reg]);
        else
            slave_err_count++;
        stmp_finalize(rsp);
        break;
    }

    case STMP_CMD_STAT_REQ:
        uart_puts("SLAVE: ["); UART_INT(req->seq); uart_puts("] STAT\r\n");
        stmp_init_pkt(rsp, req->seq, STMP_CMD_STAT_RSP);
        stmp_put_u32le(&rsp->payload[0], slave_rx_count);
        stmp_put_u32le(&rsp->payload[4], slave_tx_count);
        stmp_put_u32le(&rsp->payload[8], slave_err_count);
        stmp_finalize(rsp);
        break;

    default:
        slave_err_count++;
        uart_puts("SLAVE: ["); UART_INT(req->seq); uart_puts("] UNKNOWN_CMD\r\n");
        stmp_init_pkt(rsp, req->seq, STMP_CMD_STAT_RSP);
        rsp->payload[0] = STMP_STATUS_ERR_CMD;
        stmp_finalize(rsp);
        break;
    }

    slave_tx_count++;
    BCTRL_CTRL |= BCTRL_CTRL_ACK_RX;
}

/* ===================================================================== */
/*  MASTER test loop                                                      */
/* ===================================================================== */

static void run_master(void)
{
    uart_puts("MASTER: STMP v1 test start\r\n");
    spi_dma_init();

    uint32_t passed = 0, failed = 0;
    stmp_packet_t       *tx = (stmp_packet_t*)dma_tx_buf;
    const stmp_packet_t *rx = (const stmp_packet_t*)dma_rx_buf;

#define DO_EXCHANGE() spi_dma_exchange(dma_tx_buf, dma_rx_buf, STMP_PACKET_SIZE)
#define PASS(msg) do { passed++; uart_puts("MASTER: " msg "\r\n"); } while(0)
#define FAIL(msg) do { failed++; uart_puts("MASTER: " msg " FAIL\r\n"); } while(0)

    /* 1: PING seq=1 */
    stmp_init_pkt(tx, 1, STMP_CMD_PING_REQ);
    stmp_finalize(tx);
    DO_EXCHANGE();
    if (stmp_validate_rsp(rx, STMP_CMD_PING_RSP, 1) && rx->payload[0] == 1)
        PASS("[1] PING OK");
    else FAIL("[1] PING");

    /* 2: PING seq=2 */
    stmp_init_pkt(tx, 2, STMP_CMD_PING_REQ);
    stmp_finalize(tx);
    DO_EXCHANGE();
    if (stmp_validate_rsp(rx, STMP_CMD_PING_RSP, 2) && rx->payload[0] == 2)
        PASS("[2] PING OK");
    else FAIL("[2] PING");

    /* 3: REG_WR reg=0 val=0xCAFEBABE */
    stmp_init_pkt(tx, 3, STMP_CMD_REG_WR);
    tx->payload[0] = 0;
    stmp_put_u32le(&tx->payload[1], 0xCAFEBABEU);
    stmp_finalize(tx);
    DO_EXCHANGE();
    if (stmp_validate_rsp(rx, STMP_CMD_REG_WR_ACK, 3) &&
        rx->payload[0] == 0 && rx->payload[1] == STMP_STATUS_OK)
        PASS("[3] REG_WR[0]=CAFEBABE OK");
    else FAIL("[3] REG_WR[0]");

    /* 4: REG_RD reg=0 -- expect 0xCAFEBABE */
    stmp_init_pkt(tx, 4, STMP_CMD_REG_RD);
    tx->payload[0] = 0;
    stmp_finalize(tx);
    DO_EXCHANGE();
    if (stmp_validate_rsp(rx, STMP_CMD_REG_RD_RSP, 4) &&
        rx->payload[0] == 0 &&
        stmp_get_u32le(&rx->payload[1]) == 0xCAFEBABEU)
        PASS("[4] REG_RD[0]=CAFEBABE OK");
    else FAIL("[4] REG_RD[0]");

    /* 5: REG_WR reg=1 val=0xDEAD1234 */
    stmp_init_pkt(tx, 5, STMP_CMD_REG_WR);
    tx->payload[0] = 1;
    stmp_put_u32le(&tx->payload[1], 0xDEAD1234U);
    stmp_finalize(tx);
    DO_EXCHANGE();
    if (stmp_validate_rsp(rx, STMP_CMD_REG_WR_ACK, 5) &&
        rx->payload[0] == 1 && rx->payload[1] == STMP_STATUS_OK)
        PASS("[5] REG_WR[1]=DEAD1234 OK");
    else FAIL("[5] REG_WR[1]");

    /* 6: REG_RD reg=1 -- expect 0xDEAD1234 */
    stmp_init_pkt(tx, 6, STMP_CMD_REG_RD);
    tx->payload[0] = 1;
    stmp_finalize(tx);
    DO_EXCHANGE();
    if (stmp_validate_rsp(rx, STMP_CMD_REG_RD_RSP, 6) &&
        rx->payload[0] == 1 &&
        stmp_get_u32le(&rx->payload[1]) == 0xDEAD1234U)
        PASS("[6] REG_RD[1]=DEAD1234 OK");
    else FAIL("[6] REG_RD[1]");

    /* 7: STAT_REQ -- expect rx=7 tx=6 */
    stmp_init_pkt(tx, 7, STMP_CMD_STAT_REQ);
    stmp_finalize(tx);
    DO_EXCHANGE();
    {
        uint32_t r = stmp_get_u32le(&rx->payload[0]);
        uint32_t t2 = stmp_get_u32le(&rx->payload[4]);
        if (stmp_validate_rsp(rx, STMP_CMD_STAT_RSP, 7) && r == 7 && t2 == 6)
            PASS("[7] STAT rx=7 tx=6 OK");
        else { failed++; uart_puts("MASTER: [7] STAT FAIL rx="); UART_INT(r);
               uart_puts(" tx="); UART_INT(t2); uart_puts("\r\n"); }
    }

    /* 8: PING seq=8 */
    stmp_init_pkt(tx, 8, STMP_CMD_PING_REQ);
    stmp_finalize(tx);
    DO_EXCHANGE();
    if (stmp_validate_rsp(rx, STMP_CMD_PING_RSP, 8) && rx->payload[0] == 8)
        PASS("[8] PING OK");
    else FAIL("[8] PING");

    /* 9: REG_RD reg=0 -- expect 0xCAFEBABE (persisted from exchange 3) */
    stmp_init_pkt(tx, 9, STMP_CMD_REG_RD);
    tx->payload[0] = 0;
    stmp_finalize(tx);
    DO_EXCHANGE();
    if (stmp_validate_rsp(rx, STMP_CMD_REG_RD_RSP, 9) &&
        rx->payload[0] == 0 &&
        stmp_get_u32le(&rx->payload[1]) == 0xCAFEBABEU)
        PASS("[9] REG_RD[0]=CAFEBABE OK");
    else FAIL("[9] REG_RD[0]");

    /* 10: STAT_REQ -- expect rx=10 tx=9 */
    stmp_init_pkt(tx, 10, STMP_CMD_STAT_REQ);
    stmp_finalize(tx);
    DO_EXCHANGE();
    {
        uint32_t r = stmp_get_u32le(&rx->payload[0]);
        uint32_t t2 = stmp_get_u32le(&rx->payload[4]);
        if (stmp_validate_rsp(rx, STMP_CMD_STAT_RSP, 10) && r == 10 && t2 == 9)
            PASS("[10] STAT rx=10 tx=9 OK");
        else { failed++; uart_puts("MASTER: [10] STAT FAIL rx="); UART_INT(r);
               uart_puts(" tx="); UART_INT(t2); uart_puts("\r\n"); }
    }

#undef DO_EXCHANGE
#undef PASS
#undef FAIL

    if (failed == 0 && passed == NUM_TEST_PACKETS)
        uart_puts("MASTER: ALL TESTS PASSED\r\n");
    else {
        uart_puts("MASTER: TEST FAILED passed="); UART_INT(passed);
        uart_puts(" failed="); UART_INT(failed); uart_puts("\r\n");
    }
}

/* ===================================================================== */
/*  SLAVE wait loop                                                       */
/* ===================================================================== */

static void run_slave(void)
{
    uart_puts("SLAVE: STMP v1 ready\r\n");
    bridge_ctrl_init();

    while (slave_rx_count < NUM_TEST_PACKETS)
        __asm volatile("wfi");

    if (slave_err_count == 0)
        uart_puts("SLAVE: ALL DONE\r\n");
    else {
        uart_puts("SLAVE: DONE WITH ERRORS err=");
        UART_INT(slave_err_count);
        uart_puts("\r\n");
    }
}

/* ===================================================================== */
/*  main                                                                  */
/* ===================================================================== */

int main(void)
{
    uart_init();

    if (g_role_flag == ROLE_MASTER)
        run_master();
    else
        run_slave();

    while (1) __asm volatile("wfi");
}
