/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#define FLASH_ACR       (*(volatile uint32_t *)0x40022000UL)
#define RCC_CR          (*(volatile uint32_t *)0x40021000UL)
#define RCC_CFGR        (*(volatile uint32_t *)0x40021004UL)
#define RCC_APB2ENR     (*(volatile uint32_t *)0x40021018UL)
#define GPIOA_CRH       (*(volatile uint32_t *)0x40010804UL)

#define FLASH_ACR_PRFTBE     (1UL << 4)
#define FLASH_ACR_LATENCY_2  2UL

#define RCC_CR_HSEON         (1UL << 16)
#define RCC_CR_HSERDY        (1UL << 17)
#define RCC_CR_PLLON         (1UL << 24)
#define RCC_CR_PLLRDY        (1UL << 25)

#define RCC_CFGR_SW_PLL      2UL
#define RCC_CFGR_PPRE1_DIV2  (4UL << 8)
#define RCC_CFGR_PLLSRC_HSE  (1UL << 16)
#define RCC_CFGR_PLLMUL_9    (7UL << 18)

#define RCC_APB2ENR_AFIOEN   (1UL << 0)
#define RCC_APB2ENR_IOPAEN   (1UL << 2)
#define RCC_APB2ENR_USART1EN (1UL << 14)

#define GPIO_CRH_PIN9_MASK   (0xFUL << 4)
#define GPIO_CRH_PIN9_AF_PP  (0xBUL << 4)

extern void br_uart_init(void);

static void clocks_init(void)
{
    FLASH_ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    RCC_CR |= RCC_CR_HSEON;
    while (!(RCC_CR & RCC_CR_HSERDY)) { }

    RCC_CFGR = RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMUL_9 | RCC_CFGR_PPRE1_DIV2;

    RCC_CR |= RCC_CR_PLLON;
    while (!(RCC_CR & RCC_CR_PLLRDY)) { }

    RCC_CFGR |= RCC_CFGR_SW_PLL;
}

void br_hal_board_init(void)
{
    clocks_init();

    RCC_APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_USART1EN;
    GPIOA_CRH = (GPIOA_CRH & ~GPIO_CRH_PIN9_MASK) | GPIO_CRH_PIN9_AF_PP;

    br_uart_init();
}
