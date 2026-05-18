/*
 * FreeRTOS production hooks: stack overflow + malloc failure.
 *
 * Behaviour: store fault marker in a fixed RAM location (survives soft reset)
 * and freeze the CPU. Independent watchdog (if enabled) will then trigger
 * a hard reset, and the marker can be read in the bootloader / by debugger.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* Magic values stored in the last 16 bytes of CCMRAM (or top of SRAM if no CCM).
 * On STM32F407 CCMRAM at 0x10000000, 64 KB — last word at 0x1000FFFC. */
#define FAULT_MARKER_ADDR  ((volatile uint32_t *)0x1000FFF0u)
#define FAULT_MAGIC_STACK  0xDEAD1111u
#define FAULT_MAGIC_MALLOC 0xDEAD2222u

static void freeze(uint32_t magic, const char *name)
{
    /* Save marker so a post-reset diagnostic can read it */
    FAULT_MARKER_ADDR[0] = magic;
    FAULT_MARKER_ADDR[1] = (uint32_t)name;        /* pointer to task name */
    FAULT_MARKER_ADDR[2] = (uint32_t)xTaskGetTickCount();
    FAULT_MARKER_ADDR[3] = 0xA5A5A5A5u;           /* sentinel */
    __DSB();

    /* Disable interrupts — independent watchdog will reset the MCU.
     * Without IWDG we just spin (visible to debugger). */
    __disable_irq();
    for (;;) { __NOP(); }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    freeze(FAULT_MAGIC_STACK, pcTaskName);
}

void vApplicationMallocFailedHook(void)
{
    freeze(FAULT_MAGIC_MALLOC, "malloc");
}
