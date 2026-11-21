/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

extern void br_uart_init(void);

void br_hal_board_init(void)
{
    br_uart_init();
}
