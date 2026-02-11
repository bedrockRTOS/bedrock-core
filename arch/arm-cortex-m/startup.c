/*
 * Project: bedrock[RTOS]
 * Version: 0.0.1
 * Author:  AnmiTaliDev <anmitalidev@nuros.org>
 * License: BSD 3-Clause
 *
 * Minimal startup / vector table for Cortex-M3 (QEMU LM3S6965).
 */

#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata, _edata;
extern uint32_t _sbss, _ebss;

extern int main(void);
extern void SysTick_Handler(void);
extern void PendSV_Handler(void);

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
    (uint32_t)Default_Handler,  /* NMI */
    (uint32_t)Default_Handler,  /* HardFault */
    (uint32_t)Default_Handler,  /* MemManage */
    (uint32_t)Default_Handler,  /* BusFault */
    (uint32_t)Default_Handler,  /* UsageFault */
    0, 0, 0, 0,                 /* Reserved */
    (uint32_t)Default_Handler,  /* SVCall */
    (uint32_t)Default_Handler,  /* Debug */
    0,                          /* Reserved */
    (uint32_t)PendSV_Handler,
    (uint32_t)SysTick_Handler,
};
