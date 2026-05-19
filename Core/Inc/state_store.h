#ifndef __STATE_STORE_H__
#define __STATE_STORE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Persistent storage for output state.
 * Backed by RTC backup registers (RTC_BKP_DR0..DR1, in the backup domain
 * which is powered from VBAT when main supply is off).
 *
 * Survives: software reset, IWDG reset, brown-out (if VBAT present).
 * Lost on:  cold power-on without VBAT battery.
 *
 * Encoding (one byte = one output mask, identical to outputs.h bit layout):
 *   bit 0 LED1, bit 1 LED2, bit 2 LED3
 *   bit 4 RELAY1, bit 5 RELAY2, bit 6 RELAY3 */

/* Call ONCE during boot, before any save/load. Enables backup-domain access. */
void state_store_init(void);

/* Read saved state. Returns 0 if BKP is invalid (first boot or VBAT lost). */
uint8_t state_store_load(void);

/* Persist new state. Idempotent — only writes if value changed. */
void state_store_save(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif
