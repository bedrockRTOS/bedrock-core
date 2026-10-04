/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#define RESETS_RESET_CLR     (*(volatile uint32_t *)0x4000F000UL)
#define RESETS_RESET_DONE    (*(volatile uint32_t *)0x4000C008UL)

#define XOSC_CTRL            (*(volatile uint32_t *)0x40024000UL)
#define XOSC_STATUS          (*(volatile uint32_t *)0x40024004UL)
#define XOSC_STARTUP         (*(volatile uint32_t *)0x4002400CUL)

#define CLK_REF_CTRL         (*(volatile uint32_t *)0x40008030UL)
#define CLK_REF_SELECTED     (*(volatile uint32_t *)0x40008038UL)
#define CLK_SYS_CTRL         (*(volatile uint32_t *)0x4000803CUL)
#define CLK_SYS_SELECTED     (*(volatile uint32_t *)0x40008044UL)
#define CLK_PERI_CTRL        (*(volatile uint32_t *)0x40008048UL)

#define WATCHDOG_TICK        (*(volatile uint32_t *)0x4005802CUL)

#define RESET_IO_BANK0       (1UL << 5)
#define RESET_PADS_BANK0     (1UL << 8)
#define RESET_TIMER          (1UL << 21)
#define RESET_UART0          (1UL << 22)

#define XOSC_FREQ_1_15MHZ    0xAA0UL
#define XOSC_ENABLE          (0xFABUL << 12)
#define XOSC_STABLE          (1UL << 31)
#define XOSC_STARTUP_DELAY   47

#define CLK_REF_SRC_XOSC     2
#define CLK_SYS_SRC_REF      0
#define CLK_PERI_ENABLE      (1UL << 11)

#define WATCHDOG_TICK_ENABLE (1UL << 9)
#define XOSC_MHZ             12

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata, _edata;
extern uint32_t _sbss, _ebss;

extern int main(void);
extern void br_uart_init(void);
extern void br_hal_board_init(void);
extern void SVC_Handler(void);
extern void PendSV_Handler(void);
extern void TIMER_IRQ_0_Handler(void);

static void clocks_init(void)
{
    XOSC_STARTUP = XOSC_STARTUP_DELAY;
    XOSC_CTRL = XOSC_FREQ_1_15MHZ | XOSC_ENABLE;
    while (!(XOSC_STATUS & XOSC_STABLE)) { }

    CLK_REF_CTRL = CLK_REF_SRC_XOSC;
    while (CLK_REF_SELECTED != (1UL << CLK_REF_SRC_XOSC)) { }

    CLK_SYS_CTRL = CLK_SYS_SRC_REF;
    while (CLK_SYS_SELECTED != (1UL << CLK_SYS_SRC_REF)) { }

    CLK_PERI_CTRL = CLK_PERI_ENABLE;

    WATCHDOG_TICK = XOSC_MHZ | WATCHDOG_TICK_ENABLE;
}

static void resets_release(void)
{
    uint32_t mask = RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_TIMER | RESET_UART0;

    RESETS_RESET_CLR = mask;
    while ((RESETS_RESET_DONE & mask) != mask) { }
}

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    clocks_init();
    resets_release();
    br_uart_init();
    br_hal_board_init();

    main();
    while (1) { }
}

static void Default_Handler(void)
{
    while (1) { }
}

__attribute__((section(".isr_vector")))
const uint32_t vectors[] = {
    (uint32_t)&_estack,
    (uint32_t)Reset_Handler,
    (uint32_t)Default_Handler,
    (uint32_t)Default_Handler,
    0, 0, 0, 0, 0, 0, 0,
    (uint32_t)SVC_Handler,
    0, 0,
    (uint32_t)PendSV_Handler,
    (uint32_t)Default_Handler,
    (uint32_t)TIMER_IRQ_0_Handler,
};
