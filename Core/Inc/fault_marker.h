#ifndef __FAULT_MARKER_H__
#define __FAULT_MARKER_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reason for the previous reset (from RCC_CSR + CCMRAM marker). */
typedef enum {
    RESET_REASON_NONE          = 0,  /* normal cold-boot (unknown / first boot) */
    RESET_REASON_POWER_ON      = 1,  /* power-on reset (PORRSTF) */
    RESET_REASON_PIN           = 2,  /* external NRST pin */
    RESET_REASON_SOFTWARE      = 3,  /* NVIC_SystemReset() */
    RESET_REASON_IWDG          = 4,  /* independent watchdog timeout */
    RESET_REASON_WWDG          = 5,  /* window watchdog */
    RESET_REASON_LOWPOWER      = 6,  /* low-power reset */
    RESET_REASON_BROWN_OUT     = 7,  /* brown-out reset */
    RESET_REASON_STACK_OVF     = 100,  /* FreeRTOS stack overflow hook fired */
    RESET_REASON_MALLOC_FAIL   = 101,  /* FreeRTOS malloc failed hook fired */
} reset_reason_t;

typedef struct {
    reset_reason_t reason;
    const char    *task_name;   /* valid only for STACK_OVF / MALLOC_FAIL */
    uint32_t       tick_at_fault;
} fault_info_t;

/* Snapshot reset reason on early boot. Reads RCC_CSR, then clears RMVF.
 * Also reads CCMRAM marker (set by FreeRTOS hooks) and clears it. */
void fault_marker_capture(void);

/* Get the captured snapshot. Returns the same value each call. */
const fault_info_t *fault_marker_get(void);

/* Human-readable name of the reset reason (static literal). */
const char *fault_marker_reason_str(reset_reason_t r);

#ifdef __cplusplus
}
#endif

#endif /* __FAULT_MARKER_H__ */
