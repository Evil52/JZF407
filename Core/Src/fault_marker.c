/*
 * Fault marker — captures previous-reset cause.
 *
 * Sources:
 *   - RCC->CSR flags (PORRSTF, PINRSTF, IWDGRSTF, ...): set by hardware,
 *     persist through soft resets, cleared by writing RMVF.
 *   - CCMRAM marker at 0x1000FFF0 (last 16 bytes of CCMRAM): written by
 *     vApplicationStackOverflowHook / vApplicationMallocFailedHook before
 *     the CPU is frozen. CCMRAM survives software reset (and IWDG reset),
 *     but contents are random after power-on.
 *
 * Call fault_marker_capture() ONCE, very early in main() — before RCC->CSR
 * could be cleared by any other code.
 */

#include "fault_marker.h"
#include "stm32f4xx.h"

#define MARKER_ADDR        ((volatile uint32_t *)0x1000FFF0u)
#define MAGIC_STACK        0xDEAD1111u
#define MAGIC_MALLOC       0xDEAD2222u
#define MAGIC_SENTINEL     0xA5A5A5A5u

static fault_info_t s_info = { RESET_REASON_NONE, NULL, 0 };
static uint8_t       s_captured = 0;

static reset_reason_t classify_rcc(uint32_t csr)
{
    /* Check in priority order — most specific first. IWDG takes priority over
     * generic resets because IWDG also raises PIN/POR flags on some MCUs. */
    if (csr & RCC_CSR_IWDGRSTF) return RESET_REASON_IWDG;
    if (csr & RCC_CSR_WWDGRSTF) return RESET_REASON_WWDG;
    if (csr & RCC_CSR_SFTRSTF)  return RESET_REASON_SOFTWARE;
    if (csr & RCC_CSR_LPWRRSTF) return RESET_REASON_LOWPOWER;
    if (csr & RCC_CSR_BORRSTF)  return RESET_REASON_BROWN_OUT;
    if (csr & RCC_CSR_PINRSTF)  return RESET_REASON_PIN;
    if (csr & RCC_CSR_PORRSTF)  return RESET_REASON_POWER_ON;
    return RESET_REASON_NONE;
}

void fault_marker_capture(void)
{
    if (s_captured) return;
    s_captured = 1;

    /* 1. CCMRAM marker takes priority — it indicates a software-detected
     *    fault (stack overflow / OOM) before the IWDG kicked in. */
    uint32_t magic    = MARKER_ADDR[0];
    uint32_t name_ptr = MARKER_ADDR[1];
    uint32_t tick     = MARKER_ADDR[2];
    uint32_t sentinel = MARKER_ADDR[3];

    if (sentinel == MAGIC_SENTINEL) {
        if (magic == MAGIC_STACK) {
            s_info.reason        = RESET_REASON_STACK_OVF;
            s_info.task_name     = (const char *)name_ptr;
            s_info.tick_at_fault = tick;
        } else if (magic == MAGIC_MALLOC) {
            s_info.reason        = RESET_REASON_MALLOC_FAIL;
            s_info.task_name     = "malloc";
            s_info.tick_at_fault = tick;
        }
        /* Clear marker so next reset is not falsely attributed */
        MARKER_ADDR[0] = 0;
        MARKER_ADDR[1] = 0;
        MARKER_ADDR[2] = 0;
        MARKER_ADDR[3] = 0;
    }

    /* 2. If no software marker, classify from RCC_CSR */
    if (s_info.reason == RESET_REASON_NONE) {
        s_info.reason = classify_rcc(RCC->CSR);
    }

    /* Clear RCC reset flags so the *next* boot sees a clean slate */
    RCC->CSR |= RCC_CSR_RMVF;
}

const fault_info_t *fault_marker_get(void)
{
    return &s_info;
}

const char *fault_marker_reason_str(reset_reason_t r)
{
    switch (r) {
        case RESET_REASON_POWER_ON:    return "power_on";
        case RESET_REASON_PIN:         return "nrst_pin";
        case RESET_REASON_SOFTWARE:    return "software";
        case RESET_REASON_IWDG:        return "iwdg_timeout";
        case RESET_REASON_WWDG:        return "wwdg";
        case RESET_REASON_LOWPOWER:    return "lowpower";
        case RESET_REASON_BROWN_OUT:   return "brown_out";
        case RESET_REASON_STACK_OVF:   return "stack_overflow";
        case RESET_REASON_MALLOC_FAIL: return "malloc_failed";
        default:                       return "unknown";
    }
}
