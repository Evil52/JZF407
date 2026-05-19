#ifndef __OUTPUTS_H__
#define __OUTPUTS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MQTT-managed outputs (bit masks shared with led_dispatch):
 *
 *   Bit   Name      Pin     Where           Notes
 *   0x01  OUT_LED1  PE13    onboard         green/red LED, active-LOW
 *   0x02  OUT_LED2  PE14    onboard         active-LOW
 *   0x04  OUT_RELAY PD4     P4 pin 16       SONGLE relay CH, active-LOW
 *
 * PE15 (LED3) is reserved as the heartbeat indicator — driven by mqtt_app
 * regardless of MQTT, NOT managed by outputs_apply / state_store. */
#define OUT_LED1    0x01u
#define OUT_LED2    0x02u
#define OUT_RELAY   0x04u
#define OUT_ALL     0x07u

/* Apply state to outputs in mask. Bits not in mask are unchanged.
 * Persists the resulting full state to EEPROM. */
void outputs_apply(uint8_t mask, uint8_t state);

/* Restore from EEPROM and drive all outputs to match.
 * Call ONCE early in boot, after MX_GPIO_Init() and state_store_init(). */
void outputs_restore_from_nvm(void);

/* Fail-safe: turn every managed output OFF (LED1, LED2, RELAY).
 * Used by emergency paths only. */
void outputs_fail_safe(void);

/* --- Heartbeat LED helpers (LED3 / PE15), not part of MQTT state --- */
void heartbeat_led_on(void);
void heartbeat_led_off(void);

#ifdef __cplusplus
}
#endif

#endif
