/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "stm32f4_regs.h"

#define PORT_A          0
#define PIN_USART1_TX   9
#define PIN_USART1_RX   10
#define AF_USART1       7UL

extern void br_uart_init(void);

void br_hal_board_init(void)
{
    RCC_APB2ENR_REG |= RCC_APB2ENR_USART1EN;
    (void)RCC_APB2ENR_REG;

    stm32_gpio_pin(PORT_A, PIN_USART1_TX, GPIO_MODE_AF, GPIO_PULL_NONE, AF_USART1, 0);
    stm32_gpio_pin(PORT_A, PIN_USART1_RX, GPIO_MODE_AF, GPIO_PULL_UP, AF_USART1, 0);

    br_uart_init();
}
