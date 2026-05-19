#ifndef __BUTTONS_H__
#define __BUTTONS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Onboard tactile buttons on JZ-F407VET6:
 *   S1 → PE10 (10K pull-up R17, press pulls LOW)
 *   S2 → PE11 (10K pull-up R18, press pulls LOW)
 *
 * Behaviour:
 *   S1 press → relay ON  (publishes nothing; state stored to EEPROM)
 *   S2 press → relay OFF
 *
 * Debounce: 4 consecutive matching 100 ms samples = 400 ms stability before
 * action. That's effectively immediate for a human and safely past any
 * mechanical bounce (~5-20 ms typical). */

void buttons_init(void);

/* Sample buttons and act on press edges. Call once every ~100 ms. */
void buttons_poll(void);

#ifdef __cplusplus
}
#endif

#endif
