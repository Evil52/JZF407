#ifndef __OUTPUTS_H__
#define __OUTPUTS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Output bitmask values — share between led_dispatch and outputs modules.
 *
 *   Bit  Output         Pin     P4 pin    Notes
 *   0x01 LED1           PE13    -         onboard
 *   0x02 LED2           PE14    -         onboard
 *   0x04 LED3           PE15    -         onboard
 *   0x10 RELAY1 (CH1)   PA0     P4 pin 8  external relay module
 *   0x20 RELAY2 (CH2)   PC0     P4 pin 7
 *   0x40 RELAY3 (CH3)   PA3     P4 pin 5
 */
#define OUT_LED1     0x01u
#define OUT_LED2     0x02u
#define OUT_LED3     0x04u
#define OUT_LED_ALL  0x07u
#define OUT_RELAY1   0x10u
#define OUT_RELAY2   0x20u
#define OUT_RELAY3   0x40u
#define OUT_RELAY_ALL 0x70u

/* Apply state to outputs in mask. Bits not in mask are unchanged.
 * All outputs are active-LOW (HIGH on pin = OFF, LOW = ON / energised). */
void outputs_apply(uint8_t mask, uint8_t state);

/* Fail-safe: turn EVERY managed output OFF immediately.
 * Idempotent — safe to call repeatedly. */
void outputs_fail_safe(void);

#ifdef __cplusplus
}
#endif

#endif
