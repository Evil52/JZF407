#ifndef __OUTPUTS_H__
#define __OUTPUTS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Apply a single output. mask bits: 0x01=LED1, 0x02=LED2, 0x04=LED3.
 * For each set bit, set the LED to `state`. Bits not in mask are unchanged. */
void outputs_apply(uint8_t mask, uint8_t state);

/* Fail-safe: turn EVERY managed output OFF immediately.
 * Called when MQTT connection is lost or during grace period after reconnect. */
void outputs_fail_safe(void);

#ifdef __cplusplus
}
#endif

#endif
