#ifndef __LED_DISPATCH_H__
#define __LED_DISPATCH_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bitmask of LEDs to update: 0x01=LED1, 0x02=LED2, 0x04=LED3.
 * Mask = 0 means "topic not recognised" (no action). */
typedef struct {
    uint8_t mask;   /* which LEDs to affect */
    uint8_t state;  /* 1 = ON, 0 = OFF */
    uint8_t echo;   /* 1 = echo response should be sent (for stm32/ping) */
} led_dispatch_t;

/* Pure function: parse topic+payload into an action descriptor.
 * No side effects, no globals — perfect for unit testing.
 *
 *   topic       null-terminated MQTT topic string
 *   payload     payload bytes (may be empty)
 *   payload_len length of payload
 *
 * Returns the action to take. Caller invokes HAL_GPIO_WritePin / mqtt_publish. */
led_dispatch_t led_dispatch_parse(const char *topic,
                                  const unsigned char *payload,
                                  size_t payload_len);

#ifdef __cplusplus
}
#endif

#endif
