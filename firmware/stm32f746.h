#ifndef STM32F746_H
#define STM32F746_H

#include <stdint.h>

/* ---- Peripheral base addresses --------------------------------------- */
#define RCC_BASE        0x40023800UL
#define GPIOA_BASE      0x40020000UL
#define USART1_BASE     0x40011000UL
#define SPI1_BASE       0x40013000UL
#define DMA2_BASE       0x40026400UL

/* ---- RCC registers --------------------------------------------------- */
#define RCC_AHB1ENR     (*(volatile uint32_t*)(RCC_BASE + 0x30))
#define RCC_APB2ENR     (*(volatile uint32_t*)(RCC_BASE + 0x44))

#define RCC_AHB1ENR_GPIOAEN     (1U << 0)
#define RCC_AHB1ENR_DMA2EN      (1U << 22)
#define RCC_APB2ENR_USART1EN    (1U << 4)
#define RCC_APB2ENR_SPI1EN      (1U << 12)

/* ---- GPIOA registers ------------------------------------------------- */
#define GPIOA_MODER     (*(volatile uint32_t*)(GPIOA_BASE + 0x00))
#define GPIOA_OSPEEDR   (*(volatile uint32_t*)(GPIOA_BASE + 0x08))
#define GPIOA_AFRL      (*(volatile uint32_t*)(GPIOA_BASE + 0x20))
#define GPIOA_AFRH      (*(volatile uint32_t*)(GPIOA_BASE + 0x24))
#define GPIOA_BSRR      (*(volatile uint32_t*)(GPIOA_BASE + 0x18))
#define GPIOA_ODR       (*(volatile uint32_t*)(GPIOA_BASE + 0x14))

/* ---- USART1 registers (STM32F7 layout) ------------------------------- */
#define USART1_CR1      (*(volatile uint32_t*)(USART1_BASE + 0x00))
#define USART1_BRR      (*(volatile uint32_t*)(USART1_BASE + 0x0C))
#define USART1_ISR      (*(volatile uint32_t*)(USART1_BASE + 0x1C))
#define USART1_TDR      (*(volatile uint32_t*)(USART1_BASE + 0x28))

#define USART_CR1_UE    (1U << 0)
#define USART_CR1_RE    (1U << 2)
#define USART_CR1_TE    (1U << 3)
#define USART_ISR_TXE   (1U << 7)

/* ---- SPI1 registers -------------------------------------------------- */
#define SPI1_CR1        (*(volatile uint32_t*)(SPI1_BASE + 0x00))
#define SPI1_CR2        (*(volatile uint32_t*)(SPI1_BASE + 0x04))
#define SPI1_SR         (*(volatile uint32_t*)(SPI1_BASE + 0x08))
#define SPI1_DR         (*(volatile uint32_t*)(SPI1_BASE + 0x0C))

#define SPI_CR1_MSTR    (1U << 2)
#define SPI_CR1_BR_DIV2 (0U << 3)   /* fPCLK/2 */
#define SPI_CR1_SPE     (1U << 6)
#define SPI_CR1_SSI     (1U << 8)
#define SPI_CR1_SSM     (1U << 9)
#define SPI_CR2_RXDMAEN (1U << 0)
#define SPI_CR2_TXDMAEN (1U << 1)
#define SPI_SR_RXNE     (1U << 0)
#define SPI_SR_TXE      (1U << 1)
#define SPI_SR_BSY      (1U << 7)

/* ---- DMA2 registers -------------------------------------------------- */
/* DMA2 global status/clear */
#define DMA2_LISR       (*(volatile uint32_t*)(DMA2_BASE + 0x00))
#define DMA2_HISR       (*(volatile uint32_t*)(DMA2_BASE + 0x04))
#define DMA2_LIFCR      (*(volatile uint32_t*)(DMA2_BASE + 0x08))
#define DMA2_HIFCR      (*(volatile uint32_t*)(DMA2_BASE + 0x0C))

/* Stream 2 (SPI1_RX, channel 3) at offset 0x40 */
#define DMA2_S2CR       (*(volatile uint32_t*)(DMA2_BASE + 0x40))
#define DMA2_S2NDTR     (*(volatile uint32_t*)(DMA2_BASE + 0x44))
#define DMA2_S2PAR      (*(volatile uint32_t*)(DMA2_BASE + 0x48))
#define DMA2_S2M0AR     (*(volatile uint32_t*)(DMA2_BASE + 0x4C))
#define DMA2_S2FCR      (*(volatile uint32_t*)(DMA2_BASE + 0x54))

/* Stream 3 (SPI1_TX, channel 3) at offset 0x58 */
#define DMA2_S3CR       (*(volatile uint32_t*)(DMA2_BASE + 0x58))
#define DMA2_S3NDTR     (*(volatile uint32_t*)(DMA2_BASE + 0x5C))
#define DMA2_S3PAR      (*(volatile uint32_t*)(DMA2_BASE + 0x60))
#define DMA2_S3M0AR     (*(volatile uint32_t*)(DMA2_BASE + 0x64))
#define DMA2_S3FCR      (*(volatile uint32_t*)(DMA2_BASE + 0x6C))

/* DMA stream CR bits */
#define DMA_SCR_EN      (1U << 0)
#define DMA_SCR_TCIE    (1U << 4)
#define DMA_SCR_MINC    (1U << 10)
#define DMA_SCR_DIR_P2M (0U << 6)   /* Peripheral to memory */
#define DMA_SCR_DIR_M2P (1U << 6)   /* Memory to peripheral */
#define DMA_SCR_CH3     (3U << 25)  /* Channel 3 = SPI1 */
#define DMA_SCR_PL_MED  (1U << 16)  /* Medium priority */

/* LISR/LIFCR bits for stream 2 */
#define DMA_LISR_TCIF2  (1U << 21)
#define DMA_LIFCR_CTCIF2 (1U << 21)
/* LISR/LIFCR bits for stream 3 */
#define DMA_LISR_TCIF3  (1U << 27)
#define DMA_LIFCR_CTCIF3 (1U << 27)

/* ---- NVIC ------------------------------------------------------------ */
#define NVIC_ISER1      (*(volatile uint32_t*)0xE000E104)
/* IRQ58 = DMA2_Stream2 = bit 26 of ISER1 */
#define NVIC_IRQ58_EN   (1U << 26)

/* ---- Bridge Controller (custom, at 0x50000000) ----------------------- */
#define BCTRL_BASE      0x50000000UL
#define BCTRL_STATUS    (*(volatile uint32_t*)(BCTRL_BASE + 0x00))
#define BCTRL_RX_COUNT  (*(volatile uint32_t*)(BCTRL_BASE + 0x04))
#define BCTRL_DMA_DST   (*(volatile uint32_t*)(BCTRL_BASE + 0x08))
#define BCTRL_DMA_SRC   (*(volatile uint32_t*)(BCTRL_BASE + 0x0C))
#define BCTRL_CTRL      (*(volatile uint32_t*)(BCTRL_BASE + 0x10))

#define BCTRL_STATUS_RX_READY   (1U << 0)
#define BCTRL_CTRL_ENABLE       (1U << 0)
#define BCTRL_CTRL_USE_DMA      (1U << 1)
#define BCTRL_CTRL_IRQ_EN       (1U << 2)
#define BCTRL_CTRL_ACK_RX       (1U << 3)

#endif /* STM32F746_H */
