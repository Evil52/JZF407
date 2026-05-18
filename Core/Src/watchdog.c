/*
 * Bare-metal IWDG driver (no HAL dependency).
 *
 * IWDG hardware:
 *   - Clocked by LSI (~32 kHz, free-running, not affected by main clock failures)
 *   - 12-bit down counter with programmable prescaler (/4 .. /256)
 *   - Three magic keys to KR register:
 *       0xCCCC = start the watchdog (irreversible until reset)
 *       0x5555 = unlock PR/RLR for write
 *       0xAAAA = refresh counter (reload from RLR)
 *
 * Timing (LSI nominal 32 kHz, prescaler /256, reload 1000):
 *   tick = 256 / 32000 = 8 ms
 *   timeout = 1000 * 8 ms = 8000 ms (~8 seconds)
 *
 * LSI is uncalibrated; actual timeout may range 5.5..11 s. The watchdog_task
 * refreshes every 1500 ms, leaving comfortable margin even in worst case.
 */

#include "watchdog.h"
#include "stm32f4xx.h"
#include "cmsis_os.h"

#define IWDG_KEY_RELOAD    ((uint32_t)0x0000AAAAu)
#define IWDG_KEY_ENABLE    ((uint32_t)0x0000CCCCu)
#define IWDG_KEY_WRITE_EN  ((uint32_t)0x00005555u)

#define IWDG_PRESCALER_256 ((uint32_t)0x06u)  /* PR[2:0] = 110 */
#define IWDG_RELOAD_VALUE  ((uint32_t)2500u)  /* ~20 sec at /256 — covers slow PHY init */

void watchdog_start(void)
{
    /* Enable write access to PR/RLR */
    IWDG->KR = IWDG_KEY_WRITE_EN;

    /* Wait until PR is writable (PVU bit clears) */
    while (IWDG->SR & IWDG_SR_PVU) { __NOP(); }
    IWDG->PR = IWDG_PRESCALER_256;

    /* Wait until RLR is writable (RVU bit clears) */
    while (IWDG->SR & IWDG_SR_RVU) { __NOP(); }
    IWDG->RLR = IWDG_RELOAD_VALUE;

    /* Reload counter and start the watchdog */
    IWDG->KR = IWDG_KEY_RELOAD;
    IWDG->KR = IWDG_KEY_ENABLE;
}

void watchdog_refresh(void)
{
    IWDG->KR = IWDG_KEY_RELOAD;
}

void watchdog_task(void *argument)
{
    (void)argument;
    for (;;) {
        watchdog_refresh();
        osDelay(1500);  /* 1.5 s — leaves >5 s margin before 8 s timeout */
    }
}
