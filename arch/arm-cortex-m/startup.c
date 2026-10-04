/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata, _edata;
extern uint32_t _sbss, _ebss;

extern int main(void);
extern void br_hal_board_init(void);
extern void SysTick_Handler(void);
extern void PendSV_Handler(void);
extern void SVC_Handler(void);

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

    br_hal_board_init();
    main();
    while (1) { }
}

static void Default_Handler(void)
{
    while (1) { }
}

void IRQ0_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ1_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ2_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ3_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ4_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ5_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ6_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ7_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ8_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ9_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ10_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ11_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ12_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ13_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ14_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ15_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ16_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ17_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ18_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ19_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ20_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ21_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ22_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ23_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ24_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ25_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ26_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ27_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ28_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ29_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ30_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ31_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ32_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ33_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ34_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ35_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ36_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ37_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ38_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ39_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ40_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ41_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ42_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ43_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ44_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ45_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ46_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ47_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ48_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ49_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ50_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ51_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ52_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ53_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ54_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ55_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ56_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ57_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ58_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ59_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ60_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ61_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ62_Handler(void) __attribute__((weak, alias("Default_Handler")));
void IRQ63_Handler(void) __attribute__((weak, alias("Default_Handler")));

__attribute__((section(".isr_vector")))
const uint32_t vectors[] = {
    (uint32_t)&_estack,
    (uint32_t)Reset_Handler,
    (uint32_t)Default_Handler,  /* NMI */
    (uint32_t)Default_Handler,  /* HardFault */
    (uint32_t)Default_Handler,  /* MemManage */
    (uint32_t)Default_Handler,  /* BusFault */
    (uint32_t)Default_Handler,  /* UsageFault */
    0, 0, 0, 0,                 /* Reserved */
    (uint32_t)SVC_Handler,      /* SVCall */
    (uint32_t)Default_Handler,  /* Debug */
    0,                          /* Reserved */
    (uint32_t)PendSV_Handler,
    (uint32_t)SysTick_Handler,
    (uint32_t)IRQ0_Handler,
    (uint32_t)IRQ1_Handler,
    (uint32_t)IRQ2_Handler,
    (uint32_t)IRQ3_Handler,
    (uint32_t)IRQ4_Handler,
    (uint32_t)IRQ5_Handler,
    (uint32_t)IRQ6_Handler,
    (uint32_t)IRQ7_Handler,
    (uint32_t)IRQ8_Handler,
    (uint32_t)IRQ9_Handler,
    (uint32_t)IRQ10_Handler,
    (uint32_t)IRQ11_Handler,
    (uint32_t)IRQ12_Handler,
    (uint32_t)IRQ13_Handler,
    (uint32_t)IRQ14_Handler,
    (uint32_t)IRQ15_Handler,
    (uint32_t)IRQ16_Handler,
    (uint32_t)IRQ17_Handler,
    (uint32_t)IRQ18_Handler,
    (uint32_t)IRQ19_Handler,
    (uint32_t)IRQ20_Handler,
    (uint32_t)IRQ21_Handler,
    (uint32_t)IRQ22_Handler,
    (uint32_t)IRQ23_Handler,
    (uint32_t)IRQ24_Handler,
    (uint32_t)IRQ25_Handler,
    (uint32_t)IRQ26_Handler,
    (uint32_t)IRQ27_Handler,
    (uint32_t)IRQ28_Handler,
    (uint32_t)IRQ29_Handler,
    (uint32_t)IRQ30_Handler,
    (uint32_t)IRQ31_Handler,
    (uint32_t)IRQ32_Handler,
    (uint32_t)IRQ33_Handler,
    (uint32_t)IRQ34_Handler,
    (uint32_t)IRQ35_Handler,
    (uint32_t)IRQ36_Handler,
    (uint32_t)IRQ37_Handler,
    (uint32_t)IRQ38_Handler,
    (uint32_t)IRQ39_Handler,
    (uint32_t)IRQ40_Handler,
    (uint32_t)IRQ41_Handler,
    (uint32_t)IRQ42_Handler,
    (uint32_t)IRQ43_Handler,
    (uint32_t)IRQ44_Handler,
    (uint32_t)IRQ45_Handler,
    (uint32_t)IRQ46_Handler,
    (uint32_t)IRQ47_Handler,
    (uint32_t)IRQ48_Handler,
    (uint32_t)IRQ49_Handler,
    (uint32_t)IRQ50_Handler,
    (uint32_t)IRQ51_Handler,
    (uint32_t)IRQ52_Handler,
    (uint32_t)IRQ53_Handler,
    (uint32_t)IRQ54_Handler,
    (uint32_t)IRQ55_Handler,
    (uint32_t)IRQ56_Handler,
    (uint32_t)IRQ57_Handler,
    (uint32_t)IRQ58_Handler,
    (uint32_t)IRQ59_Handler,
    (uint32_t)IRQ60_Handler,
    (uint32_t)IRQ61_Handler,
    (uint32_t)IRQ62_Handler,
    (uint32_t)IRQ63_Handler,
};
