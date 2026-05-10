#ifndef STM32F4_H
#define STM32F4_H

#include <stdint.h>

#define RCC_BASE        0x40023800UL
#define GPIOA_BASE      0x40020000UL
#define USART1_BASE     0x40011000UL
#define MOCK_USB_BASE   0x50000000UL

#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x44))

#define RCC_AHB1ENR_GPIOAEN     (1U << 0)
#define RCC_APB2ENR_USART1EN    (1U << 4)

#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_AFRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x24))

#define USART1_SR       (*(volatile uint32_t *)(USART1_BASE + 0x00))
#define USART1_DR       (*(volatile uint32_t *)(USART1_BASE + 0x04))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x08))
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x0C))

#define USART_CR1_UE    (1U << 13)
#define USART_CR1_TE    (1U << 3)
#define USART_SR_TXE    (1U << 7)

#define MOCK_STATUS     (*(volatile uint32_t *)(MOCK_USB_BASE + 0x00))
#define MOCK_CONTROL    (*(volatile uint32_t *)(MOCK_USB_BASE + 0x04))
#define MOCK_OUT_WORD(i) (*(volatile uint32_t *)(MOCK_USB_BASE + 0x100 + ((i) * 4U)))
#define MOCK_IN_WORD(i)  (*(volatile uint32_t *)(MOCK_USB_BASE + 0x200 + ((i) * 4U)))

#define MOCK_STATUS_OUT_READY      (1U << 0)
#define MOCK_STATUS_IN_READY       (1U << 1)
#define MOCK_STATUS_TOUCH_PRESENT  (1U << 2)
#define MOCK_STATUS_DONE           (1U << 3)

#define MOCK_CONTROL_ACK_OUT       (1U << 0)
#define MOCK_CONTROL_SEND_IN       (1U << 1)

#endif /* STM32F4_H */
